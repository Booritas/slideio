// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <string>

#include "slideio/imagetools/tiffkeeper.hpp"

namespace tinyxml2
{
    class XMLElement;
    class XMLDocument;
}

namespace slideio
{
    namespace ometiff
    {
        struct ImageData
        {
            std::shared_ptr<tinyxml2::XMLDocument> doc;
            tinyxml2::XMLElement* imageXml;
            std::string imageId;
            std::string imageFilePath;
        };

		typedef std::vector<int> PlaneCoordinates;

   //     struct TiffBlockData
   //     {
   //         cv::Range channelRange = {};
   //         cv::Range zSliceRange = {};
   //         cv::Range tFrameRange = {};
			//int IFD = 0;
   //         int planeCount = 0;
   //         std::string filePath;
   //         libtiff::TIFF* tiff = nullptr;
   //         TiffDirectory tiffDirectory;
			//bool isInRange(int channel, int slice, int frame) const {
			//	return (channel >= channelRange.start && channel < channelRange.end &&
			//		slice >= zSliceRange.start && slice < zSliceRange.end &&
			//		frame >= tFrameRange.start && frame < tFrameRange.end);
			//}
   //     };

        const std::string DimC = "C";
        const std::string DimZ = "Z";
        const std::string DimT = "T";
    }
}
