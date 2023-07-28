// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "slideio/drivers/vsi/vsi_api_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS Pyramid
        {
        public:
            Pyramid();
            ~Pyramid();
            std::vector<std::string> channelNames;
            std::string name;
            int width = 0;
            int height = 0;
            int tileOriginX = 0;
            int tileOriginY = 0;
            double physicalSizeX = 0;
            double physicalSizeY = 0;
            double originX = 0;
            double originY = 0;
            std::vector<std::string> deviceTypes;
            std::vector<std::string> deviceIDs;
            std::vector<std::string> deviceNames;
            std::vector<std::string> deviceManufacturers;
            std::vector<int64_t> exposureTimes;
            int64_t defaultExposureTime = 0;
            std::vector<int64_t> otherExposureTimes;
            int64_t acquisitionTime = 0;
            double refractiveIndex = 0;
            double magnification = 0;
            double numericalAperture = 0;
            double workingDistance = 0;
            std::vector<std::string> objectiveNames;
            std::vector<int> objectiveTypes;
            int bitDepth = 0;
            int binningX = 0;
            int binningY = 0;
            double gain = 0;
            double offset = 0;
            double redGain = 0;
            double greenGain = 0;
            double blueGain = 0;
            double redOffset = 0;
            double greenOffset = 0;
            double blueOffset = 0;
            std::vector<double> channelWavelengths;
            double zStart = 0;
            double zIncrement = 0;
            std::vector<double> zValues;
            std::map<std::string,int> dimensionOrdering;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
