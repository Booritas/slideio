// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "convolutionfilterscene.hpp"
#include <opencv2/imgproc.hpp>

#include "convolutionfilter.hpp"
#include "transformertools.hpp"
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
                                                          const std::vector<int>& componentIndices, int zSliceIndex,
                                                          int tFrameIndex, cv::OutputArray output)
{
    cv::Rect extendedBlockRect;
    cv::Size extendedBlockSize;
    cv::Point blockPosition;
    const int inflationValue = TransformerTools::getBlockInflationValue(*m_transformation);
    cv::Rect sceneRect = getRect();
    cv::Size sceneSize(sceneRect.size());
    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue, blockSize, extendedBlockRect, extendedBlockSize, blockPosition);
    cv::Mat inflatedBlock;
    getOriginScene()->readResampledBlockChannelsEx(extendedBlockRect, extendedBlockSize, componentIndices, zSliceIndex,
                                                   tFrameIndex, inflatedBlock);
    cv::Mat transformedInflatedBlock;
    applyTransformation(inflatedBlock, transformedInflatedBlock);
    cv::Rect rectInInflatedRect = cv::Rect(blockPosition.x, blockPosition.y, blockSize.width, blockSize.height);
    cv::Mat block = transformedInflatedBlock(rectInInflatedRect);
    block.copyTo(output);
}


void ConvolutionFilterScene::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
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
    default:
        RAISE_RUNTIME_ERROR << "Unexpected transformation" << (int)m_transformation->getType();
    }
}

DataType ConvolutionFilterScene::getChannelDataType(int channel) const
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
    }
    return TransformerScene::getChannelDataType(channel);
}
