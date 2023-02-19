// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#ifndef OPENCV_slideio_cvtools_HPP
#define OPENCV_slideio_cvtools_HPP

#include <opencv2/core.hpp>
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <string>
#include <vector>

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS CVTools
    {
    public:
        static std::shared_ptr<CVSlide> cvOpenSlide(const std::string& path, const std::string& driver);
        static std::vector<std::string> cvGetDriverIDs();

        static inline DataType fromOpencvType(int type)
        {
            return static_cast<DataType>(type);
        }
        static inline int toOpencvType(DataType dt)
        {
            return static_cast<int>(dt);
        }

        static inline bool isValidDataType(slideio::DataType dt)
        {
            return dt <= slideio::DataType::DT_LastValid;
        }

        static int cvGetDataTypeSize(DataType dt);
        static void extractSliceFrom3D(cv::Mat mat3D, int sliceIndex, cv::OutputArray output);
        static void extractSliceFromMultidimMatrix(cv::Mat multidimMat, const std::vector<int>& indices,
                                                   cv::OutputArray output);
        /**
         * \brief Copies a 2D raster to a slice of a 3D/4D/XD matrix.
         * \param multidimMat: 3/4/XD matrix to be modified
         * \param sliceMat: 2D slice raster to be copied to the multi-dim matrix
         * \param indices vector of indices of the slice. For 3D matrix - size=1, for 4D matrix size=2 etc.
         */
        static void insertSliceInMultidimMatrix(cv::Mat& multidimMat, const cv::Mat& sliceMat,
                                                const std::vector<int>& indices);
        static std::string dataTypeToString(DataType dataType);
        static std::string compressionToString(Compression dataType);
    };
}

#endif
