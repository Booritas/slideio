#pragma once
#include "slideio/core/cvscene.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>


class PyScene
{
public:
    PyScene(std::shared_ptr<slideio::CVScene> scene) : m_Scene(std::move(scene))
    {
    }
    std::string getFilePath() const;
    std::string getName() const;
    std::tuple<int,int,int,int> getRect() const;
    int getNumChannels() const;
    int getNumZSlices() const;
    int getNumTFrames() const;
    std::string getChannelName(int channel) const;
    std::tuple<double, double> getResolution() const;
    double getZSliceResolution() const;
    double getTFrameResolution() const;
    double getMagnification() const;
    pybind11::dtype getChannelDataType(int channel) const;
    pybind11::array readBlock(std::tuple<int,int,int,int> rect,
        std::tuple<int,int> size, std::vector<int> channelIndices,
        std::tuple<int,int> sliceRange, std::tuple<int,int> tframeRange) const;
private:
    std::shared_ptr<slideio::CVScene> m_Scene;
};
