
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformer_def.hpp"
#include "transformerscene.hpp"
#include "convolutionfilter.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS ConvolutionFilterScene : public TransformerScene
    {
    public:
        ConvolutionFilterScene(std::shared_ptr<CVScene> originScene, Transformation& transformation);
        ~ConvolutionFilterScene();

    public:
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                          cv::OutputArray output) override;
        int getBlockExtensionForGaussianBlur(const GaussianBlurFilter& gaussianBlur) const;
        cv::Rect extendBlockRect(const cv::Rect& rect);
        void appyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock);
        DataType getChannelDataType(int channel) const override;
    private:
        template <class Filter>
        const Filter& getFilter() const{
            return (static_cast<const Filter&>(*m_transformation.get()));
        }
    private:
        std::shared_ptr<Transformation> m_transformation;
    };
}
