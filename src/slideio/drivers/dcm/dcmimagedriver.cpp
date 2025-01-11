// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"
#include <filesystem>

#include <dcmdata/dcpixel.h>
#include <dcmdata/dcrledrg.h>    /* for DcmRLEDecoderRegistration */
#include <dcmjpeg/djdecode.h>    /* for dcmjpeg decoders */
#include <dcmjpeg/ddpiimpl.h>    /* for class DicomDirImageImplementation */
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmdata/dccodec.h>

#include "jp2decoderregistration.hpp"
#include "slideio/core/tools/tools.hpp"

using namespace slideio;

static const std::string filePathPattern = "*.dcm";
static const std::string ID("DCM");

DCMImageDriver::DCMImageDriver()
{
    initializeDCMTK();
}

DCMImageDriver::~DCMImageDriver()
{
    clieanUpDCMTK();
}


std::string DCMImageDriver::getID() const
{
	return ID;
}

std::shared_ptr<CVSlide> DCMImageDriver::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath, "DCMImageDriver::openFile");
    std::shared_ptr<CVSlide> ptr(new DCMSlide(filePath));
    return ptr;
}

std::string DCMImageDriver::getFileSpecs() const
{
	return filePathPattern;
}

void DCMImageDriver::initializeDCMTK()
{
    DcmRLEDecoderRegistration::registerCodecs();
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
    Jp2DecoderRegistration::registerCodecs();
}

void DCMImageDriver::clieanUpDCMTK()
{
#ifndef __GNUC__
    DcmRLEDecoderRegistration::cleanup();
    DJDecoderRegistration::cleanup();
    DJLSDecoderRegistration::cleanup();
    Jp2DecoderRegistration::cleanup();
#endif
}

bool DCMImageDriver::canOpenFile(const std::string& filePath) const
{
    bool can = ImageDriver::canOpenFile(filePath);
    if(!can) {
#if defined(WIN32)
        std::filesystem::path fp(Tools::toWstring(filePath));
        std::wstring extension = fp.extension().wstring();
        can = extension.empty();
#else
        std::filesystem::path fp(filePath);
        std::string extension = fp.extension().string();
        can = extension.empty();
#endif
    }
    return can;
}
