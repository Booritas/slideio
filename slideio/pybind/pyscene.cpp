#include "pyscene.hpp"
#include <pybind11/numpy.h>

namespace py = pybind11;

std::string PyScene::getFilePath() const
{
    return m_Scene->getFilePath();
}

std::string PyScene::getName() const
{
    return m_Scene->getName();
}

std::tuple<int, int, int, int> PyScene::getRect() const
{
    cv::Rect rect = m_Scene->getRect();
    std::tuple<int,int,int,int> wrapper(rect.x, rect.y, rect.width, rect.height);
    return wrapper;
}

int PyScene::getNumChannels() const
{
    return m_Scene->getNumChannels();
}

int PyScene::getNumZSlices() const
{
    return m_Scene->getNumZSlices();
}

int PyScene::getNumTFrames() const
{
    return m_Scene->getNumTFrames();
}

std::string PyScene::getChannelName(int channel) const
{
    return m_Scene->getChannelName(channel);
}

std::tuple<double, double> PyScene::getResolution() const
{
    slideio::Resolution res = m_Scene->getResolution();
    std::tuple<double,double> wrapper(res.x, res.y);
    return wrapper;
}

double PyScene::getZSliceResolution() const
{
    return m_Scene->getZSliceResolution();
}

double PyScene::getTFrameResolution() const
{
    return m_Scene->getTFrameResolution();
}

double PyScene::getMagnification() const
{
    return m_Scene->getMagnification();
}

py::dtype PyScene::getChannelDataType(int channel) const
{
    auto dt = m_Scene->getChannelDataType(channel);
    switch(dt)
    {
        case slideio::DataType::DT_Byte:
            return py::detail::npy_format_descriptor<uint8_t>::dtype();
        case slideio::DataType::DT_Int8:
            return py::detail::npy_format_descriptor<int8_t>::dtype();
        case slideio::DataType::DT_Int16:
            return py::detail::npy_format_descriptor<int16_t>::dtype();
        case slideio::DataType::DT_Int32:
            return py::detail::npy_format_descriptor<int32_t>::dtype();
        case slideio::DataType::DT_Float32:
            return py::detail::npy_format_descriptor<float>::dtype();
        case slideio::DataType::DT_Float64:
            return py::detail::npy_format_descriptor<double>::dtype();
        case slideio::DataType::DT_UInt16:
            return py::detail::npy_format_descriptor<uint16_t>::dtype();
        default:
            throw std::runtime_error("Cannot convert data to numpy: Unsupported data type");
    }
}

pybind11::array PyScene::readBlock(std::tuple<int, int, int, int> rect,
    std::tuple<int, int> size, std::vector<int> channelIndices,
    std::tuple<int,int> sliceRange, std::tuple<int,int> tframeRange) const
{
    const int imageChannels = getNumChannels();
    const cv::Rect imageRect = m_Scene->getRect();
    const int imageWidth = imageRect.width - imageRect.x;
    const int imageHeight = imageRect.height - imageRect.y;
    const int startSlice = std::max(0,std::get<0>(sliceRange));
    const int stopSlice = std::get<1>(sliceRange);
    const int startFrame = std::max(0,std::get<0>(tframeRange));
    const int stopFrame = std::get<1>(tframeRange);
    const int numSlices = std::max(1,stopSlice - startSlice);
    const int numFrames = std::max(1,stopFrame - startFrame);
    const cv::Range zRange(startSlice, startSlice+numSlices);
    const cv::Range tRange(startFrame, startFrame+numFrames);

    if(channelIndices.empty())
    {
        for(int index=0; index<imageChannels; ++index)
        {
            channelIndices.push_back(index);
        }
    }

    if(channelIndices.empty())
    {
        throw std::runtime_error("Unexpected: number of channels is zero");
    }

    const auto dtype = getChannelDataType(channelIndices.front());
    const auto cvType = m_Scene->getChannelDataType(0);
    for(const auto& channel : channelIndices)
    {
        auto channelDataType = m_Scene->getChannelDataType(channel);
        if(channelDataType != cvType)
        {
            throw std::runtime_error("All channels in the block should be of the same type.");
        }
    }

    // source block parameters
    int srcHeight = std::get<3>(rect);
    if(srcHeight<=0)
        srcHeight = imageHeight;
    int srcWidth = std::get<2>(rect);
    if(srcWidth==0)
        srcWidth = imageWidth;
    const int blockChannels = static_cast<int>(channelIndices.size());

    // output block parameters
    int trgWidth = std::get<0>(size);
    if(trgWidth==0)
        trgWidth = srcWidth;
    int trgHeight = std::get<1>(size);
    if(trgHeight==0)
        trgHeight = srcHeight;

    const cv::Rect srcRect(std::get<0>(rect), std::get<1>(rect), srcWidth, srcHeight);
    const cv::Size trgSize( trgWidth, trgHeight);

    py::array numpy_array(dtype, {trgWidth, trgHeight, blockChannels*numSlices*numFrames});
    cv::Mat raster(trgHeight, trgWidth, CV_MAKETYPE(static_cast<int>(cvType), blockChannels), numpy_array.mutable_data());

    if(zRange.start==0 && zRange.end==1 && tRange.start==0 && tRange.end==1)
    {
        m_Scene->readResampledBlockChannels(srcRect, trgSize, channelIndices, raster);
    }
    else
    {
        m_Scene->readResampled4DBlockChannels(srcRect, trgSize, channelIndices, zRange, tRange, raster);
    }

    if(numpy_array.mutable_data()!=raster.data)
    {
        throw std::runtime_error("Unexpected data reallocation");
    }

    return numpy_array;
}
