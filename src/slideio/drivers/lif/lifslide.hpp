// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/lif/lif_api_def.hpp"
#include "slideio/drivers/lif/lifscene.hpp"
#include "slideio/core/cvslide.hpp"
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

    /**@brief a Leica LIF file: one scene per supported image per tile field, in
     * document order (spec D 4.5).
     *
     * Images the driver cannot read correctly are left out of the scene list, logged
     * and listed by skippedImages(); the open fails only when no image is left. The
     * slide's raw metadata is the file's whole XML description.*/
    class SLIDEIO_LIF_EXPORTS LIFSlide : public CVSlide
    {
    public:
        // Throws, naming the file and the cause, when the file is not a LIF or holds
        // no image the driver supports.
        static std::shared_ptr<LIFSlide> openFile(const std::string& path, lif::ReadLimits limits = {});
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        /// One entry per skipped image: "<path>: <reason>".
        const std::vector<std::string>& skippedImages() const { return m_skipped; }
    private:
        LIFSlide() = default;
        void init(const lif::ReadLimits& limits);
    private:
        std::string m_filePath;
        std::shared_ptr<const FileReader> m_file;
        std::vector<std::shared_ptr<CVScene>> m_scenes;
        std::vector<std::string> m_skipped;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
