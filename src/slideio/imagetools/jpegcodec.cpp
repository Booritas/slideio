#include <stdio.h>
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.#include <jpeglib.h>
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/jpeglib_aux.hpp"


void slideio::ImageTools::decodeJpegStream(const uint8_t* jpg_buffer, size_t jpg_size, cv::OutputArray output)
{
    try {
        jpeglibDecode(jpg_buffer, jpg_size, output);
    }
    catch(std::runtime_error& er) {
        RAISE_RUNTIME_ERROR << "Error decoding jpeg stream: " << er.what();
    }
}


void slideio::ImageTools::encodeJpeg(const cv::Mat& raster, std::vector<uint8_t>& encodedStream, const JpegEncodeParameters& params)
{
    try {
        jpeglibEncode(raster, encodedStream, params.getQuality());
    }
    catch (std::runtime_error& er) {
        RAISE_RUNTIME_ERROR << "Error encoding jpeg stream: " << er.what();
    }
}
