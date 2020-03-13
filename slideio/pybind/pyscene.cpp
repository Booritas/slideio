#include "pyscene.hpp"
#include <pybind11/numpy.h>

namespace py = pybind11;

std::string PyScene::getFilePath() const
{
    return m_scene->getFilePath();
}

std::string PyScene::getName() const
{
    return m_scene->getName();
}

std::tuple<int, int, int, int> PyScene::getRect() const
{
    return m_scene->getRect();
}

int PyScene::getNumChannels() const
{
    return m_scene->getNumChannels();
}

int PyScene::getNumZSlices() const
{
    return m_scene->getNumZSlices();
}

int PyScene::getNumTFrames() const
{
    return m_scene->getNumTFrames();
}

std::string PyScene::getChannelName(int channel) const
{
    return m_scene->getChannelName(channel);
}

std::tuple<double, double> PyScene::getResolution() const
{
    return m_scene->getResolution();
}

double PyScene::getZSliceResolution() const
{
    return m_scene->getZSliceResolution();
}

double PyScene::getTFrameResolution() const
{
    return m_scene->getTFrameResolution();
}

double PyScene::getMagnification() const
{
    return m_scene->getMagnification();
}

py::dtype PyScene::getChannelDataType(int channel) const
{
    auto dt = m_scene->getChannelDataType(channel);
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
    const std::tuple<int,int,int,int> imageRect = m_scene->getRect();
    const int imageWidth = std::get<2>(imageRect);
    const int imageHeight = std::get<3>(imageRect);

    const int startSlice = std::max(0,std::get<0>(sliceRange));
    const int stopSlice = std::get<1>(sliceRange);
    const int startFrame = std::max(0,std::get<0>(tframeRange));
    const int stopFrame = std::get<1>(tframeRange);
    const int numSlices = std::max(1,stopSlice - startSlice);
    const int numFrames = std::max(1,stopFrame - startFrame);

    const int refChannel = channelIndices.empty()?0:channelIndices[0];
    const int numChannels = channelIndices.empty()?imageChannels:static_cast<int>(channelIndices.size());

    const py::dtype dtype = getChannelDataType(refChannel);

    // source block parameters
    int srcHeight = std::get<3>(rect);
    if(srcHeight<=0)
        srcHeight = imageHeight;
    int srcWidth = std::get<2>(rect);
    if(srcWidth==0)
        srcWidth = imageWidth;

    // output block parameters
    int trgWidth = std::get<0>(size);
    if(trgWidth==0)
        trgWidth = srcWidth;
    int trgHeight = std::get<1>(size);
    if(trgHeight==0)
        trgHeight = srcHeight;

    std::tuple<int,int> blockSize(trgWidth, trgHeight);
    std::tuple<int, int, int, int> blockRect(std::get<0>(rect), std::get<1>(rect), srcWidth, srcHeight);

    const int memSize = m_scene->getBlockSize(blockSize, refChannel, numChannels, numSlices, numFrames);

    py::array::ShapeContainer shape;
    const int planes = numChannels*numSlices*numFrames;

    if(planes==1)
    {
        shape = {trgHeight, trgWidth};
    }
    else
    {
        shape = {trgHeight, trgWidth, planes};
    }

    py::array numpy_array(dtype, shape);

    if(startSlice==0 && stopSlice==1 && startFrame==0 && stopFrame==1)
    {
        m_scene->readResampledBlockChannels(blockRect, blockSize, channelIndices, numpy_array.mutable_data(), memSize);
    }
    else
    {
        m_scene->readResampled4DBlockChannels(blockRect, blockSize, channelIndices, sliceRange, tframeRange, numpy_array.mutable_data(), memSize);
    }

    return numpy_array;
}
