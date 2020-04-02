// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#if !defined(WIN32)
#define INITGUID
#endif
#include <jxrcodec/jxrcodec.hpp>
#include <opencv2/imgproc.hpp>


static int getCvType(jpegxr_image_info& info)
{
    int type = -1;
    if(info.sample_type== jpegxr_sample_type::Uint)
    {
        switch(info.sample_size)
        {
        case 1:
            type = CV_8U;
            break;
        case 2:
            type = CV_16U;
            break;
        }
    }
    else if(info.sample_type== jpegxr_sample_type::Int)
    {
        switch(info.sample_size)
        {
        case 2:
            type = CV_16S;
            break;
        case 4:
            type = CV_32S;
            break;
        }
    }
    else if(info.sample_type== jpegxr_sample_type::Float)
    {
        switch(info.sample_size)
        {
        case 2:
            type = CV_16F;
            break;
        case 4:
            type = CV_32F;
            break;
        }
    }
    if(type<0)
        throw std::runtime_error("Unsuported type of jpegxr compression");

    return type;
}

void slideio::ImageTools::readJxrImage(const std::string& path, cv::OutputArray output)
{
    namespace fs = boost::filesystem;
    if(!fs::exists(path))
    {
        throw std::runtime_error(
            (boost::format("File %1% does not exist") % path).str()
        );
    }
    FILE* input_file = fopen(path.c_str(), "rb");
    if(!input_file)
    {
        throw std::runtime_error(
            (boost::format("Cannot open file %1%") % path).str()
        );
    }
    try
    {
        jpegxr_image_info info;
        jpegxr_get_image_info(input_file, info);
        int type = getCvType(info);
        output.create(info.height, info.width,CV_MAKETYPE(type, info.channels));
        cv::Mat mat = output.getMat();
        uint8_t* outputBuff = mat.data;
        uint32_t ouputBuffSize = (int)(mat.total() * mat.elemSize());
        jpegxr_decompress(input_file, outputBuff, ouputBuffSize);
        fclose(input_file);
    }
    catch(std::exception& ex)
    {
        if(input_file)
            fclose(input_file);
        throw ex;
    }
}


void slideio::ImageTools::decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output)
{
    jpegxr_image_info info;
    jpegxr_get_image_info((uint8_t*)data, dataBlockSize, info);
    int type = getCvType(info);
    output.create(info.height, info.width,CV_MAKETYPE(type, info.channels));
    cv::Mat mat = output.getMat();
    uint8_t* outputBuff = mat.data;
    uint32_t ouputBuffSize = (int)(mat.total() * mat.elemSize());
    jpegxr_decompress((uint8_t*)data, (uint32_t)dataBlockSize, outputBuff, ouputBuffSize);
}
