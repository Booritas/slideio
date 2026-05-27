// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/pke/pke_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include <map>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class PKESlide;
}

//std::ostream& operator << (std::ostream& os, const slideio::PKESlide& slide);

namespace slideio
{
    class SLIDEIO_PKE_EXPORTS PKESlide : public slideio::CVSlide
    {
    protected:
        PKESlide();
    public:
        virtual ~PKESlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        static std::shared_ptr<PKESlide> openFile(const std::string& filePath, const std::string& driverId);
        static std::shared_ptr<PKESlide> openFile(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId);
        static void closeFile(libtiff::TIFF* hfile);
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        void log();
    private:
        // Shared implementation for both path- and stream-based opens.
        // `tiff` is an already-open handle; `identifier` is used for m_filePath,
        // scene file paths and logging; `stream` (may be null) lets stream-opened
        // scenes reopen the TIFF lazily.
        static std::shared_ptr<PKESlide> openFile(libtiff::TIFF* tiff,
                                                  const std::string& identifier,
                                                  const std::string& driverId,
                                                  std::shared_ptr<RandomAccessStream> stream);
        std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        // Held so the stream-backed handles (scene/aux) always have a live stream
        // to call back into; null for local-path opens.
        std::shared_ptr<RandomAccessStream> m_stream;
        std::string m_filePath;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif