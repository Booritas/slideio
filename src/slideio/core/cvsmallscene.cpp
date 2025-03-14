// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "cvsmallscene.hpp"
#include <opencv2/imgproc.hpp>

#include "slideio/base/exceptions.hpp"

using namespace slideio;

void CVSmallScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)

{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "CVSmallScene: 3D and 4D images are not supported";
	}
    cv::Mat image;
    readImage(image);
    cv::Rect imageRect = { 0, 0, image.size().width, image.size().height };
    cv::Rect intersection = blockRect & imageRect;
    cv::Mat imageBlock = image(intersection);
    cv::Mat block;
    if(!channelIndices.empty()) {
        std::vector<cv::Mat> channels;
        for(auto channelIndex:channelIndices) {
            cv::Mat channel;
            cv::extractChannel(imageBlock, channel, channelIndex);
            channels.push_back(channel);
        }
        cv::merge(channels.data(), channels.size(), block);
    } else {
        block = imageBlock;
    }
    cv::Mat resizedBlock;
    cv::resize(block, resizedBlock, blockSize);
    if(output.empty()) {
        output.assign(resizedBlock);
    }
    else {
        imageBlock.copyTo(output);
    }

}
