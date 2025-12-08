// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "convertersvstools.hpp"

#include "converter.hpp"
#include "converterparameters.hpp"
#include "convertertools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include <filesystem>
#include "slideio/drivers/dcm/jp2codecparameter.hpp"

using namespace slideio;
using namespace slideio::converter;


void ConverterSVSTools::checkSVSRequirements(const CVScenePtr& scene, const ConverterParameters& parameters)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    for (int channel = 1; channel < numChannels; ++channel) {
        if (dt != scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS!";
        }
    }
}

static std::string retrieveDate()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Date = %m/%d/%Y", std::localtime(&time));
    std::string strDate(buffer);
    return strDate;
}

static std::string retrieveTime()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Time = %H/%M/%S", std::localtime(&time));
    std::string strTime(buffer);
    return strTime;
}

std::string ConverterSVSTools::createDescription(const CVScenePtr& scene, const ConverterParameters& parameters)
{
    auto rect = scene->getRect();
	std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(parameters.getContainerParameters());
    std::stringstream buff;
    buff << "SlideIO Library 2.0" << std::endl;
    buff << rect.width << "x" << rect.height;
    buff << "(" << tiffParams->getTileWidth() << "x" << tiffParams->getTileHeight() << ") ";
    if(parameters.getEncoding() == Compression::Jpeg) {
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(parameters.getEncodeParameters());
        buff << "JPEG/RGB " << "Q=" << jpegParams->getQuality();
    }
    else if(parameters.getEncoding() == Compression::Jpeg2000) {
        buff << "J2K";
    }
    buff << std::endl;
    double magn = scene->getMagnification();
    Resolution resolution = scene->getResolution();
    if(resolution.x>0) {
        buff << "|MPP = " << resolution.x * 1.e6;
    }
    if (magn > 0) {
        buff << "|AppMag = " << magn;
    }

    std::string filePath = scene->getFilePath();
    std::filesystem::path path(filePath);
    buff << "|Filename = " << path.stem().string();
    buff << "|" << retrieveDate() << "|" << retrieveTime();

    return buff.str();
}

