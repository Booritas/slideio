// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_tools_HPP
#define OPENCV_slideio_tools_HPP

#if defined(WIN32)
#elif __APPLE__
#else
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#endif
#include "slideio/core/slideio_core_def.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <list>
#include <opencv2/core.hpp>
#include "slideio/base/slideio_enums.hpp"
#include "slideio/base/slideio_structs.hpp"

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS Tools
    {
    public:
        struct FileDeleter {
            void operator()(std::FILE* file) const {
                if (file) {
                    std::fclose(file);
                }
            }
        };
        static bool matchPattern(const std::string& path, const std::string& pattern);
        static std::vector<int> completeChannelList(const std::vector<int>& orgChannelList, int numChannels)
        {
            std::vector<int> channelList(orgChannelList);
            if(channelList.empty())
            {
                channelList.resize(numChannels);
                for(int channel=0; channel<numChannels; ++channel)
                {
                    channelList[channel] = channel;
                }
            }
            return channelList;
        }
        template <typename Functor>
        static int findZoomLevel(double zoom, int numLevels, Functor zoomFunction)
        {
            if(numLevels <= 0) {
                return -1;
            }
            const double baseZoom = zoomFunction(0);
            if (zoom >= baseZoom)
            {
                return 0;
            }
            int goodLevelIndex = -1;
            double lastZoom = baseZoom;
            for (int levelIndex = 1; levelIndex < numLevels; levelIndex++)
            {
                const double currentZoom = zoomFunction(levelIndex);
                const double absDif = std::abs(currentZoom - zoom);
                const double relDif = absDif / currentZoom;
                if (relDif < 0.01)
                {
                    goodLevelIndex = levelIndex;
                    break;
                }
                if (zoom <= lastZoom && zoom > currentZoom)
                {
                    goodLevelIndex = levelIndex - 1;
                    break;
                }
                lastZoom = currentZoom;
            }
            if (goodLevelIndex < 0)
            {
                goodLevelIndex = numLevels - 1;
            }
            return  goodLevelIndex;
        }
        static void convert12BitsTo16Bits(const uint8_t* source, uint16_t* target, int targetLen);
        static void scaleRect(const cv::Rect& srcRect, const cv::Size& newSize, cv::Rect& trgRect);
        static void scaleRect(const cv::Rect& srcRect, double scaleX, double scaleY, cv::Rect& trgRect);
        static bool isCompleteChannelList(const std::vector<int>& channelIndices, const int numChannels)
        {
            bool allChannels = channelIndices.empty();
            if(!allChannels) {
                if (channelIndices.size() == numChannels) {
                    allChannels = true;
                    for (int channel = 0; channel < channelIndices.size(); ++channel) {
                        if (channelIndices[channel] != channel) {
                            allChannels = false;
                            break;
                        }
                    }
                }
            }
            return allChannels;
        }
#if defined(WIN32)
          static std::wstring toWstring(const std::string& string);
#endif

        static std::string fromUnicode16(const std::u16string& u16string);
        static void throwIfPathNotExist(const std::string& path, const std::string label);
        static std::list<std::string> findFilesWithExtension(const std::string& directory, const std::string& extension);
        static void extractChannels(const cv::Mat& sourceRaster, const std::vector<int>& channels, cv::OutputArray output);
        static FILE* openFile(const std::string& filePath, const char* mode);
        static uint64_t getFilePos(FILE* file);
        static int setFilePos(FILE* file, uint64_t pos, int origin);
        static uint64_t getFileSize(FILE* file);
        static int dataTypeSize(slideio::DataType dt);
        static Size cvSizeToSize(const cv::Size& cvSize) {
            return {cvSize.width, cvSize.height};
        }
        static Rect cvRectToRect(const cv::Rect& cvRect) {
            return {cvRect.x, cvRect.y, cvRect.width, cvRect.height};
        }
        static void replaceAll(std::string& str, const std::string& from, const std::string& to);
        static std::vector<std::string> split(const std::string& value, char delimiter);

    };
}
#endif
