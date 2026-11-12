// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/core/exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

using namespace slideio;
using namespace slideio::lif;

namespace
{
    constexpr int kDimX = 1, kDimY = 2, kDimZ = 3, kDimT = 4, kDimM = 10;

    // Builds an Unsupported from stream-style arguments.
    class Reason
    {
    public:
        template <typename T>
        Reason& operator<<(const T& v)
        {
            m_s << v;
            return *this;
        }
        [[noreturn]] void raise() const { throw Unsupported(m_s.str()); }
    private:
        std::ostringstream m_s;
    };

    const char* dimensionName(int id)
    {
        // Attributed meanings (FD 13.3), not validated on samples.
        switch (id) {
        case 5: return "emission wavelength";
        case 6: return "rotation";
        case 7: return "XT slices";
        case 8: return "T slices";
        case 9: return "excitation wavelength";
        case 11: return "loop";
        default: return "unknown axis";
        }
    }

    uint64_t mulChecked(uint64_t a, uint64_t b)
    {
        if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
            (Reason() << "byte offsets overflow 64 bits").raise();
        }
        return a * b;
    }

    uint64_t addChecked(uint64_t a, uint64_t b)
    {
        if (a > std::numeric_limits<uint64_t>::max() - b) {
            (Reason() << "byte offsets overflow 64 bits").raise();
        }
        return a + b;
    }

    int toInt(int64_t count, const char* what)
    {
        if (count > std::numeric_limits<int>::max()) {
            (Reason() << what << " has " << count << " elements, more than supported").raise();
        }
        return static_cast<int>(count);
    }

    // |Length| / (N - 1) for a length axis in metres; 0 when unknown.
    double spacingMetres(const Dimension* d)
    {
        if (d == nullptr || d->count <= 1 || d->unit != "m") {
            return 0;
        }
        return std::fabs(d->length) / static_cast<double>(d->count - 1);
    }

    // Sample type and storage bytes from DataType/Resolution (D 5.2), or
    // DT_Unknown when the pair is not one the table names.
    std::pair<DataType, int> typeFromDescription(int dataType, int resolution)
    {
        if (dataType == 0) {
            if (resolution >= 1 && resolution <= 8) return {DataType::DT_Byte, 1};
            if (resolution >= 9 && resolution <= 16) return {DataType::DT_UInt16, 2};
            if (resolution >= 17 && resolution <= 32) return {DataType::DT_UInt32, 4};
            if (resolution >= 33 && resolution <= 64) return {DataType::DT_UInt64, 8};
        }
        else if (dataType == 1) {
            if (resolution >= 9 && resolution <= 16) return {DataType::DT_Float16, 2};
            if (resolution >= 17 && resolution <= 32) return {DataType::DT_Float32, 4};
            if (resolution >= 33 && resolution <= 64) return {DataType::DT_Float64, 8};
        }
        return {DataType::DT_Unknown, 0};
    }

    // Bio-Formats' X-stride rule, the fallback when no channel has a DataType.
    std::pair<DataType, int> typeFromBytes(uint64_t bytes)
    {
        switch (bytes) {
        case 1: return {DataType::DT_Byte, 1};
        case 2: return {DataType::DT_UInt16, 2};
        case 4: return {DataType::DT_Float32, 4};
        case 8: return {DataType::DT_Float64, 8};
        default: return {DataType::DT_Unknown, 0};
        }
    }

    // True when the three increments are 0, b, 2b in some order.
    bool isSampleTriple(const std::vector<uint64_t>& inc, uint64_t b)
    {
        if (inc.size() != 3) {
            return false;
        }
        std::vector<uint64_t> s(inc);
        std::sort(s.begin(), s.end());
        return s[0] == 0 && s[1] == b && s[2] == 2 * b;
    }
}

