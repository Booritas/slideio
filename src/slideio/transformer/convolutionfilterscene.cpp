// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "convolutionfilterscene.hpp"
#include <opencv2/imgproc.hpp>

#include "convolutionfilter.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

ConvolutionFilterScene::ConvolutionFilterScene(std::shared_ptr<CVScene> originScene, Transformation& transformation)
    : TransformerScene(originScene)
{
       m_transformation = transformation;
}

void ConvolutionFilterScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    cv::Mat inflatedBlock;
    cv::Rect inflatedBlockRect = inflateRect(blockRect);
    getOriginScene()->readResampledBlockChannelsEx(inflatedBlockRect, blockSize, componentIndices, zSliceIndex, tFrameIndex, inflatedBlock);
    cv::Mat transformedInflatedBlock;
    appyTransformation(inflatedBlock, transformedInflatedBlock);
    cv::Rect rectInInflatedRect = cv::Rect(blockRect.x - inflatedBlockRect.x, blockRect.y - inflatedBlockRect.y, blockRect.width, blockRect.height);
    cv::Mat block = transformedInflatedBlock(rectInInflatedRect);
    block.copyTo(output);
}

int ConvolutionFilterScene::getBlockExtensionForGaussianBlur(const GaussianBlurFilter& gaussianBlur) const
{   int kernel = std::max(gaussianBlur.getKernelSizeX(), gaussianBlur.getKernelSizeY());
    if(kernel == 0) {
        kernel = (int)ceil(2 * 
            ceil(2 * std::max(gaussianBlur.getSigmaX(), gaussianBlur.getSigmaY()))
            + 1);
    }
    const int extension = (kernel + 1) / 2;
    return extension;
}

cv::Rect ConvolutionFilterScene::inflateRect(const cv::Rect& rect)
{
    int shift = 0;
    switch(m_transformation.getType()) {
    case TransformationType::GaussianBlurFilter: {
            const auto& gaussianBlur = static_cast<GaussianBlurFilter&>(m_transformation);
            shift = getBlockExtensionForGaussianBlur(gaussianBlur);
        }
        break;
    case TransformationType::MedianBlurFilter: {
            const auto& medianBlur = static_cast<MedianBlurFilter&>(m_transformation);
            shift = (medianBlur.getKernelSize()+1)/2;
        }
        break;
    case TransformationType::SobelFilter: {
            const auto& sobel = static_cast<SobelFilter&>(m_transformation);
            shift = (sobel.getKernelSize() + 1) / 2;
    }
        break;
    case TransformationType::ScharrFilter: {
            shift = 2;
        }
        break;
    default: 
        RAISE_RUNTIME_ERROR << "Unexpected transformation" << (int)m_transformation.getType();
    }
    int left = rect.x - shift;
    if(left <0) {
        left = 0;
    }
    int top = rect.y - shift;
    if(top < 0) {
        top = 0;
    }
    const cv::Size sceneSize = getRect().size();
    int right = rect.x + rect.width + shift;
    if(right > sceneSize.width) {
        right = sceneSize.width;
    }
    int bottom = rect.y + rect.height + shift;
    if (bottom > sceneSize.height) {
        bottom = sceneSize.height;
    }
    cv::Rect inflatedRect(left,top,right-left,bottom-top);
    return inflatedRect;
}
