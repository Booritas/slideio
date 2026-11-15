// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsbundle.hpp"
#include "slideio/drivers/mrxs/mrxsslide.hpp"
#include "slideio/core/exceptions.hpp"

std::string slideio::MRXSImageDriver::getID() const
{
    return "MRXS";
}

bool slideio::MRXSImageDriver::canOpenFile(const std::string& filePath) const
{
    return mrxs::isEntryName(filePath);
}

std::shared_ptr<slideio::CVSlide> slideio::MRXSImageDriver::openFile(const std::string& filePath)
{
    return openFile(filePath, DriverOptions());
}

std::shared_ptr<slideio::CVSlide> slideio::MRXSImageDriver::openFile(const std::string& filePath,
                                                                     const DriverOptions& options)
{
    bool cropToContent = true;
    for (const auto& option : options) {
        if (option.first != "crop_to_content") {
            RAISE_RUNTIME_ERROR << "driver " << getID() << " has no open option '"
                << optionKeyOf(getID(), option.first) << "'";
        }
        cropToContent = parseBoolOption(optionKeyOf(getID(), option.first), option.second);
    }
    return MRXSSlide::openFile(filePath, cropToContent);
}

std::string slideio::MRXSImageDriver::getFileSpecs() const
{
    return "*.mrxs;Slidedat.ini";
}
