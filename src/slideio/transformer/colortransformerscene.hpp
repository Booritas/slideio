// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformer_def.hpp"
#include "transformerscene.hpp"
#include "transformation.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS ColorTransformerScene : public TransformerScene
    {
    public:
        ColorTransformerScene(std::shared_ptr<CVScene> originScene, ColorTransformation& transformation);

    public:
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;

    private:
        ColorTransformation m_transformation;
    };
}
