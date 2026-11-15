// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/openoptions.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS ImageDriver
    {
    public:
        virtual ~ImageDriver(){}
        virtual std::string getID() const = 0;
        virtual bool canOpenFile(const std::string& filePath) const;
        virtual std::shared_ptr<CVSlide> openFile(const std::string& filePath) = 0;
        /**@brief opens with this driver's own open options, their prefix already stripped.
         *
         * The default takes no options: it rejects any key before touching the file and
         * otherwise calls openFile(filePath). A driver that honours options overrides this
         * and CVScene::getOpenOptions() too, or the converter's per-thread re-open loses
         * them.*/
        virtual std::shared_ptr<CVSlide> openFile(const std::string& filePath, const DriverOptions& options);
        virtual std::string getFileSpecs() const = 0;
    };
}
