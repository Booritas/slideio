// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include <map>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    class SVSSlide;
}

//std::ostream& operator << (std::ostream& os, const slideio::SVSSlide& slide);

namespace slideio
{
    class SLIDEIO_SVS_EXPORTS SVSSlide: public slideio::CVSlide
    {
    protected:
        SVSSlide();

    public:
        virtual ~SVSSlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id);
        static void closeFile(libtiff::TIFF* hfile);
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        void log();

    protected:
        MetadataBuilder buildMetadataTree() const override;

    private:
        std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
    };
} // namespace slideio

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
