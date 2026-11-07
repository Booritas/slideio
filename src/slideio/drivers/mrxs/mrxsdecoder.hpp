// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsmetadata.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace slideio::mrxs
{
    /// Largest decoded image accepted, in bytes of 8-bit RGB (a budget of design section 4).
    constexpr int64_t kMaxDecodedBytes = int64_t(256) << 20;

    /**@brief decodes one encoded pyramid image into top-down 8-bit RGB (CV_8UC3).
     *
     * Before any pixel is allocated: the stream's signature must match the declared
     * format, its header must report exactly width x height, BMP24 must satisfy the
     * documented profile (40-byte DIB, 24 bpp, BI_RGB, bottom-up, no palette), and PNG
     * must carry no alpha. Failures are MRXS corrupt or unsupported errors naming `what`.*/
    SLIDEIO_MRXS_EXPORTS cv::Mat decodeImage(const std::vector<uint8_t>& bytes, ImageFormat format,
                                             int width, int height, const std::string& what);
}
