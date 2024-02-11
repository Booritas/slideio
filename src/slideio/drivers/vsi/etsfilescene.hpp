// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include <opencv2/core.hpp>


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class EtsFile;
        class VSIFile;
        class SLIDEIO_VSI_EXPORTS EtsFileScene : public VSIScene
        {
        public:
            EtsFileScene(const std::string& filePath, std::shared_ptr<VSIFile>& vsiFile, int etsIndex);
        public:
            int getTileCount(void* userData) override;
            bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
            bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                          void* userData) override;
            void addAuxImage(const std::string& name, std::shared_ptr<CVScene> scene);
            std::shared_ptr<CVScene> getAuxImage(const std::string& imageName) const override;
            int getNumZSlices() const override;
            int getNumTFrames() const override;
            int getNumLambdas() const;
            int getNumPyramidLevels() const;
            DataType getChannelDataType(int channelIndex) const override;
            Resolution getResolution() const override;
            double getZSliceResolution() const override;
            double getTFrameResolution() const override;
            int getNumChannels() const override;
            void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                cv::OutputArray output) override;
        protected:
            void init();
            std::shared_ptr<EtsFile> getEtsFile() const;
            int findZoomLevelIndex(double zoom) const;
        protected:
            int m_etsIndex;
            std::map<std::string, std::shared_ptr<CVScene>> m_auxScenes;
        };
    }

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
