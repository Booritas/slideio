// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include <boost/format.hpp>
#include "slideio/base/base.hpp"

#include <string>

using namespace slideio;


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
    RAISE_RUNTIME_ERROR << "Unsupported data type" << (int)dt;
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
        RAISE_RUNTIME_ERROR << "Slice extraction: expected 3 or more dimensions. Received:" << multidimMat.dims;
    }
    const int extraDims = dimCount - 2;
    if(indices.size()!=extraDims) {
        RAISE_RUNTIME_ERROR << "Slice extraction : expected: " << extraDims << " indices. Received:" << indices.size();
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

void CVTools::insertSliceInMultidimMatrix(cv::Mat& multidimMat, const cv::Mat& sliceMat, const std::vector<int>& indices)
{
    const int dimCount = multidimMat.dims;
    if (dimCount <= 2) {
        RAISE_RUNTIME_ERROR << "Slice copy: expected 3 or more dimensions. Received:" << multidimMat.dims;
    }
    const int extraDims = dimCount - 2;
    if (indices.size() != extraDims) {
        RAISE_RUNTIME_ERROR << "Slice copy: expected: " << extraDims << " indices. Received:" << indices.size();
    }
    if(multidimMat.type()!=sliceMat.type())
    {
        RAISE_RUNTIME_ERROR << "Slice copy: Only matrices of the same type are supported for copy slice operation"
            << "Received types: XD: " << multidimMat.type() << ". 2D: " << sliceMat.type();
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
    void* dataPtr1 = sliceMat1.data;
    cv::Mat tmpMat = sliceMat.reshape(multidimMat.channels(), static_cast<int>(dims.size()), dims.data());
    tmpMat.copyTo(sliceMat1);
    void* dataPtr2 = sliceMat1.data;
    if(dataPtr1!=dataPtr2)
    {
        RAISE_RUNTIME_ERROR << "Slice copy: Unexpected memory reallocation";
    }
}

std::string CVTools::dataTypeToString(DataType dataType) {
#define TONAME(name) std::string(#name)

    switch(dataType) {
    case DataType::DT_Byte: return TONAME(DT_Byte);
    case DataType::DT_Int8: return TONAME(DT_Int8);
    case DataType::DT_Int16: return TONAME(DT_Int16);
    case DataType::DT_Float16: return TONAME(DT_Float16);
    case DataType::DT_Int32: return TONAME(DT_Int32);
    case DataType::DT_Float32: return TONAME(DT_Float32);
    case DataType::DT_Float64: return TONAME(DT_Float64);
    case DataType::DT_UInt16: return TONAME(DT_UInt16);
    case DataType::DT_Unknown: return TONAME(DT_Unknown);
    case DataType::DT_None: return TONAME(DT_None);
    }
    RAISE_RUNTIME_ERROR << "Unexpected data type: " << static_cast<int>(dataType);
}

std::string CVTools::compressionToString(Compression compression) {
    std::string name;
    switch (compression) {
    case slideio::Compression::Unknown: name = "Unknown";  break;
    case slideio::Compression::Uncompressed: name = "Uncompressed";  break;
    case slideio::Compression::Jpeg:  name = "Jpeg";  break;
    case slideio::Compression::JpegXR: name = "JpegXR";  break;
    case slideio::Compression::Png: name = "Png";  break;
    case slideio::Compression::Jpeg2000: name = "Jpeg2000"; break;
    case slideio::Compression::LZW: name = "LZW"; break;
    case slideio::Compression::HuffmanRL: name = "HuffmanRL"; break;
    case slideio::Compression::CCITT_T4: name = "CCITT_T4"; break;
    case slideio::Compression::CCITT_T6: name = "CCITT_T6"; break;
    case slideio::Compression::JpegOld: name = "JpegOld"; break;
    case slideio::Compression::Zlib: name = "Zlib"; break;
    case slideio::Compression::JBIG85: name = "JBIG85"; break;
    case slideio::Compression::JBIG43: name = "JBIG43"; break;
    case slideio::Compression::NextRLE: name = "NextRLE"; break;
    case slideio::Compression::PackBits: name = "PackBits"; break;
    case slideio::Compression::ThunderScanRLE: name = "ThunderScanRLE"; break;
    case slideio::Compression::RasterPadding: name = "RasterPadding"; break;
    case slideio::Compression::RLE_LW: name = "RLE_LW"; break;
    case slideio::Compression::RLE_HC: name = "RLE_HC"; break;
    case slideio::Compression::RLE_BL: name = "RLE_BL"; break;
    case slideio::Compression::PKZIP: name = "PKZIP"; break;
    case slideio::Compression::KodakDCS: name = "KodakDCS"; break;
    case slideio::Compression::JBIG: name = "JBIG"; break;
    case slideio::Compression::NikonNEF: name = "NikonNEF"; break;
    case slideio::Compression::JBIG2: name = "JBIG2"; break;
    case slideio::Compression::GIF: name = "GIF"; break;
    case slideio::Compression::BIGGIF: name = "BIGGIF"; break;
    case slideio::Compression::RLE: name = "RLE"; break;
    default: name = std::to_string((int)compression);
    }
    return name;
}
