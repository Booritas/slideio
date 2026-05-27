// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include <map>
#include <memory>
#include <tinyxml2.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    namespace ometiff
    {
        struct ImageData;
        class SLIDEIO_OMETIFF_EXPORTS OTSlide : public slideio::CVSlide
        {
        protected:
            OTSlide();
        public:
            ~OTSlide() override;
            int getNumScenes() const override;
            std::string getFilePath() const override;
            std::shared_ptr<slideio::CVScene> getScene(int index) const override;
			static std::shared_ptr<OTSlide> createSlide(const std::string& filePath, const std::string& driverId,
                                                            std::shared_ptr<tinyxml2::XMLDocument> doc,
                                                            std::shared_ptr<RandomAccessStream> stream = nullptr);
            static std::shared_ptr<OTSlide> openFile(const std::string& path, const std::string& driverId);
            static std::shared_ptr<OTSlide> openFile(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId);
			static std::shared_ptr<CVScene> createScene(const ImageData& imageData, int sceneIndex, const std::string& driverId);
            static void closeFile(libtiff::TIFF* hfile);
            std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
            void log();
        private:
            // Shared implementation for both path- and stream-based opens. `tiff` is
            // an already-open handle; `filePath` is the identifier (path or URI) used
            // for scene file paths, companion-XML resolution and logging; `stream`
            // (may be null) lets stream-opened scenes reopen the TIFF lazily.
            static std::shared_ptr<OTSlide> openFile(libtiff::TIFF* tiff,
                                                     const std::string& filePath,
                                                     const std::string& driverId,
                                                     std::shared_ptr<RandomAccessStream> stream);
            std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
            std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
            // Held so stream-backed scenes always have a live stream; null for path opens.
            std::shared_ptr<RandomAccessStream> m_stream;
            std::string m_filePath;
        };
    }
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif