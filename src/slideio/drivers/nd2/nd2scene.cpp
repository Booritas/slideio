// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2scene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/tools.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    /// Julian day of the Unix epoch.
    constexpr double kUnixEpochJulianDay = 2440587.5;
    constexpr double kSecondsPerDay = 86400.;
}

ND2Scene::ND2Scene(const std::string& filePath, int position, std::shared_ptr<const ImageInfo> info,
                   std::shared_ptr<const FrameSource> frames, Compression compression, ResampleLimits limits)
    : m_filePath(filePath),
      m_driverId("ND2"),
      m_position(position),
      m_info(std::move(info)),
      m_frames(std::move(frames)),
      m_compression(compression),
      m_limits(std::move(limits))
{
    const Position& pos = m_info->positions.at(static_cast<size_t>(m_position));
    m_name = pos.name;
    m_numZ = static_cast<int>(m_info->experiment.count(Axis::Z));
    m_numT = static_cast<int>(m_info->experiment.count(Axis::T));

    const std::vector<ChannelInfo>& channels = m_info->channels;
    for (size_t k = 0; k < channels.size(); ++k) {
        const int channel = static_cast<int>(k);
        setChannelAttribute(channel, "Color", colorToHex(channels[k].color));
        setChannelAttribute(channel, "ND2Channel", static_cast<int64_t>(channels[k].nd2Channel));
        setChannelAttribute(channel, "Component", static_cast<int64_t>(channels[k].component));
        for (const auto& attribute : channels[k].attributes) {
            setChannelAttribute(channel, attribute.first, attribute.second);
        }
    }


    m_rawMetadata = pos.rawJson;
    m_metadataFormat = pos.rawJson.empty() ? MetadataFormat::None : MetadataFormat::JSON;

    const Size size(m_info->attributes.width, m_info->attributes.height);
    m_levels.emplace_back(0, size, 1.0, getMagnification(), size);
}

void ND2Scene::initTimes() const
{
    // Design 6.3 (R30): this scene's own measured step, else the nominal period.
    m_tStep = nd2::timeStepSeconds(m_info->experiment, m_info->frameTimesMs, m_position);

    // Design 6.4. Frame times are ms by frame index; a plane's time is its frame's.
    const Experiment& experiment = m_info->experiment;
    const nd2::FrameTimes& times = m_info->frameTimesMs;
    bool anyPresent = false;
    bool allKnown = true;
    double earliest = std::numeric_limits<double>::infinity();
    for (int t = 0; t < m_numT; ++t) {
        for (int z = 0; z < m_numZ; ++z) {
            const int64_t frame = experiment.frameIndex(t, m_position, z);
            if (!m_frames->hasFrame(frame)) {
                continue;
            }
            anyPresent = true;
            const double ms = times[frame];
            if (std::isnan(ms)) {
                allKnown = false;
            }
            else {
                earliest = std::min(earliest, ms);
            }
        }
    }
    m_hasTimestamps = anyPresent && allKnown;
    m_earliestMs = std::isfinite(earliest) ? earliest : 0.;

    // The acquisition time is dTimeAbsolute plus the scene's earliest frame time,
    // floored to a whole second; a plane's timestamp is its offset from that floored
    // value, so none is negative and acquisition + offset is the frame's time. The
    // whole and fractional seconds are kept apart so that the offsets keep their
    // precision next to a Unix time of ~1.7e9.
    //
    // Without an absolute time (dTimeAbsolute < 1 is unknown, design 6.4),
    // getAcquisitionTime() is 0 and a plane's timestamp is relative to the experiment
    // start: its frame time in seconds, as recorded.
    // With an absolute time but no complete frame times, the acquisition time is still
    // reported, from the earliest known frame time (or dTimeAbsolute alone).
    m_hasAbsoluteTime = m_info->absoluteJulianDay >= 1;
    if (m_hasAbsoluteTime) {
        const double absolute = (m_info->absoluteJulianDay - kUnixEpochJulianDay) * kSecondsPerDay;
        const double whole = std::floor(absolute);
        const double fraction = (absolute - whole) + m_earliestMs / 1000.;
        const double carry = std::floor(fraction);
        m_acquisitionTime = static_cast<int64_t>(whole + carry);
        m_firstOffset = std::max(0., fraction - carry);
    }
}

cv::Rect ND2Scene::getRect() const
{
    return cv::Rect(0, 0, m_info->attributes.width, m_info->attributes.height);
}

int ND2Scene::getNumChannels() const
{
    return m_info->attributes.components;
}

DataType ND2Scene::getChannelDataType(int channel) const
{
    checkChannel(channel);
    return m_info->attributes.dataType();
}

std::string ND2Scene::getChannelName(int channel) const
{
    checkChannel(channel);
    return m_info->channels[static_cast<size_t>(channel)].name;
}

Resolution ND2Scene::getResolution() const
{
    return Resolution(m_info->pixelSizeX, m_info->pixelSizeY);
}

double ND2Scene::getZSliceResolution() const
{
    return m_info->zStep;
}

double ND2Scene::getTFrameResolution() const
{
    std::call_once(m_timesOnce, [this] { initTimes(); });
    return m_tStep;
}

bool ND2Scene::hasPlaneTimestamps() const
{
    std::call_once(m_timesOnce, [this] { initTimes(); });
    return m_hasTimestamps;
}

