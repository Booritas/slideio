// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/rect.hpp"
#include "slideio/core/resolution.hpp"
#include <string>
#include <vector>

namespace slideio
{
    /**@brief Everything a scene built by #makeScene reports about itself.
     *
     * A declaration, not a request. Each field is returned verbatim by the
     * corresponding Scene getter: getRect() reports #rect, it does not crop to
     * it; #channelNames is what getChannelName() hands back, not a selector.
     * The pixels come from the ISceneSource handed to makeScene alongside this.
     */
    struct SceneMetadata
    {
        /**@brief Scene::getName().*/
        std::string name;
        /**@brief Scene::getFilePath(). A computed scene has no file; leave it
         * empty, or name the slide the result was derived from.*/
        std::string reportedFilePath;
        /**@brief Scene::getRect(). Must match the size of level 0.*/
        Rect rect;
        /**@brief One entry per channel. Its size defines getNumChannels().*/
        std::vector<DataType> channelDataTypes;
        /**@brief Empty, or one name per channel.*/
        std::vector<std::string> channelNames;
        /**@brief Empty, or one count per channel. 0 means unknown.*/
        std::vector<int> channelSignificantBits;
        /**@brief Pixel size in meters.*/
        Resolution resolution{0.0, 0.0};
        /**@brief Objective magnification. 0 if not meaningful.*/
        double magnification = 0.0;
        /**@brief How the source's pixels were stored, for reporting only.*/
        Compression compression = Compression::Uncompressed;
        /**@brief 1 for a plane scene.*/
        int numZSlices = 1;
        /**@brief 1 for a scene without time frames.*/
        int numTFrames = 1;
        /**@brief Z slice thickness in meters. 0 if not meaningful.*/
        double zSliceResolution = 0.0;
        /**@brief Seconds between time frames. 0 if not meaningful.*/
        double tFrameResolution = 0.0;
        /**@brief The pyramid, finest first. At least one entry; level 0 has
         * scale 1 and the size of #rect.*/
        std::vector<LevelInfo> levels;
        /**@brief Empty means the scene carries no ICC profile.*/
        ColorProfile colorProfile;
    };
}
