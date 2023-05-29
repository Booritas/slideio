// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "colortransformerscene.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/base/exceptions.hpp"

using namespace slideio;

ColorTransformerScene::ColorTransformerScene(
    std::shared_ptr<CVScene> originScene,
    ColorTransformation& transformation):
        TransformerScene(originScene),
        m_transformation(transformation)
{
}

void ColorTransformerScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    std::vector<int> allChannels = { 0,1,2 };
    cv::Mat block;
    getOriginScene()->readResampledBlockChannelsEx(blockRect, blockSize, allChannels, zSliceIndex, tFrameIndex, block);
    ColorSpace targetColorSpace = m_transformation.getColorSpace();
    cv::Mat converted;
    switch (targetColorSpace) {
        case ColorSpace::GRAY:
            cv::cvtColor(block, converted, cv::COLOR_RGB2GRAY);
            break;
        case ColorSpace::HSV:
            cv::cvtColor(block, converted, cv::COLOR_RGB2HSV);
            break;
        case ColorSpace::HLS:
            cv::cvtColor(block, converted, cv::COLOR_RGB2HLS);
            break;
        case ColorSpace::YUV:
            cv::cvtColor(block, converted, cv::COLOR_RGB2YUV);
            break;
        case ColorSpace::YCbCr:
            cv::cvtColor(block, converted, cv::COLOR_RGB2YCrCb);
            break;
        case ColorSpace::XYZ:
            cv::cvtColor(block, converted, cv::COLOR_RGB2XYZ);
            break;
        case ColorSpace::LAB:
            cv::cvtColor(block, converted, cv::COLOR_RGB2Lab);
            break;
        case ColorSpace::LUV:
            cv::cvtColor(block, converted, cv::COLOR_RGB2Luv);
            break;
        default:
            RAISE_RUNTIME_ERROR << "Unsupported color space";
    }
    if(componentIndices.empty() 
        || targetColorSpace==ColorSpace::GRAY
        || (componentIndices.size()==3 
            && componentIndices[0]==0 
            && componentIndices[1]==1 
            && componentIndices[2]==2)) {
        converted.copyTo(output);
    }
    else {
        std::vector<cv::Mat> channels;
        cv::split(converted, channels);
        std::vector<cv::Mat> selectedChannels;
        for(auto index : componentIndices) {
                       selectedChannels.push_back(channels[index]);
        }
        cv::merge(selectedChannels, output);
        
    }
}

int ColorTransformerScene::getNumChannels() const
{
    if(m_transformation.getColorSpace() == ColorSpace::GRAY)
        return 1;
    return TransformerScene::getNumChannels();
}
