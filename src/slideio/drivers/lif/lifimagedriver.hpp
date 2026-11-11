// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/lif/lif_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    /**@brief driver for Leica LIF files (LAS X / LAS AF projects).
     *
     * Claims `*.lif` by name alone, in any letter case, through the base class's
     * canOpenFile, and never reads the file there: a .lif that is not LIF must reach
     * openFile and be refused with its own error (spec D 4.1).*/
    class SLIDEIO_LIF_EXPORTS LIFImageDriver : public ImageDriver
    {
    public:
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
