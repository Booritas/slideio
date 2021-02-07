// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/czi/czithumbnail.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;
#include "gdal/gdal.h"

bool CZIThumbnail::init()
{
    bool ret = true;
    try {
        std::vector<uint8_t> buffer;
        m_slide->readBlock(m_dataPos, m_dataSize, buffer);
        cv::Mat jpegImage;
        slideio::ImageTools::decodeJpegStream(buffer.data(), buffer.size(), jpegImage);
        m_filePath = m_slide->getFilePath();
        m_sceneRect = { 0,0,jpegImage.size().width,jpegImage.size().height};
        m_numChannel = jpegImage.channels();
        m_compression = Compression::Jpeg;
        m_channelDataType = DataType::DT_Byte;
    }
    catch (std::exception& ex){
        ret = false;
    }
    return ret;
}

void CZIThumbnail::readImage(cv::OutputArray output)
{
}
