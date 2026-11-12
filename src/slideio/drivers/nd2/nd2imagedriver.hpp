// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    /**@brief driver for Nikon NIS-Elements ND2 files.
     *
     * Claims `*.nd2` by name alone, in any letter case, through the base class's
     * canOpenFile, and never reads the file there: an .nd2 that is not ND2 must reach
     * openFile and be refused with its own error (design 4.1).*/
    class SLIDEIO_ND2_EXPORTS ND2ImageDriver : public ImageDriver
    {
    public:
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
