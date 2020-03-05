// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once

#include "slideio/slideio_def.hpp"
#include "structs.hpp"
#include <string>
#include <vector>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CVScene;
    class SLIDEIO_EXPORTS Scene
    {
    public:
        Scene(std::shared_ptr<CVScene> scene);
        ~Scene() = default;
        std::string getFilePath() const;
        std::string getName() const;
        std::tuple<int,int,int,int> getRect() const;
        int getNumChannels() const;
        int getNumZSlices();
        int getNumTFrames();
        slideio::DataType getChannelDataType(int channel) const;
        std::string getChannelName(int channel) const;
        std::tuple<double,double> getResolution() const;
        double getZSliceResolution() const;
        double getTFrameResolution() const;
        double getMagnification() const;
        int getBlockSize(const std::tuple<int,int>& blockSize, int refChannel, int numChannels, int numSlices, int numFrames) const;
        void readBlock(const std::tuple<int,int,int,int>& blockRect, void* buffer, size_t bufferSize);
        void readBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize);
        void readResampledBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, void* buffer, size_t bufferSize);
        void readResampledBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize);
        void read4DBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        void read4DBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::vector<int>& channelIndices, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        void readResampled4DBlock(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
        void readResampled4DBlockChannels(const std::tuple<int,int,int,int>& blockRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize);
    private:
        std::shared_ptr<CVScene> m_scene;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
