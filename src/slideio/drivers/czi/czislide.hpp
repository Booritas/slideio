// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_czislide_HPP
#define OPENCV_slideio_czislide_HPP
#include "slideio/drivers/czi/czi_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/drivers/czi/cziscene.hpp"
#include "slideio/drivers/czi/czistructs.hpp"
#include <fstream>


namespace tinyxml2
{
    class XMLNode;
    class XMLElement;
    class XMLDocument;
}

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_CZI_EXPORTS CZISlide : public CVSlide
    {
        friend class CZIImageDriver;
    protected:
        CZISlide(const std::string& filePath);
    public:
        virtual ~CZISlide() override;
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<CVScene> getScene(int index) const override;
        double getMagnification() const { return m_magnification; }
        Resolution getResolution() const { return m_res; }
        double getZSliceResolution() const {return m_resZ;}
        double getTFrameResolution() const {return m_resT;}
        const CZIChannelInfos& getChannelInfo() const { return m_channels; }
        const std::string& getTitle() const { return m_title; }
        void readBlock(uint64_t pos, uint64_t size, std::vector<unsigned char>& data);;
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        void readFileHeader(FileHeader& fileHeader);
        void readSubBlocks(uint64_t pos, uint64_t originPos, std::vector<CZISubBlocks>& sceneBlocks, std::vector<uint64_t>& sceneIds);
        std::shared_ptr<CZIScene> constructScene(uint64_t sceneId, const CZISubBlocks& blocks, bool mainScene = true);
    private:
        void readAttachments();
        void init();
        void readMetadata();
        void readFileHeader();
        void readDirectory();
        void parseMagnification(tinyxml2::XMLNode* root);
        void parseMetadataXmL(const char* xml, size_t dataSize);
        void parseResolutions(tinyxml2::XMLNode* root);
        void parseSizes(tinyxml2::XMLNode* root);
        void createJpgAttachmentScenes(int64_t dataPosition, int64_t dataSize, const std::string& name);
        void parseChannels(tinyxml2::XMLNode* root);
        void createCZIAttachmentScenes(const int64_t dataPos, int64_t dataSize, const std::string& attachmentName);
        void addAuxiliaryImage(const std::string& name, const std::string& type, int64_t position);
		static void updateSegmentHeaderBE(SegmentHeader& header);
		static void updateFileHeaderBE(FileHeader& header);
		static void updateMetadataHeaderBE(MetadataHeader& header);
		static void updateDirectoryHeaderBE(DirectoryHeader& header);
		static void updateDirectoryEntryBE(DirectoryEntryDV& entry);
		static void updateDimensionEntryBE(DimensionEntryDV& entry);
		static void updateSublockHeaderBE(SubBlockHeader& header);
		static void updateAttachmentEntryBE(AttachmentEntry& entry);
		static void updateAttachmentDirectorySegmentDataBE(AttachmentDirectorySegmentData& data);
		static void updateAttachmentDirectorySegmentBE(AttachmentDirectorySegment& segment);
		static void updateAttachmentEntryA1BE(AttachmentEntryA1& entry);
		static void updateAttachmentSegmentDataBE(AttachmentSegmentData& data);
		static void updateAttachmentSegmentBE(AttachmentSegment& segment);
		static void updateDimensionBE(Dimension& dim);
    private:
        std::vector<std::shared_ptr<CZIScene>> m_scenes;
        std::string m_filePath;
        std::ifstream m_fileStream;
        uint64_t m_directoryPosition{};
        uint64_t m_metadataPosition{};
        uint64_t m_attachmentDirectoryPosition;
        // image parameters
        int m_slideXs{};
        int m_slideYs{};
        int m_slideZs{};
        int m_slideTs{};
        int m_slideRs{};
        int m_slideIs{};
        int m_slideSs{};
        int m_slideHs{};
        int m_slideMs{};
        int m_slideBs{};
        int m_slideVs{};
        double m_magnification{};
        Resolution m_res{};
        double m_resZ{};
        double m_resT{};
        CZIChannelInfos m_channels;
        std::string m_title;
        std::map<std::string, std::shared_ptr<CVScene >> m_auxImages;
    };


}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
