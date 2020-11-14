// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/cvslide.hpp"
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/core/cvtools.hpp"
#include <boost/format.hpp>

#include <string>

using namespace slideio;

std::shared_ptr<CVSlide> CVTools::cvOpenSlide(const std::string& filePath, const std::string& driver)
{
    return ImageDriverManager::openSlide(filePath, driver);
}

std::vector<std::string> CVTools::cvGetDriverIDs()
{
    return ImageDriverManager::getDriverIDs();
}

int CVTools::cvGetDataTypeSize(DataType dt)
{
    switch(dt)
    {
    case DataType::DT_Byte: return 1;
    case DataType::DT_Int8: return 1;
    case DataType::DT_Int16: return 2;
    case DataType::DT_Float16: return 2;
    case DataType::DT_Int32: return 4;
    case DataType::DT_Float32: return 4;
    case DataType::DT_Float64: return 8;
    case DataType::DT_UInt16: return 2;
    case DataType::DT_Unknown:
    case DataType::DT_None:
    break;
    }
    throw std::runtime_error("Unsupported data type.");
}

void CVTools::extractSliceFrom3D(cv::Mat mat3D, int sliceIndex, cv::OutputArray output)
{
    std::vector<int> indices = { sliceIndex };
    extractSliceFromMultidimMatrix(mat3D, indices, output);
}

void CVTools::extractSliceFromMultidimMatrix(cv::Mat multidimMat, const std::vector<int>& indices, cv::OutputArray output)
{
    const int dimCount = multidimMat.dims;
    if (dimCount <= 2) {
        throw std::runtime_error(
            (boost::format("Slice extraction: expected 3 or more dimensions. Received: %1%") % multidimMat.dims).str());
    }
    const int extraDims = dimCount - 2;
    if(indices.size()!=extraDims) {
        throw std::runtime_error(
            (boost::format("Slice extraction: expected %1% indices. Received: %2%") % extraDims % indices.size()).str());
    }

    const int height = multidimMat.size[0];
    const int width = multidimMat.size[1];
    const int type = multidimMat.type();
    std::vector<cv::Range> ranges = {
        cv::Range(0, height),
        cv::Range(0, width),
    };
    for (int dim = 0; dim < extraDims; dim++) {
        ranges.emplace_back(indices[dim], indices[dim] + 1);
    }
    const cv::Mat sliceMat = multidimMat(ranges);
    cv::Mat continuousMat;
    sliceMat.copyTo(continuousMat);
    const cv::Mat matOut = continuousMat.reshape(multidimMat.channels(), height);
    output.assign(matOut);
}

void CVTools::insertSliceInMultidimMatrix(cv::Mat multidimMat, cv::Mat sliceMat, const std::vector<int>& indices)
{
    const int dimCount = multidimMat.dims;
    if (dimCount <= 2) {
        throw std::runtime_error(
            (boost::format("Slice extraction: expected 3 or more dimensions. Received: %1%") % multidimMat.dims).str());
    }
    const int extraDims = dimCount - 2;
    if (indices.size() != extraDims) {
        throw std::runtime_error(
            (boost::format("Slice extraction: expected %1% indices. Received: %2%") % extraDims % indices.size()).str());
    }

    const int height = multidimMat.size[0];
    const int width = multidimMat.size[1];
    const int type = multidimMat.type();
    std::vector<cv::Range> ranges = {
        cv::Range(0, height),
        cv::Range(0, width),
    };
    std::vector<int> dims = { height, width };
    for (int dim = 0; dim < extraDims; dim++) {
        ranges.emplace_back(indices[dim], indices[dim] + 1);
        dims.push_back(1);
    }
    cv::Mat sliceMat1 = multidimMat(ranges);
    sliceMat.reshape(multidimMat.channels(), dims.size(), dims.data()).copyTo(sliceMat1);
}
