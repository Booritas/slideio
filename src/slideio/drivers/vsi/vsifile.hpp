// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <boost/json.hpp>
#include "vsiscene.hpp"
#include "vsistruct.hpp"
#include "vsitools.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    namespace vsi
    {
        struct TempData;
        class VSIStream;
        class Pyramid;
        class EtsFile;

        class SLIDEIO_VSI_EXPORTS VSIFile
        {
        public:
            VSIFile(const std::string& filePath);
            int getNumExternalFiles() const {
                return static_cast<int>(m_etsFiles.size());
            }
            int getNumPyramids() const {
                return static_cast<int>(m_pyramids.size());
            }
            std::shared_ptr<Pyramid> getPyramid(int index) const {
                return m_pyramids[index];
            }
            std::shared_ptr<vsi::EtsFile> getEtsFile(int index) const {
                return m_etsFiles[index];
            }
            int getNumEtsFiles() const {
                return static_cast<int>(m_etsFiles.size());
            }

        private:
            void read();
            bool readTags(vsi::VSIStream& vsi, bool populateMetadata, std::string tagPrefix, vsi::TempData& temp);
            bool readMetadata(VSIStream& vsiStream, boost::json::object& parent);
            void readVolumeInfo();
            void readExternalFiles();
            std::string extractTagValue(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo);

        private:
            std::vector<std::shared_ptr<Pyramid>> m_pyramids;
            std::vector<std::shared_ptr<vsi::EtsFile>> m_etsFiles;
            bool m_hasExternalFiles = false;
            int m_numChannels = 0;
            int m_numSlices = 0;
            std::string m_filePath;
            boost::json::object m_metadata;
        };
    }
}