Layout Layout::build(const ImageDescription& image, uint64_t payloadLength)
{
    Layout l;
    const auto& dims = image.dimensions;

    // ---- axis model (D 5.1)
    if (dims.empty()) {
        (Reason() << "the image has no dimensions").raise();
    }
    if (dims.front().dimId != kDimX) {
        (Reason() << "the first dimension is DimID " << dims.front().dimId << ", not X (1)").raise();
    }
    std::set<int> seen;
    const Dimension* x = &dims.front();
    const Dimension* y = nullptr;
    const Dimension* z = nullptr;
    const Dimension* t = nullptr;
    const Dimension* m = nullptr;
    for (const Dimension& d : dims) {
        if (!seen.insert(d.dimId).second) {
            (Reason() << "dimension " << d.dimId << " is repeated").raise();
        }
        if (d.count < 1) {
            (Reason() << "dimension " << d.dimId << " has " << d.count << " elements").raise();
        }
        switch (d.dimId) {
        case kDimX: break;
        case kDimY: y = &d; break;
        case kDimZ: z = &d; break;
        case kDimT: t = &d; break;
        case kDimM: m = &d; break;
        default:
            if (d.count > 1) {
                (Reason() << "dimension " << d.dimId << " (" << dimensionName(d.dimId) << ") has "
                    << d.count << " elements; extra axes are not supported").raise();
            }
            break;
        }
    }
    // With no Y, a Z or T directly after X is the display Y (XZ / XT scan).
    enum class YRole { Y, Z, T, None } yRole = YRole::Y;
    if (y == nullptr) {
        yRole = YRole::None;
        if (dims.size() > 1 && dims[1].dimId == kDimZ) {
            y = z;
            z = nullptr;
            yRole = YRole::Z;
        }
        else if (dims.size() > 1 && dims[1].dimId == kDimT) {
            y = t;
            t = nullptr;
            yRole = YRole::T;
        }
    }

    // A negative BytesInc is malformed on an axis that is stepped along: X, the
    // display Y, and any axis of more than one element. An axis of one element is
    // only ever at index 0, so its stride is never used: it is taken as 0.
    for (const Dimension& d : dims) {
        if (d.bytesInc < 0 && (&d == x || &d == y || d.count > 1)) {
            (Reason() << "dimension " << d.dimId << " has a negative BytesInc " << d.bytesInc).raise();
        }
    }
    const auto stride = [](const Dimension* d) {
        return d && d->bytesInc > 0 ? static_cast<uint64_t>(d->bytesInc) : uint64_t(0);
    };

    l.m_width = toInt(x->count, "X");
    l.m_height = y ? toInt(y->count, "Y") : 1;
    l.m_numZ = z ? toInt(z->count, "Z") : 1;
    l.m_numT = t ? toInt(t->count, "T") : 1;
    l.m_numFields = m ? toInt(m->count, "the tile axis") : 1;
    l.m_xStride = stride(x);
    l.m_zStride = stride(z);
    l.m_tStride = stride(t);
    l.m_mStride = stride(m);

    // ---- sample type (D 5.2)
    const auto& chans = image.channels;
    if (chans.empty()) {
        (Reason() << "the image has no channels").raise();
    }
    for (const Channel& c : chans) {
        if (c.bytesInc < 0) {
            (Reason() << "a channel has a negative BytesInc " << c.bytesInc).raise();
        }
        l.m_channelInc.push_back(static_cast<uint64_t>(c.bytesInc));
    }
    const size_t typed = std::count_if(chans.begin(), chans.end(),
                                       [](const Channel& c) { return c.dataType >= 0; });
    std::pair<DataType, int> type{DataType::DT_Unknown, 0};
    if (typed == chans.size()) {
        for (const Channel& c : chans) {
            const auto ct = typeFromDescription(c.dataType, c.resolution);
            if (ct.first == DataType::DT_Unknown) {
                (Reason() << "DataType " << c.dataType << " with Resolution " << c.resolution
                    << " is not a supported sample type").raise();
            }
            if (type.first != DataType::DT_Unknown && ct != type) {
                (Reason() << "channels have different sample types").raise();
            }
            type = ct;
        }
    }
    else if (typed == 0) {
        uint64_t bytes = l.m_xStride;
        if (bytes % 3 == 0 && isSampleTriple(l.m_channelInc, bytes / 3)) {
            bytes /= 3;
        }
        type = typeFromBytes(bytes);
        if (type.first == DataType::DT_Unknown) {
            (Reason() << "no channel has a DataType and X BytesInc " << l.m_xStride
                << " names no sample type").raise();
        }
    }
    else {
        (Reason() << "DataType is present on some channels only").raise();
    }
    l.m_dataType = type.first;
    l.m_sampleBytes = type.second;
    const uint64_t b = static_cast<uint64_t>(l.m_sampleBytes);
    // Significant bits cannot exceed the storage width: a Resolution larger than the
    // sample (possible when the type came from X BytesInc) is clamped to it.
    const int storageBits = 8 * l.m_sampleBytes;
    for (const Channel& c : chans) {
        l.m_bits.push_back(c.resolution > 0 ? std::min(c.resolution, storageBits) : storageBits);
    }

    // ---- interleaved colour (D 5.3) or scalar channels (D 5.4)
    if (l.m_xStride == b) {
        l.m_interleaved = false;
        l.m_numChannels = toInt(static_cast<int64_t>(chans.size()), "the channel axis");
        std::vector<uint64_t> sorted(l.m_channelInc);
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
            (Reason() << "two channels have the same BytesInc").raise();
        }
        l.m_channelBase = sorted.front();
        if (sorted.size() > 1) {
            l.m_channelSpacing = sorted[1] - sorted[0];
            for (size_t i = 2; i < sorted.size(); ++i) {
                if (sorted[i] - sorted[i - 1] != l.m_channelSpacing) {
                    (Reason() << "channel BytesInc values are not evenly spaced (" << sorted[i - 2]
                        << ", " << sorted[i - 1] << ", " << sorted[i] << ")").raise();
                }
            }
        }
    }
    else if (l.m_xStride > b && l.m_xStride % b == 0) {
        const uint64_t samples = l.m_xStride / b;
        if (samples != 3 || chans.size() != 3 || !isSampleTriple(l.m_channelInc, b)) {
            (Reason() << "X BytesInc " << l.m_xStride << " interleaves " << samples << " samples of "
                << b << " bytes over " << chans.size()
                << " channels; only three-channel RGB at BytesInc 0, b, 2b is supported").raise();
        }
        l.m_interleaved = true;
        l.m_numChannels = 3;
        bool tagged = true;
        std::array<int, 3> byTag{{-1, -1, -1}};
        for (int i = 0; i < 3; ++i) {
            const int tag = chans[i].channelTag;
            if (tag < 1 || tag > 3 || byTag[tag - 1] >= 0) {
                tagged = false;
                break;
            }
            byTag[tag - 1] = i;
        }
        if (tagged) {
            l.m_rgb = byTag;
        }
        else {
            // Bio-Formats: untagged interleaved colour is stored B, G, R.
            for (int i = 0; i < 3; ++i) {
                l.m_rgb[2 - l.m_channelInc[i] / b] = i;
            }
        }
    }
    else {
        (Reason() << "X BytesInc " << l.m_xStride << " does not match the " << b
            << "-byte sample type").raise();
    }

    // ---- rows
    const uint64_t rowBytes = mulChecked(static_cast<uint64_t>(l.m_width), l.m_xStride);
    l.m_yStride = y ? stride(y) : rowBytes;
    if (l.m_height > 1 && l.m_yStride < rowBytes) {
        (Reason() << "row stride " << l.m_yStride << " is shorter than a row of " << rowBytes
            << " bytes").raise();
    }

    // ---- no aliasing, and bounds (D 5.4)
    std::vector<StackAxis> axes;
    if (!l.m_interleaved && chans.size() > 1) {
        axes.push_back({Axis::C, chans.size(), l.m_channelSpacing});
    }
    if (l.m_numZ > 1) axes.push_back({Axis::Z, static_cast<uint64_t>(l.m_numZ), l.m_zStride});
    if (l.m_numT > 1) axes.push_back({Axis::T, static_cast<uint64_t>(l.m_numT), l.m_tStride});
    if (l.m_numFields > 1) axes.push_back({Axis::M, static_cast<uint64_t>(l.m_numFields), l.m_mStride});
    std::stable_sort(axes.begin(), axes.end(),
                     [](const StackAxis& p, const StackAxis& q) { return p.stride < q.stride; });
    static const char* const axisNames[] = {"channel", "Z", "T", "tile"};
    uint64_t extent = addChecked(mulChecked(static_cast<uint64_t>(l.m_height - 1), l.m_yStride), rowBytes);
    for (const StackAxis& a : axes) {
        if (a.stride < extent) {
            (Reason() << "planes overlap: the " << axisNames[static_cast<int>(a.axis)] << " stride "
                << a.stride << " is smaller than the " << extent << " bytes nested inside it").raise();
        }
        extent = addChecked(mulChecked(a.stride, a.count - 1), extent);
    }
    const uint64_t needed = addChecked(l.m_channelBase, extent);
    if (needed > payloadLength) {
        (Reason() << "the image needs " << needed << " bytes but its payload holds "
            << payloadLength).raise();
    }
    l.m_storageOrder = std::move(axes);

    // ---- calibration (D 6.2)
    l.m_calibration.x = spacingMetres(x);
    l.m_calibration.y = yRole == YRole::T ? 0 : spacingMetres(y);
    l.m_calibration.z = spacingMetres(z);
    double tSpacing = 0;
    if (t && t->count > 1) {
        const double perStep = std::fabs(t->length) / static_cast<double>(t->count - 1);
        if (t->unit == "s") tSpacing = perStep;
        else if (t->unit == "Ks") tSpacing = perStep * 1000;
    }
    if (tSpacing > 0) {
        l.m_calibration.t = tSpacing;
    }
    else if (yRole != YRole::T) {
        l.m_calibration.t = image.delayTimeSeconds;
    }
    return l;
}

