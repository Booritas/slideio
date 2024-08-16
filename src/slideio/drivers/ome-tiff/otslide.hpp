// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include <map>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

/*
namespace slideio
{
    namespace ometiff
    {
        class OTSlide;
    }
}
*/

//std::ostream& operator << (std::ostream& os, const slideio::OTSlide& slide);

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTSlide : public slideio::CVSlide
        {
        protected:
            OTSlide();
        public:
            virtual ~OTSlide();
            int getNumScenes() const override;
            std::string getFilePath() const override;
            std::shared_ptr<slideio::CVScene> getScene(int index) const override;
            static std::shared_ptr<OTSlide> openFile(const std::string& path);
            static void closeFile(libtiff::TIFF* hfile);
            std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
            void log();
        private:
            std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
            std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
            std::string m_filePath;
        };
    }
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif