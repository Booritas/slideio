// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/bif/bif_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief a Ventana BIF slide: one scene per scanned AOI, plus the label, thumbnail
     * and probability images as auxiliary images.
     *
     * The slide's raw metadata is the IFD 0 iScan packet (XML). It can carry barcode
     * attributes, so it is never logged.
     * See software-docs/specs/2026-11-09-bif-driver-design.md section 4.*/
    class SLIDEIO_BIF_EXPORTS BIFSlide : public CVSlide
    {
    public:
        // Throws, naming the file and the cause, on anything that is not a BIF the
        // driver supports.
        static std::shared_ptr<BIFSlide> openFile(const std::string& path);
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& name) const override;
    private:
        BIFSlide() = default;
        void init();
    private:
        std::string m_filePath;
        std::vector<std::shared_ptr<CVScene>> m_scenes;
        std::map<std::string, std::shared_ptr<CVScene>> m_auxImages;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
