// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svsimagedriver.hpp"

namespace slideio
{
    // Philips files are *.tif and *.tiff, an extension that says nothing: gdal reads
    // plain tiff and the ome-tiff driver reads its own flavour. Only the metadata in the
    // description of the first directory identifies a philips file.
    class SLIDEIO_SVS_EXPORTS PHTIFFImageDriver : public SVSImageDriver
    {
    public:
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
        bool canOpenFile(const std::string& filePath) const override;
    };
}
