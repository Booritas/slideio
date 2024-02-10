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
#include "slideio/drivers/vsi/taginfo.hpp"

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
            VSIFile(const std::string& filePath);
            std::shared_ptr<vsi::EtsFile> getEtsFile(int index) const {
                return m_etsFiles[index];
            }
            int getNumEtsFiles() const {
                return static_cast<int>(m_etsFiles.size());
            }
            std::string getRawMetadata() const;
            void assignAuxVolumes();

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
            bool hasMetadata() const {
                return !m_metadata.empty();
            }
            bool expectExternalFiles() const {
                return m_expectExternalFiles;
            }
            bool hasExternalFiles() const {
                return !m_etsFiles.empty();
            }
            void getVolumeMetadataItems(std::list<const TagInfo*>& volumes) const;
            static void getImageFrameMetadataItems(const TagInfo* volume, std::list<const TagInfo*>& frames);

        private:
            void read();
            void checkExternalFilePresence();
            static StackType getVolumeStackType(const TagInfo* volume);
            void extractVolumesFromMetadata();
            bool readVolumeHeader(vsi::VSIStream& vsi, vsi::VolumeHeader& volumeHeader);
            bool readMetadata(VSIStream& vsiStream, std::list<TagInfo>& path);
            void serializeMetadata(const TagInfo& tagInfo, boost::json::object& jsonObj) const;
            void readVolumeInfo();
            void readExternalFiles();
            void readExtendedType(vsi::VSIStream& vsi, TagInfo& tag, std::list<TagInfo>& path);
        private:
            std::vector<std::shared_ptr<vsi::EtsFile>> m_etsFiles;
            bool m_expectExternalFiles = false;
            int m_numChannels = 0;
            int m_numSlices = 0;
            std::string m_filePath;
            TagInfo m_metadata;
            std::vector<TiffDirectory> m_directories;
            std::vector<std::shared_ptr<Volume>> m_volumes;
        };
    }
}

