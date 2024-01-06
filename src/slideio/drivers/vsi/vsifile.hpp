// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <boost/json.hpp>
#include "vsitools.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/vsi/volume.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    namespace vsi
    {
        class VSIStream;
        class EtsFile;

        class SLIDEIO_VSI_EXPORTS VSIFile
        {
        public:
            void extractVolumesFromMetadata();
            VSIFile(const std::string& filePath);
            std::shared_ptr<vsi::EtsFile> getEtsFile(int index) const {
                return m_etsFiles[index];
            }
            int getNumEtsFiles() const {
                return static_cast<int>(m_etsFiles.size());
            }
            std::string getRawMetadata() const;
            void assignAuxImages();

            int getNumTiffDirectories() const {
                return static_cast<int>(m_directories.size());
            }
            const TiffDirectory& getTiffDirectory(int index) {
                return m_directories[index];
            }
            int getNumVolumes() const {
                return static_cast<int>(m_volumes.size());
            }
            std::shared_ptr<Volume> getVolume(int index) const {
                return m_volumes[index];
            }
        private:
            void read();
            bool readMetadata(VSIStream& vsiStream, boost::json::object& parent);
            void checkExternalFilePresence();
            void readVolumeInfo();
            void readExternalFiles();
            void readExtendedType(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo, boost::json::object& tagObject);
        private:
            std::vector<std::shared_ptr<vsi::EtsFile>> m_etsFiles;
            bool m_hasExternalFiles = false;
            int m_numChannels = 0;
            int m_numSlices = 0;
            std::string m_filePath;
            boost::json::object m_metadata;
            std::vector<TiffDirectory> m_directories;
            std::vector<std::shared_ptr<Volume>> m_volumes;
        };
    }
}

