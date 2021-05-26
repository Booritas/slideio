// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"
#include <boost/filesystem.hpp>
#include <dcmdata/dcrledrg.h>    /* for DcmRLEDecoderRegistration */
#include <dcmjpeg/djdecode.h>    /* for dcmjpeg decoders */
#include <dcmjpeg/ddpiimpl.h>    /* for class DicomDirImageImplementation */
#include <dcmtk/dcmjpls/djdecode.h>

using namespace slideio;

static const std::string filePathPattern = "*.dcm";
static const std::string ID("DCM");

DCMImageDriver::DCMImageDriver()
{
    DcmRLEDecoderRegistration::registerCodecs();
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
}

DCMImageDriver::~DCMImageDriver()
{
    DcmRLEDecoderRegistration::cleanup();
    DJDecoderRegistration::cleanup();
    DJLSDecoderRegistration::cleanup();
}


std::string DCMImageDriver::getID() const
{
	return ID;
}

std::shared_ptr<CVSlide> DCMImageDriver::openFile(const std::string& filePath)
{
    namespace fs = boost::filesystem;
    if (!fs::exists(filePath)) {
        throw std::runtime_error(std::string("DCMImageDriver: File does not exist:") + filePath);
    }
    std::shared_ptr<CVSlide> ptr(new DCMSlide(filePath));
    return ptr;
}

std::string DCMImageDriver::getFileSpecs() const
{
	return filePathPattern;
}
