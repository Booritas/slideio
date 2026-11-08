// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    /**@brief driver for 3DHISTECH MIRAX (MRXS) bundles.
     *
     * A bundle is an entry file `<name>.mrxs` with a sibling directory `<name>/`
     * holding Slidedat.ini, an index file and data files. It can be opened through
     * the entry file or through Slidedat.ini itself.*/
    class SLIDEIO_MRXS_EXPORTS MRXSImageDriver : public ImageDriver
    {
    public:
        std::string getID() const override;
        /**@brief claims `*.mrxs` and files named Slidedat.ini, in any letter case.
         *
         * Decides by name alone and never reads the file: a recognised but corrupt
         * bundle must reach openFile and report its own error, not fall through to
         * the next driver in AUTO detection.*/
        bool canOpenFile(const std::string& filePath) const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
