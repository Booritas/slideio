// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <string>
#include <vector>
#include <boost/json.hpp>

#include "vsistruct.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS Volume
        {
        public:
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

        private:
            std::string m_name;
            double m_magnification = 0.;
            StackType m_type = StackType::UNKNOWN;
            cv::Size m_size = {};
            int m_bitDepth = 0;
            bool m_hasExternalFile;
            int m_ifd = -1;
            std::vector<std::shared_ptr<Volume>> m_auxVolumes;
        };

    };
};