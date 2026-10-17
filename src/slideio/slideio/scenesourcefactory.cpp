// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/slideio/scenesource.hpp"
#include "slideio/slideio/scenemetadata.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/slideio/sourcecvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include <cmath>
#include <sstream>
#include <string>

using namespace slideio;

namespace
{
    // A level dimension for a message: whole numbers without a fraction or an
    // exponent, so a size reads "50000x150" whatever its magnitude.
    std::string formatDimension(double value)
    {
        std::ostringstream out;
        if (value == std::floor(value) && std::fabs(value) < 1.e15) {
            out << static_cast<long long>(value);
        }
        else {
            out.setf(std::ios::fixed);
            out.precision(1);
            out << value;
        }
        return out.str();
    }

    void validate(const std::shared_ptr<ISceneSource>& source, const SceneMetadata& m)
    {
        if (!source) {
            RAISE_RUNTIME_ERROR << "makeScene: source must not be null";
        }
        if (m.channelDataTypes.empty()) {
            RAISE_RUNTIME_ERROR << "makeScene: channelDataTypes must name at least one channel";
        }
        const int numChannels = static_cast<int>(m.channelDataTypes.size());
        for (int channel = 0; channel < numChannels; ++channel) {
            const DataType dt = m.channelDataTypes[channel];
            if (dt == DataType::DT_Unknown || dt == DataType::DT_None) {
                RAISE_RUNTIME_ERROR << "makeScene: channelDataTypes[" << channel
                                    << "] is not a usable data type";
            }
        }
        if (!m.channelNames.empty() &&
            static_cast<int>(m.channelNames.size()) != numChannels) {
            RAISE_RUNTIME_ERROR << "makeScene: channelNames has " << m.channelNames.size()
                                << " entries for " << numChannels << " channels";
        }
        if (!m.channelSignificantBits.empty() &&
            static_cast<int>(m.channelSignificantBits.size()) != numChannels) {
            RAISE_RUNTIME_ERROR << "makeScene: channelSignificantBits has "
                                << m.channelSignificantBits.size()
                                << " entries for " << numChannels << " channels";
        }
        if (m.rect.width <= 0 || m.rect.height <= 0) {
            RAISE_RUNTIME_ERROR << "makeScene: rect must have positive size, got "
                                << m.rect.width << "x" << m.rect.height;
        }
        if (m.levels.empty()) {
            RAISE_RUNTIME_ERROR << "makeScene: levels must describe at least level 0";
        }
        // Written so that NaN fails it: every comparison with NaN is false.
        if (!(std::fabs(m.levels[0].getScale() - 1.0) <= 1.e-6)) {
            RAISE_RUNTIME_ERROR << "makeScene: level 0 must have scale 1, got "
                                << m.levels[0].getScale();
        }
        if (m.levels[0].getSize().width != m.rect.width ||
            m.levels[0].getSize().height != m.rect.height) {
            RAISE_RUNTIME_ERROR << "makeScene: level 0 size "
                                << m.levels[0].getSize().width << "x"
                                << m.levels[0].getSize().height
                                << " differs from rect " << m.rect.width << "x" << m.rect.height;
        }
        for (size_t level = 0; level < m.levels.size(); ++level) {
            const LevelInfo& info = m.levels[level];
            if (info.getSize().width <= 0 || info.getSize().height <= 0) {
                RAISE_RUNTIME_ERROR << "makeScene: level " << level << " has non-positive size";
            }
            if (!std::isfinite(info.getScale())) {
                RAISE_RUNTIME_ERROR << "makeScene: level " << level << " has a non-finite scale";
            }
            if (info.getScale() <= 0.0) {
                RAISE_RUNTIME_ERROR << "makeScene: level " << level << " has non-positive scale";
            }
            // The level read path clamps against getSize() but maps to scene
            // coordinates through getScale(), so the two must describe the
            // same level. The scale is the level's width as a fraction of the
            // scene's (LevelInfo, and how drivers compute it: w_k / W), so:
            //  - the width must be within 1 pixel of W * scale -- rounded
            //    down or up and nothing more. A relative allowance would grow
            //    with the slide: 1% of a 100000-pixel-wide scene is 1000;
            //  - the height, rounded on its own axis (ceil(H / 2^k), say),
            //    also inherits the width's rounding through the scale,
            //    magnified by H / W: 1 + H / W pixels. Holding it to one
            //    pixel rejected a tall scene's pyramid copied verbatim from
            //    the driver that read it.
            const double expectedWidth = m.rect.width * info.getScale();
            const double expectedHeight = m.rect.height * info.getScale();
            const double allowWidth = 1.0;
            const double allowHeight = 1.0 + static_cast<double>(m.rect.height) / m.rect.width;
            if (std::fabs(info.getSize().width - expectedWidth) > allowWidth ||
                std::fabs(info.getSize().height - expectedHeight) > allowHeight) {
                RAISE_RUNTIME_ERROR << "makeScene: level " << level << " size "
                                    << info.getSize().width << "x" << info.getSize().height
                                    << " disagrees with its scale " << info.getScale()
                                    << ": expected " << formatDimension(expectedWidth) << "x"
                                    << formatDimension(expectedHeight)
                                    << ", within " << formatDimension(allowWidth) << "x"
                                    << formatDimension(std::floor(allowHeight)) << " pixels";
            }
            if (info.getTileSize().width < 0 || info.getTileSize().height < 0) {
                RAISE_RUNTIME_ERROR << "makeScene: level " << level << " has a negative tile size";
            }
            if (level > 0 && info.getScale() >= m.levels[level - 1].getScale()) {
                RAISE_RUNTIME_ERROR << "makeScene: level scales must decrease with index; level "
                                    << level << " has scale " << info.getScale()
                                    << " against level " << (level - 1) << "'s "
                                    << m.levels[level - 1].getScale();
            }
        }
        if (m.numZSlices < 1) {
            RAISE_RUNTIME_ERROR << "makeScene: numZSlices must be at least 1, got " << m.numZSlices;
        }
        if (m.numTFrames < 1) {
            RAISE_RUNTIME_ERROR << "makeScene: numTFrames must be at least 1, got " << m.numTFrames;
        }
    }
}

std::shared_ptr<Scene> slideio::makeScene(std::shared_ptr<ISceneSource> source,
                                          const SceneMetadata& metadata)
{
    validate(source, metadata);
    auto adapter = std::make_shared<SourceCVScene>(std::move(source), metadata);
    return SceneInternal::createScene(adapter);
}
