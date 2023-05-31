// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "convolutionfilterscene.hpp"
#include <opencv2/imgproc.hpp>

#include "convolutionfilter.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/cvtools.hpp"

using namespace slideio;

ConvolutionFilterScene::ConvolutionFilterScene(std::shared_ptr<CVScene> originScene, Transformation& transformation)
    : TransformerScene(originScene)
{
    switch (transformation.getType()) {
    case TransformationType::GaussianBlurFilter:
        {
            m_transformation.reset(new GaussianBlurFilter);
            *(static_cast<GaussianBlurFilter*>(m_transformation.get())) = static_cast<GaussianBlurFilter&>(
                transformation);
        }
        break;
    case TransformationType::MedianBlurFilter:
        {
            m_transformation.reset(new MedianBlurFilter);
            *(static_cast<MedianBlurFilter*>(m_transformation.get())) = static_cast<MedianBlurFilter&>(transformation);
        }
        break;
    case TransformationType::ScharrFilter:
        {
            m_transformation.reset(new ScharrFilter);
            *(static_cast<ScharrFilter*>(m_transformation.get())) = static_cast<ScharrFilter&>(transformation);
        }
        break;
    case TransformationType::SobelFilter:
        {
            m_transformation.reset(new SobelFilter);
            *(static_cast<SobelFilter*>(m_transformation.get())) = static_cast<SobelFilter&>(transformation);
        }
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unsupported transformation type for convolution filters";
    }
}

ConvolutionFilterScene::~ConvolutionFilterScene()
{

}

void ConvolutionFilterScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                          const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    cv::Mat inflatedBlock;
    cv::Rect inflatedBlockRect = extendBlockRect(blockRect);
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
            ceil(2 * std::max(gaussianBlur.getSigmaX(), gaussianBlur.getSigmaY())) + 1);
    }
    const int extension = (kernel + 1) / 2;
    return extension;
}

cv::Rect ConvolutionFilterScene::extendBlockRect(const cv::Rect& rect)
{
    int shift = 0;
    switch (m_transformation->getType()) {
    case TransformationType::GaussianBlurFilter:
        {
            shift = getBlockExtensionForGaussianBlur(getFilter<GaussianBlurFilter>());
        }
        break;
    case TransformationType::MedianBlurFilter:
        {
            const auto& medianBlur = getFilter<MedianBlurFilter>();
            shift = (medianBlur.getKernelSize() + 1) / 2;
        }
        break;
    case TransformationType::SobelFilter:
        {
            const auto& sobel = getFilter<SobelFilter>();
            shift = (sobel.getKernelSize() + 1) / 2;
        }
        break;
    case TransformationType::ScharrFilter:
        {
            shift = 2;
        }
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unexpected transformation type for convolution filter: "
            << (int)m_transformation->getType() << ".";
    }
    int left = rect.x - shift;
    if (left < 0) {
        left = 0;
    }
    int top = rect.y - shift;
    if (top < 0) {
        top = 0;
    }
    const cv::Size sceneSize = getRect().size();
    int right = rect.x + rect.width + shift;
    if (right > sceneSize.width) {
        right = sceneSize.width;
    }
    int bottom = rect.y + rect.height + shift;
    if (bottom > sceneSize.height) {
        bottom = sceneSize.height;
    }
    cv::Rect inflatedRect(left, top, right - left, bottom - top);
    return inflatedRect;
}

void ConvolutionFilterScene::appyTransformation(const cv::Mat& mat, const cv::Mat& transformedInflatedBlock)
{
    switch (m_transformation->getType()) {
    case TransformationType::GaussianBlurFilter:
        {
            const auto& gaussianBlur = getFilter<GaussianBlurFilter>();
            cv::GaussianBlur(mat, transformedInflatedBlock,
                             cv::Size(gaussianBlur.getKernelSizeX(), gaussianBlur.getKernelSizeY()),
                             gaussianBlur.getSigmaX(), gaussianBlur.getSigmaY());
        }
        break;
    case TransformationType::MedianBlurFilter:
        {
            const auto& medianBlur = getFilter<MedianBlurFilter>();
            cv::medianBlur(mat, transformedInflatedBlock, medianBlur.getKernelSize());
        }
        break;
    case TransformationType::SobelFilter:
        {
            const auto& sobel = getFilter<SobelFilter>();
            DataType dt = sobel.getDepth();
            int depth = CVTools::cvTypeFromDataType(dt);
            cv::Sobel(mat, transformedInflatedBlock, depth, sobel.getDx(), sobel.getDy(), sobel.getKernelSize());
        }
        break;
    case TransformationType::ScharrFilter:
        {
            const auto& scharr = getFilter<ScharrFilter>();
            DataType dt = scharr.getDepth();
            int depth = CVTools::cvTypeFromDataType(dt);
            cv::Scharr(mat, transformedInflatedBlock, -1, scharr.getDx(), scharr.getDy());
        }
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unexpected transformation" << (int)m_transformation->getType();
    }
}
