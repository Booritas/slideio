// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_scene_HPP
#define OPENCV_slideio_scene_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/structs.hpp"
#include "slideio/slideio_enums.hpp"
#include "opencv2/core.hpp"
#include <vector>
#include <string>

namespace slideio
{
    class SLIDEIO_EXPORTS CVScene
    {
    public:
        virtual ~CVScene() = default;
        virtual std::string getFilePath() const = 0;
        virtual std::string getName() const = 0;
        virtual cv::Rect getRect() const = 0;
        virtual int getNumChannels() const = 0;
        virtual int getNumZSlices() const {return 1;}
        virtual int getNumTFrames() const {return 1;}
        virtual slideio::DataType getChannelDataType(int channel) const = 0;
        virtual std::string getChannelName(int channel) const;
        virtual Resolution  getResolution() const = 0;
        virtual double getZSliceResolution() const {return 0;}
        virtual double getTFrameResolution() const {return 0;}
        virtual double getMagnification() const = 0;
        virtual Compression getCompression() const = 0;
        virtual void readBlock(const cv::Rect& blockRect, cv::OutputArray output);
        virtual void readBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, cv::OutputArray output);
        virtual void readResampledBlock(const cv::Rect& blockRect, const cv::Size& blockSize, cv::OutputArray output);
        virtual void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output) = 0;
        virtual void read4DBlock(const cv::Rect& blockRect, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        virtual void read4DBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        virtual void readResampled4DBlock(const cv::Rect& blockRect, const cv::Size& blockSize, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
        virtual void readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output);
    };
}

#endif