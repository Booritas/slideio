// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/cvscene.hpp"
#include "transformer_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class Transformation;

    class SLIDEIO_TRANSFORMER_EXPORTS TransformerScene : public CVScene
    {
    public:
        TransformerScene(std::shared_ptr<CVScene> originScene, const std::list<std::shared_ptr<Transformation>>& list);
    public:
        std::string getFilePath() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        std::string getChannelName(int channel) const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        std::string getRawMetadata() const override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
        std::shared_ptr<CVScene> getOriginScene() const {
            return m_originScene;
        }
    private:
        void initChannels();
        void computeInflationValue();
    private:
        std::shared_ptr<CVScene> m_originScene;
        std::list<std::shared_ptr<Transformation>> m_transformations;
        std::vector<DataType> m_channelDataTypes;
        int m_inflationValue;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
