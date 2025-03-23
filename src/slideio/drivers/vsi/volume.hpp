// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
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
            Volume() {
                m_dimensionOrder[0] = 0;
                m_dimensionOrder[1] = 1;
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
            void setTResolution(double res) { m_tResolution = res; }
            double getTResolution() const { return m_tResolution; }
            void setChannelName(int channelIndex, const std::string& channelName);
            std::string getChannelName(int channelIndex) const;
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
            int m_dimensionOrder[MAX_DIMENSIONS] = {-1};
            Resolution m_resolution;
            double m_zResolution = 0.;
            double m_tResolution = 0.;
            std::vector<std::string> m_channelNames;
        };

    };
};