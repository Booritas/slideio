// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "taginfo.hpp"
#include "vsistruct.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/vsi/dimensions.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS Volume : public IDimensionOrder
        {
        public:
            // The order of a dimension is only known once DIMENSION_DESCRIPTION has
            // been read, so every dimension starts unset. X and Y are the exception:
            // the format fixes them at 0 and 1, which is also why UNSET_DIMENSION_ORDER
            // cannot be 0 -- that is a real order, and readers here test for "> 1".
            Volume() {
                std::fill(std::begin(m_dimensionOrder), std::end(m_dimensionOrder),
                          UNSET_DIMENSION_ORDER);
                m_dimensionOrder[dimensionIndex(Dimensions::X)] = 0;
                m_dimensionOrder[dimensionIndex(Dimensions::Y)] = 1;
            }
            std::string getName() const { return m_name; }
            void setName(const std::string& name) { m_name = name; }

            double getMagnification() const { return m_magnification; }
            void setMagnification(double magnification) { m_magnification = magnification; }

            StackType getType() const { return m_type; }
            void setType(StackType type) { m_type = type; }

            cv::Size getSize() const { return m_size; }
            void setSize(const cv::Size& size) { m_size = size; }

            int getBitDepth() const { return m_bitDepth; }
            void setBitDepth(int bitDepth) { m_bitDepth = bitDepth; }

            bool hasExternalFile() const { return m_hasExternalFile; }
            void setHasExternalFile(bool hasExternalFile) { m_hasExternalFile = hasExternalFile; }

            int getNumAuxVolumes() const { return static_cast<int>(m_auxVolumes.size()); }
            std::shared_ptr<Volume> getAuxVolume(int index) const { return m_auxVolumes[index]; }
            void addAuxVolume(std::shared_ptr<Volume>& volume) { m_auxVolumes.push_back(volume); }

            void setIFD(int ifd) { m_ifd = ifd; }
            int getIFD() const { return m_ifd; }

            void setDefaultColor(int color) { m_defaultColor = color; }
            int getDefaultColor() const { return m_defaultColor; }

            int getDimensionOrder(Dimensions dim) const override { return m_dimensionOrder[dimensionIndex(dim)]; } 
            void setDimensionOrder(Dimensions dim, int value) { m_dimensionOrder[dimensionIndex(dim)] = value; }

            const Resolution& getResolution() const { return m_resolution; }
            void setResolution(const Resolution& resolution) { m_resolution = resolution; }
            void setZResolution(double res) { m_zResolution = res; }
            double getZResolution() const { return m_zResolution; }
            // Time in a Volume is always in seconds. A raw value arrives with the unit
            // the file stated and is converted here; if that unit cannot be read the
            // number means nothing and is not stored, rather than kept to be scaled by
            // a guess later.
            void setTResolution(double raw, const std::string& unit);
            double getTResolution() const { return m_tResolution; }
            void setChannelName(int channelIndex, const std::string& channelName);
            std::string getChannelName(int channelIndex) const;

            // Channel display color extracted from VSI metadata (tag STACK_DISPLAY_LUT
            // gradient endpoint or DISPLAY_COLOR). Stored as 0x00RRGGBB. Sentinel
            // 0xFFFFFFFF means "no color stored for this channel".
            void setChannelColor(int channelIndex, uint8_t r, uint8_t g, uint8_t b);
            bool hasChannelColor(int channelIndex) const;
            void getChannelColor(int channelIndex, uint8_t& r, uint8_t& g, uint8_t& b) const;

            // Per-channel emission wavelength in nanometres (tag CHANNEL_WAVELENGTH).
            // 0.0 means "not set".
            void setChannelEmissionWavelength(int channelIndex, double nm);
            double getChannelEmissionWavelength(int channelIndex) const;

            // Converted to seconds on the way in, like the T resolution above.
            // An unreadable unit stores nothing, so a count of 0 means "no usable
            // timestamps" and every stored value is in seconds.
            void setPlaneTimestamps(const std::vector<double>& raw, const std::string& unit);
            int getPlaneTimestampCount() const;
            double getPlaneTimestampByIndex(int index) const;

            // Acquisition start as a Unix epoch in seconds; 0 when the file records none.
            void setAcquisitionTime(int64_t epochSeconds) { m_acquisitionTime = epochSeconds; }
            int64_t getAcquisitionTime() const { return m_acquisitionTime; }

			const bool isValid() const {
				return m_size.height>0 && m_size.width>0;
			}

        private:
            static int dimensionIndex(Dimensions dim) {
                return static_cast<int>(dim);
            }
        private:
            std::string m_name;
            double m_magnification = 0.;
            StackType m_type = StackType::UNKNOWN;
            cv::Size m_size = {};
            int m_bitDepth = 0;
            bool m_hasExternalFile = false;
            int m_ifd = -1;
            std::vector<std::shared_ptr<Volume>> m_auxVolumes;
            int m_defaultColor = 0;
            // Filled by the constructor: a braced initialiser would set only the
            // first element and value-initialise the rest to 0.
            int m_dimensionOrder[MAX_DIMENSIONS];
            Resolution m_resolution;
            double m_zResolution = 0.;
            double m_tResolution = 0.;   // seconds
            std::vector<std::string> m_channelNames;
            static constexpr uint32_t kNoChannelColor = 0xFFFFFFFFu;
            std::vector<uint32_t> m_channelColors;
            std::vector<double> m_channelEmissionWavelengths;
            std::vector<double> m_planeTimestamps;   // seconds
            int64_t m_acquisitionTime = 0;
        };

    };
};