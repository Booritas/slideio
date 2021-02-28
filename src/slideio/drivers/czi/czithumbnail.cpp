// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/czi/czithumbnail.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

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
        std::string error = ex.what();
        ret = false;
    }
    return ret;
}

void CZIThumbnail::setAttachmentData(CZISlide* slide, int64_t position, int64_t size, const std::string& name)
{
    m_slide = slide;
    m_dataPos = position;
    m_dataSize = size;
    m_sceneName = name;
}

void CZIThumbnail::readImage(cv::OutputArray output)
{
    std::vector<uint8_t> buffer;
    m_slide->readBlock(m_dataPos, m_dataSize, buffer);
    slideio::ImageTools::decodeJpegStream(buffer.data(), buffer.size(), output);
}
