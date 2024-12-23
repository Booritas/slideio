// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "pyglobals.hpp"
#include <slideio/slideio/slideio.hpp>
#include <slideio/imagetools/imagetools.hpp>
#include <slideio/slideio/imagedrivermanager.hpp>
#include <boost/format.hpp>
#include <opencv2/core/mat.hpp>

#include "slideio/core/tools/cvtools.hpp"

namespace py = pybind11;

std::shared_ptr<PySlide> pyOpenSlide(const std::string& path, const std::string& driver)
{
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, driver);
    if(!slide)
    {
        throw std::runtime_error(
            (boost::format("Cannot open file \"%1%\" with driver \"%2%\"") % path % driver).str());
    }
    PySlide* pySlide = new PySlide(slide);
    std::shared_ptr<PySlide> wrapper(pySlide);
    return wrapper;
}


std::vector<std::string> pyGetDriverIDs()
{
    return slideio::getDriverIDs();
}
static slideio::DataType typeFromNumpyType(const pybind11::dtype& type)
{
    if (type.is(py::detail::npy_format_descriptor<uint8_t>::dtype()))
    {
    return slideio::DataType::DT_Byte;
    }
    else if(type.is(py::detail::npy_format_descriptor<int8_t>::dtype()))
    {
        return slideio::DataType::DT_Int8;
    }
    else if(type.is(py::detail::npy_format_descriptor<int16_t>::dtype()))
    {
        return slideio::DataType::DT_Int16;
    }
    else if (type.is(py::detail::npy_format_descriptor<uint16_t>::dtype()))
    {
        return slideio::DataType::DT_UInt16;
    }
    else if (type.is(py::detail::npy_format_descriptor<int32_t>::dtype()))
    {
        return slideio::DataType::DT_Int32;
    }
    else if (type.is(py::detail::npy_format_descriptor<float>::dtype()))
    {
        return slideio::DataType::DT_Float32;
    }
    else if (type.is(py::detail::npy_format_descriptor<double>::dtype()))
    {
        return slideio::DataType::DT_Float64;
    }
    throw std::runtime_error("Cannot convert numpy data type to internal type");
}

static cv::Mat fromNumpy2Mat(py::array& np_array)
{
    slideio::DataType dt = typeFromNumpyType(np_array.dtype());
    int cvType = slideio::CVTools::toOpencvType(dt);
    size_t dims = np_array.ndim();
    if(dims>3) {
        throw std::runtime_error("Only 2D images are supported.");
    }
    int width = (int)np_array.shape(0);
    int height = (int)np_array.shape(1);
    int channels = 1;
    if(dims>2) {
        channels = (int)np_array.shape(2);
    }
    cv::Mat mat(height, width, CV_MAKETYPE(cvType, channels), np_array.mutable_data());
    return mat;
}

double pyCompareImages(py::array& left, py::array& right)
{
    cv::Mat leftMat = fromNumpy2Mat(left);
    cv::Mat rightMat = fromNumpy2Mat(right);
    double sim = slideio::ImageTools::computeSimilarity(leftMat, rightMat);
    return sim;
}

void pySetLogLevel(const std::string& level)
{
    slideio::ImageDriverManager::setLogLevel(level);
}