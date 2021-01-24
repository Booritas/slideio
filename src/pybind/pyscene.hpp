#pragma once
#include "slideio/scene.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "pyaux.hpp"

namespace slideio
{
    class Slide;
}

class PyScene
{
public:
    PyScene(std::shared_ptr<slideio::Scene> scene, std::shared_ptr<slideio::Slide> slide);
    ~PyScene(){
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
    slideio::Compression getCompression() const;
    pybind11::dtype getChannelDataType(int channel) const;
    pybind11::array readBlock(std::tuple<int,int,int,int> rect,
        std::tuple<int,int> size, std::vector<int> channelIndices,
        std::tuple<int,int> sliceRange, std::tuple<int,int> tframeRange) const;
    std::list<std::string> getAuxImageNames() const;
    int getNumAuxImages() const;
    std::shared_ptr<PyScene> getAuxImage(const std::string& imageName);
private:
    PyRect adjustSourceRect(const PyRect& rect) const;
    PySize adjustTargetSize(const PyRect& rect, const PySize& size) const;
private:
    std::shared_ptr<slideio::Scene> m_scene;
    std::shared_ptr<slideio::Slide> m_slide;
};
