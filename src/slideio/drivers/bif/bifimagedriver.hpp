// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/bif/bif_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    /**@brief driver for Ventana / Roche BIF slides (iScan Coreo, HT and DP 200).
     *
     * Claims `*.bif` by name alone, in any letter case, through the base class's
     * canOpenFile, and never reads the file there: a .bif that is not BIF must reach
     * openFile and be refused with its own error, not fall through to the next driver
     * in AUTO detection.*/
    class SLIDEIO_BIF_EXPORTS BIFImageDriver : public ImageDriver
    {
    public:
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
