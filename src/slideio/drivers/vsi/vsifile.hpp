// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "vsistruct.hpp"
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
            const Pyramid& getPyramid(int index) const {
                return m_pyramids[index];
            }
        private:
            void read();
            bool readTags(vsi::VSIStream& vsi, bool populateMetadata, std::string tagPrefix, vsi::TempData& temp);
            void readVolumeInfo();
            void readExternalFiles();
            void init();
        private:
            std::vector<vsi::Pyramid> m_pyramids;
            std::vector<std::shared_ptr<vsi::EtsFile>> m_etsFiles;
            bool m_hasExternalFiles = false;
            int m_numChannels = 0;
            int m_numSlices = 0;
            std::string m_filePath;
        };
    }
}

