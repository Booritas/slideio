// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagetools/imagetools.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#if !defined(WIN32)
#define INITGUID
#endif
#include <jxrcodec/jxrcodec.hpp>
#include <opencv2/imgproc.hpp>
#include <jpeglib.h>


static int getCvType(jpegxr_image_info& info)
{
    int type = -1;
    if (info.sample_type == jpegxr_sample_type::Uint)
    {
        switch (info.sample_size)
        {
        case 1:
            type = CV_8U;
            break;
        case 2:
            type = CV_16U;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Int)
    {
        switch (info.sample_size)
        {
        case 2:
            type = CV_16S;
            break;
        case 4:
            type = CV_32S;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Float)
    {
        switch (info.sample_size)
        {
        case 2:
            type = CV_16F;
            break;
        case 4:
            type = CV_32F;
            break;
        }
    }
    if (type < 0)
        throw std::runtime_error("Unsuported type of jpegxr compression");

    return type;
}

void slideio::ImageTools::readJxrImage(const std::string& path, cv::OutputArray output)
{
    namespace fs = boost::filesystem;
    if (!fs::exists(path))
    {
        throw std::runtime_error(
            (boost::format("File %1% does not exist") % path).str()
        );
    }
    FILE* input_file = fopen(path.c_str(), "rb");
    if (!input_file)
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
    catch (std::exception& ex)
    {
        if (input_file)
            fclose(input_file);
        throw ex;
    }
}


void slideio::ImageTools::decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output)
{
    jpegxr_image_info info;
    jpegxr_get_image_info((uint8_t*)data, (uint32_t)dataBlockSize, info);
    int type = getCvType(info);
    output.create(info.height, info.width,CV_MAKETYPE(type, info.channels));
    cv::Mat mat = output.getMat();
    mat.setTo(cv::Scalar(0));
    uint8_t* outputBuff = mat.data;
    uint32_t ouputBuffSize = (int)(mat.total() * mat.elemSize());
    jpegxr_decompress((uint8_t*)data, (uint32_t)dataBlockSize, outputBuff, ouputBuffSize);
}

void slideio::ImageTools::decodeJpegStream(const uint8_t* jpg_buffer, size_t jpg_size, cv::OutputArray output)
{
    // code derived from: https://gist.github.com/PhirePhly/3080633
    struct jpeg_decompress_struct cinfo{};
    struct jpeg_error_mgr jerr{};

    cinfo.err = jpeg_std_error(&jerr);
    // Allocate a new decompress struct, with the default error handler.
    // The default error handler will exit() on pretty much any issue,
    // so it's likely you'll want to replace it or supplement it with
    // your own.
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpg_buffer, static_cast<unsigned long>(jpg_size));
    // Have the decompressor scan the jpeg header. This won't populate
    // the cinfo struct output fields, but will indicate if the
    // jpeg is valid.
    auto rc = jpeg_read_header(&cinfo, TRUE);

    if (rc != 1) {
        jpeg_destroy_decompress(&cinfo);
        throw std::runtime_error(
            (boost::format("decodeJpegStream: Invalid jpeg stream. JpegLib returns code: %1%")
                % rc).str());
    }

    // By calling jpeg_start_decompress, you populate cinfo
    // and can then allocate your output bitmap buffers for
    // each scanline.
    jpeg_start_decompress(&cinfo);

    const JDIMENSION width = cinfo.output_width;
    const JDIMENSION height = cinfo.output_height;
    const int channels = cinfo.output_components;

    const size_t bmpSize = width * height * channels;
    
    output.create(height, width, CV_MAKETYPE(CV_8U, channels));
    cv::Mat mat = output.getMat();

    // The row_stride is the total number of bytes it takes to store an
    // entire scanline (row). 
    const unsigned int rowStride = width * channels;

    // Now that you have the decompressor entirely configured, it's time
    // to read out all of the scanlines of the jpeg.
    //
    // By default, scanlines will come out in RGBRGBRGB...  order, 
    // but this can be changed by setting cinfo.out_color_space
    //
    // jpeg_read_scanlines takes an array of buffers, one for each scanline.
    // Even if you give it a complete set of buffers for the whole image,
    // it will only ever decompress a few lines at a time. For best 
    // performance, you should pass it an array with cinfo.rec_outbuf_height
    // scanline buffers. rec_outbuf_height is typically 1, 2, or 4, and 
    // at the default high quality decompression setting is always 1.
    while (cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char* bufferArray[1];
        bufferArray[0] = mat.data +
            (cinfo.output_scanline) * rowStride;

        jpeg_read_scanlines(&cinfo, bufferArray, 1);
    }
    // Once done reading *all* scanlines, release all internal buffers,
    // etc by calling jpeg_finish_decompress. This lets you go back and
    // reuse the same cinfo object with the same settings, if you
    // want to decompress several jpegs in a row.
    //
    // If you didn't read all the scanlines, but want to stop early,
    // you instead need to call jpeg_abort_decompress(&cinfo)
    jpeg_finish_decompress(&cinfo);

    // At this point, optionally go back and either load a new jpg into
    // the jpg_buffer, or define a new jpeg_mem_src, and then start 
    // another decompress operation.

    // Once you're really really done, destroy the object to free everything
    jpeg_destroy_decompress(&cinfo);
}
