// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_czislide_HPP
#define OPENCV_slideio_czislide_HPP
#include "slideio/core/slide.hpp"
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
    class SLIDEIO_EXPORTS CZISlide : public Slide
    {
    public:
        CZISlide(const std::string& filePath);
        int getNumbScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<Scene> getScene(int index) const override;
        double getMagnification() const { return m_magnification; }
        Resolution getResolution() const { return m_res; }
        double getZSliceResolution() const {return m_resZ;}
        double getTFrameResolution() const {return m_resT;}
        const CZIChannelInfos& getChannelInfo() const { return m_channels; }
        const std::string& getTitle() const { return m_title; }
        void readBlock(uint64_t pos, uint64_t size, std::vector<unsigned char>& data);;
    private:
        void init();
        void readMetadata();
        void readFileHeader();
        void readDirectory();
        void parseMagnification(tinyxml2::XMLNode* root);
        void parseMetadataXmL(const char* xml, size_t dataSize);
        void parseResolutions(tinyxml2::XMLNode* root);
        void parseSizes(tinyxml2::XMLNode* root);
        void parseChannels(tinyxml2::XMLNode* root);
    private:
        std::vector<std::shared_ptr<CZIScene>> m_scenes;
        std::string m_filePath;
        std::ifstream m_fileStream;
        uint64_t m_directoryPosition{};
        uint64_t m_metadataPosition{};
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
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