int64_t ND2Scene::getAcquisitionTime() const
{
    std::call_once(m_timesOnce, [this] { initTimes(); });
    return m_acquisitionTime;
}

int ND2Scene::getChannelSignificantBits(int channel) const
{
    checkChannel(channel);
    const Attributes& attrs = m_info->attributes;
    if (attrs.dataType() == DataType::DT_Float32) {
        return 0;
    }
    return std::min(std::max(attrs.significantBits, 1), attrs.bitsInMemory);
}

double ND2Scene::getPlaneTimestamp(int tFrame, int channel, int zSlice) const
{
    checkChannel(channel);
    if (tFrame < 0 || tFrame >= m_numT || zSlice < 0 || zSlice >= m_numZ) {
        RAISE_RUNTIME_ERROR << "ND2: plane (t " << tFrame << ", z " << zSlice << ") is out of range ["
            << m_numT << " x " << m_numZ << "] in scene \"" << m_name << "\"";
    }
    std::call_once(m_timesOnce, [this] { initTimes(); });
    if (!m_hasTimestamps) {
        return 0.;
    }
    const int64_t frame = m_info->experiment.frameIndex(tFrame, m_position, zSlice);
    if (!m_frames->hasFrame(frame)) {
        return 0.;
    }
    // Every present frame of the scene has a time (m_hasTimestamps), at or after the earliest.
    const double ms = m_info->frameTimesMs[frame];
    if (!m_hasAbsoluteTime) {
        return ms / 1000.;   // relative to the experiment start
    }
    return m_firstOffset + (ms - m_earliestMs) / 1000.;
}

double ND2Scene::getMagnification() const
{
    return m_info->magnification;
}

double ND2Scene::getChannelBackground(int) const
{
    return 0.;
}

void ND2Scene::checkChannel(int channel) const
{
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "ND2: channel index " << channel << " is out of range [0, "
            << getNumChannels() << ") in scene \"" << m_name << "\"";
    }
}

int ND2Scene::cvDepth() const
{
    return CVTools::cvTypeFromDataType(m_info->attributes.dataType());
}

void ND2Scene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex < 0 || zSliceIndex >= m_numZ) {
        RAISE_RUNTIME_ERROR << "ND2: z slice " << zSliceIndex << " is out of range [0, "
            << m_numZ << ") in scene \"" << m_name << "\"";
    }
    if (tFrameIndex < 0 || tFrameIndex >= m_numT) {
        RAISE_RUNTIME_ERROR << "ND2: time frame " << tFrameIndex << " is out of range [0, "
            << m_numT << ") in scene \"" << m_name << "\"";
    }
    std::vector<int> channels = channelIndices;
    if (channels.empty()) {
        for (int channel = 0; channel < getNumChannels(); ++channel) {
            channels.push_back(channel);
        }
    }
    for (int channel : channels) {
        checkChannel(channel);
    }
    const int64_t frame = m_info->experiment.frameIndex(tFrameIndex, m_position, zSliceIndex);
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray part) {
            if (size == rect.size()) {
                cv::Mat raw;
                readRaw(rect, channels, frame, nullptr, raw);
                raw.copyTo(part);
                return;
            }
            // Within the budget, or enlarging along either axis: one read and cv::resize.
            if (!needsTiledDownscale(rect.size(), size, m_limits)) {
                cv::Mat raw, resized;
                readRaw(rect, channels, frame, nullptr, raw);
                Tools::resize(raw, resized, size, cv::INTER_AREA);
                resized.copyTo(part);
                return;
            }
            // One cursor for all the tiles, so a compressed frame is decoded once.
            std::unique_ptr<nd2::FrameCursor> cursor;
            if (m_frames->hasFrame(frame)) {
                cursor = m_frames->openCursor(frame);
            }
            cv::Mat result;
            tiledAreaDownscale(rect, size, cvDepth(), static_cast<int>(channels.size()), m_limits,
                [&](const cv::Rect& tileRect, cv::Mat& tile) {
                    readRaw(tileRect, channels, frame, cursor.get(), tile);
                }, result);
            result.copyTo(part);
        },
        output);
}

void ND2Scene::readRaw(const cv::Rect& rect, const std::vector<int>& channels, int64_t frame,
    nd2::FrameCursor* cursor, cv::Mat& out) const
{
    if (m_limits.stats) {
        m_limits.stats->noteSource(static_cast<int64_t>(rect.width) * rect.height);
    }
    const int n = static_cast<int>(channels.size());
    const int type = CV_MAKETYPE(cvDepth(), n);
    if (!m_frames->hasFrame(frame)) {
        // An absent frame reads as background (design 4.5), which is 0.
        out.create(rect.height, rect.width, type);
        out.setTo(cv::Scalar::all(0));
        return;
    }
    cv::Mat all;
    if (cursor) {
        cursor->readRows(rect, all);
    }
    else {
        m_frames->readRows(frame, rect, all);
    }
    bool identity = n == all.channels();
    for (int k = 0; identity && k < n; ++k) {
        identity = channels[k] == k;
    }
    if (identity) {
        out = all;
        return;
    }
    out.create(rect.height, rect.width, type);
    std::vector<int> fromTo;
    fromTo.reserve(2 * static_cast<size_t>(n));
    for (int k = 0; k < n; ++k) {
        fromTo.push_back(channels[k]);
        fromTo.push_back(k);
    }
    cv::mixChannels(&all, 1, &out, 1, fromTo.data(), static_cast<size_t>(n));
}
