// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_VSIslide_HPP
#define OPENCV_slideio_VSIslide_HPP

#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/vsi/pyramid.hpp"
#include "slideio/drivers/vsi/vsistream.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        struct TempData;
    }

    class SLIDEIO_VSI_EXPORTS VSISlide : public slideio::CVSlide
    {
        friend class VSIImageDriver;
    protected:
        VSISlide(const std::string& filePath);
    public:
        virtual ~VSISlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
    private:
        std::string getStackType(const std::string& value);
        std::string getDeviceSubtype(const std::string& value);
        void addMetaList(const std::string& basicString, const std::string& value, const std::string& originalMetadata);
        void addGlobalMetaList(const std::string& basicString, const std::string& value);
        bool readTags(vsi::VSIStream& vsi, bool populateMetadata, std::string tagPrefix, vsi::TempData& temp);
        void readVolumeInfo();
        void init();
        std::string getVolumeName(int32_t tag);
        std::string getTagName(int32_t tag);
    private:
        std::vector<std::shared_ptr<slideio::VSIScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
        std::vector<vsi::Pyramid> m_pyramids;
        bool expectETS;
        int channelCount = 0;
        int zCount = 0;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif