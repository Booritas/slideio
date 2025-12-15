// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/converter/converter_def.hpp"
#include <opencv2/core/types.hpp>

namespace slideio
{
    class CVScene;

    namespace converter
    {
        class ConverterParameters;

        class TiffChannel
        {
        public:
            TiffChannel(const std::string& name, const std::string& id, int samples)
                : m_name(name), m_id(id), m_samples(samples) {
            }

            const std::string& getName() const {
                return m_name;
            }

            const std::string& getID() const {
                return m_id;
            }

            int getSamples() const {
                return m_samples;
            }

        private:
            std::string m_name;
            std::string m_id;
            int m_samples;
        };

        class TiffDirectoryStructure
        {
        public:
            TiffDirectoryStructure() = default;
            
            TiffDirectoryStructure(const TiffDirectoryStructure& other) = default;
            
            TiffDirectoryStructure& operator=(const TiffDirectoryStructure& other) = default;
            
            cv::Range getChannelRange() const {
                return m_channelRange;
            }

            cv::Range getZSliceRange() const {
                return m_zSliceRange;
            }

            cv::Range getTFrameRange() const {
                return m_tFrameRange;
            }

            cv::Range getZoomLevelRange() const {
                return m_zoomLevelRange;
            }

            void setChannelRange(cv::Range range, int sourceFirstChannel) {
                m_channelRange = range;
                m_sourceFirstChannel = sourceFirstChannel;
            }

            void setZSliceRange(cv::Range range, int sourceFirstSlice) {
                m_zSliceRange = range;
                m_sourceFirstSlice = sourceFirstSlice;
            }

            void setTFrameRange(cv::Range range, int sourceFirstFrame) {
                m_tFrameRange = range;
                m_sourceFirstFrame = sourceFirstFrame;
            }

            void setZoomLevelRange(cv::Range range) {
                m_zoomLevelRange = range;
            }

            int getSourceFirstChannel() const {
                return m_sourceFirstChannel;
            }

            int getSourceFirstSlice() const {
                return m_sourceFirstSlice;
            }

            int getSourceFirstFrame() const {
                return m_sourceFirstFrame;
            }

            void setDescription(const std::string& string) {
                m_description = string;
            }

            const std::string& getDescription() const {
                return m_description;
            }

            void setPlaneCount(int count) {
                m_planeCount = count;
            }

            int getPlaneCount() const {
                return m_planeCount;
            }

        private:
            cv::Range m_channelRange;
            cv::Range m_zSliceRange;
            cv::Range m_tFrameRange;
            cv::Range m_zoomLevelRange;
            int m_planeCount = 1;
            int m_sourceFirstChannel = 0;
            int m_sourceFirstSlice = 0;
            int m_sourceFirstFrame = 0;
            std::string m_description;
        };

        class SLIDEIO_CONVERTER_EXPORTS TiffPageStructure : public TiffDirectoryStructure
        {
        public:
            int getNumSubDirectories() const {
                return static_cast<int>(m_subDirectories.size());
            }

            const TiffDirectoryStructure& getSubDirectory(int index) const;

            TiffDirectoryStructure& getSubDirectory(int index);

            TiffDirectoryStructure& appendSubDirectory() {
                return m_subDirectories.emplace_back();
            }

        private:
            std::vector<TiffDirectoryStructure> m_subDirectories;
        };
    }
}
