// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/czi/czi_api_def.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/cvslide.hpp"
#include <opencv2/core.hpp>
#include <string>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    class SLIDEIO_CZI_EXPORTS CZIImageDriver: public slideio::ImageDriver
    {
    public:
        CZIImageDriver();
        ~CZIImageDriver();
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;

    private:
        static std::string filePathPattern;
    };
} // namespace slideio

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