void Layout::checkIndices(int channel, int z, int t, int field) const
{
    if (channel < 0 || channel >= m_numChannels || z < 0 || z >= m_numZ ||
        t < 0 || t >= m_numT || field < 0 || field >= m_numFields) {
        RAISE_RUNTIME_ERROR << "LIF: plane (c=" << channel << ", z=" << z << ", t=" << t
            << ", m=" << field << ") is outside the image (" << m_numChannels << ", " << m_numZ
            << ", " << m_numT << ", " << m_numFields << ")";
    }
}

int Layout::storedSample(int channel) const
{
    if (channel < 0 || channel >= m_numChannels) {
        RAISE_RUNTIME_ERROR << "LIF: channel " << channel << " is outside the image (" << m_numChannels << ")";
    }
    return m_interleaved ? m_rgb[channel] : channel;
}

int Layout::significantBits(int channel) const
{
    return m_bits[storedSample(channel)];
}

uint64_t Layout::planeOffset(int channel, int z, int t, int field) const
{
    checkIndices(channel, z, t, field);
    // Validated by build: every term and the sum are within the payload.
    return m_channelInc[storedSample(channel)]
        + static_cast<uint64_t>(z) * m_zStride
        + static_cast<uint64_t>(t) * m_tStride
        + static_cast<uint64_t>(field) * m_mStride;
}

int64_t Layout::storageIndex(int channel, int z, int t, int field) const
{
    checkIndices(channel, z, t, field);
    int64_t index = 0;
    int64_t scale = 1;
    for (const StackAxis& a : m_storageOrder) {
        int64_t i = 0;
        switch (a.axis) {
        case Axis::C:
            i = static_cast<int64_t>((m_channelInc[channel] - m_channelBase) / m_channelSpacing);
            break;
        case Axis::Z: i = z; break;
        case Axis::T: i = t; break;
        case Axis::M: i = field; break;
        }
        index += i * scale;
        scale *= static_cast<int64_t>(a.count);
    }
    return index;
}
