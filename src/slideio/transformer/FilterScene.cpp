// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "FilterScene.hpp"
#include <opencv2/imgproc.hpp>

#include "filter.hpp"
#include "transformertools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/cvtools.hpp"

using namespace slideio;

FilterScene::FilterScene(std::shared_ptr<CVScene> originScene, Transformation& transformation)
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
    case TransformationType::LaplacianFilter:
        {
            m_transformation.reset(new LaplacianFilter);
            *(static_cast<LaplacianFilter*>(m_transformation.get())) = static_cast<LaplacianFilter&>(transformation);
        }
        break;
    case TransformationType::BilateralFilter:
        {
            m_transformation.reset(new BilateralFilter);
            *(static_cast<BilateralFilter*>(m_transformation.get())) = static_cast<BilateralFilter&>(transformation);
        }
        break;
    case TransformationType::CannyFilter:
        {
            m_transformation.reset(new CannyFilter);
            *(static_cast<CannyFilter*>(m_transformation.get())) = static_cast<CannyFilter&>(transformation);
        }
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unsupported transformation type for convolution filters";
    }
}

FilterScene::~FilterScene() = default;

void FilterScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                          const std::vector<int>& componentIndices, int zSliceIndex,
                                                          int tFrameIndex, cv::OutputArray output)
{
    cv::Rect extendedBlockRect;
    cv::Size extendedBlockSize;
    cv::Point blockPosition;
    const int inflationValue = TransformerTools::getBlockInflationValue(*m_transformation);
    cv::Rect sceneRect = getRect();
    cv::Size sceneSize(sceneRect.size());
    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue, blockSize, extendedBlockRect,
                                                extendedBlockSize, blockPosition);
    cv::Mat inflatedBlock;
    getOriginScene()->readResampledBlockChannelsEx(extendedBlockRect, extendedBlockSize, componentIndices, zSliceIndex,
                                                   tFrameIndex, inflatedBlock);
    cv::Mat transformedInflatedBlock;
    applyTransformation(inflatedBlock, transformedInflatedBlock);
    cv::Rect rectInInflatedRect = cv::Rect(blockPosition.x, blockPosition.y, blockSize.width, blockSize.height);
    cv::Mat block = transformedInflatedBlock(rectInInflatedRect);
    block.copyTo(output);
}


void FilterScene::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    switch (m_transformation->getType()) {
    case TransformationType::GaussianBlurFilter:
        {
            const auto& gaussianBlur = getFilter<GaussianBlurFilter>();
            cv::GaussianBlur(block, transformedBlock,
                             cv::Size(gaussianBlur.getKernelSizeX(), gaussianBlur.getKernelSizeY()),
                             gaussianBlur.getSigmaX(), gaussianBlur.getSigmaY());
        }
        break;
    case TransformationType::MedianBlurFilter:
        {
            const auto& medianBlur = getFilter<MedianBlurFilter>();
            cv::medianBlur(block, transformedBlock, medianBlur.getKernelSize());
        }
        break;
    case TransformationType::SobelFilter:
        {
            const auto& sobel = getFilter<SobelFilter>();
            DataType dt = sobel.getDepth();
            int depth = CVTools::cvTypeFromDataType(dt);
            cv::Sobel(block, transformedBlock, depth, sobel.getDx(), sobel.getDy(), sobel.getKernelSize());
        }
        break;
    case TransformationType::ScharrFilter:
        {
            const auto& scharr = getFilter<ScharrFilter>();
            DataType dt = scharr.getDepth();
            int depth = CVTools::cvTypeFromDataType(dt);
            cv::Scharr(block, transformedBlock, depth, scharr.getDx(), scharr.getDy());
        }
        break;
    case TransformationType::LaplacianFilter:
        {
            const auto& laplacian = getFilter<LaplacianFilter>();
            DataType dt = laplacian.getDepth();
            int depth = CVTools::cvTypeFromDataType(dt);
            cv::Laplacian(block, transformedBlock, depth, laplacian.getKernelSize(), laplacian.getScale(),
                          laplacian.getDelta());
        }
        break;
    case TransformationType::BilateralFilter:
        {
            const auto& bilateral = getFilter<BilateralFilter>();
            cv::bilateralFilter(block, transformedBlock, bilateral.getDiameter(), bilateral.getSigmaColor(),
                                bilateral.getSigmaSpace());
            break;
        }
    case TransformationType::CannyFilter:
        {
            const auto& canny = getFilter<CannyFilter>();
            cv::Canny(block, transformedBlock, canny.getThreshold1(), canny.getThreshold2(), canny.getApertureSize(),
                      canny.getL2Gradient());
            break;
        }
    default:
        RAISE_RUNTIME_ERROR << "Unexpected transformation" << (int)m_transformation->getType();
    }
}

DataType FilterScene::getChannelDataType(int channel) const
{
    TransformationType transformationType = m_transformation->getType();
    switch (transformationType) {
    case TransformationType::SobelFilter:
        {
            const SobelFilter& filter = getFilter<SobelFilter>();
            return filter.getDepth();
        }
    case TransformationType::ScharrFilter:
        {
            const ScharrFilter& filter = getFilter<ScharrFilter>();
            return filter.getDepth();
        }
    case TransformationType::LaplacianFilter:
        {
            const LaplacianFilter& filter = getFilter<LaplacianFilter>();
            return filter.getDepth();
        }
    }
    return TransformerScene::getChannelDataType(channel);
}

int FilterScene::getNumChannels() const
{
    TransformationType transformationType = m_transformation->getType();
    if(transformationType==TransformationType::CannyFilter) {
        return 1;
    }
    return TransformerScene::getNumChannels();
}
