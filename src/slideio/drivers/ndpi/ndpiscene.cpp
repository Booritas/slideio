// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi//ndpiscene.hpp"

#include "ndpifile.hpp"

using namespace slideio;

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1)
{
}

void NDPIScene::init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;

}

cv::Rect NDPIScene::getRect() const
{
    if (!m_pfile) 
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if(m_startDir<0 || m_startDir>=directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const NDPITiffDirectory& dir = directories[m_startDir];
    cv::Rect rect(0,0, dir.width, dir.height);
    return rect;
}

int NDPIScene::getNumChannels() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.channels;
}



// const TiffDirectory& NDPIScene::findZoomDirectory(double zoom) const
// {
//     const cv::Rect sceneRect = getRect();
//     const double sceneWidth = static_cast<double>(sceneRect.width);
//     const auto& directories = m_directories;
//     int index = Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index){
//         return directories[index].width/sceneWidth;
//     });
//     return m_directories[index];
// }


std::string NDPIScene::getFilePath() const
{
    if (!m_pfile)
    {
        throw std::runtime_error(std::string("NDPIScene: Invalid file pointer"));
    }
    return m_pfile->getFilePath();
}

slideio::DataType NDPIScene::getChannelDataType(int channel) const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.dataType;
}

Resolution NDPIScene::getResolution() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.res;
}

double NDPIScene::getMagnification() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.magnification;
}

Compression NDPIScene::getCompression() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.slideioCompression;
}

