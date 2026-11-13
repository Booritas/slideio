// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/tools/tiledareadownscale.hpp"
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class FileReader;

    /**@brief a Nikon ND2 file: one scene per acquired XY position, in loop order
     * (design 4.6). The slide's raw metadata is JSON with one member per decoded
     * metadata chunk (design 6.5).*/
    class SLIDEIO_ND2_EXPORTS ND2Slide : public CVSlide
    {
    public:
        /// Throws "ND2: <cause> (file: <path>)" when the file is not an ND2 the driver
        /// supports or is damaged beyond reading.
        static std::shared_ptr<ND2Slide> openFile(const std::string& path, ResampleLimits limits = {});
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
    private:
        ND2Slide() = default;
        void init(const ResampleLimits& limits);
    private:
        std::string m_filePath;
        std::shared_ptr<const FileReader> m_file;
        std::vector<std::shared_ptr<CVScene>> m_scenes;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
