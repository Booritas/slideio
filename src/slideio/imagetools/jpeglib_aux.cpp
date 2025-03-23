#include "slideio/base/exceptions.hpp"
#include <cstdint>
#include <stdio.h>
#include <jpeglib.h>
#include <opencv2/core.hpp>

void jpeglibDecode(const uint8_t* jpg_buffer, size_t jpg_size, cv::OutputArray output)
{
    // code derived from: https://gist.github.com/PhirePhly/3080633
    struct jpeg_decompress_struct cinfo {};
    struct jpeg_error_mgr jerr {};

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
        RAISE_RUNTIME_ERROR << "Invalid jpeg stream. JpegLib returns code: " << rc;
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

void jpeglibEncode(const cv::Mat& raster, std::vector<uint8_t>& encodedStream, int quality)
{
    if (!raster.isContinuous()) {
        RAISE_RUNTIME_ERROR << "Expected continuous matrix!";
    }
    if (raster.dims != 2) {
        RAISE_RUNTIME_ERROR << "Expected 2D matrix!";
    }
    if (raster.depth() != CV_8U) {
        RAISE_RUNTIME_ERROR << "Expected 8bit matrix!";
    }
    const int imageWidth = raster.cols;
    const int imageHeight = raster.rows;
    const int numChannels = raster.channels();
    if (numChannels != 1 && numChannels != 3) {
        RAISE_RUNTIME_ERROR << "Only 3 or 1 channel images are supported!";
    }
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride = imageWidth * numChannels;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    cinfo.image_width = imageWidth;
    cinfo.image_height = imageHeight;
    cinfo.input_components = numChannels;
    cinfo.in_color_space = JCS_RGB;
    if (numChannels == 1) {
        cinfo.in_color_space = JCS_GRAYSCALE;
    }
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    size_t length = 0;
    uint8_t* output = nullptr;
    jpeg_mem_dest(&cinfo, &output, &length);
    for(int channel=0; channel<numChannels; ++channel) {
        cinfo.comp_info[channel].h_samp_factor = 1;
        cinfo.comp_info[channel].v_samp_factor = 1;
    }
    jpeg_start_compress(&cinfo, TRUE);
    uint8_t* row(raster.data);
    bool success(true);
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = row;
        if (jpeg_write_scanlines(&cinfo, row_pointer, 1) == 0) {
            success = false;
            break;
        }
        row += row_stride;
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    if (success) {
        encodedStream.resize(length);
        memcpy(encodedStream.data(), output, length);
    }
    else {
        RAISE_RUNTIME_ERROR << "Error during compressing of raster with libjpeg!";
    }
}
