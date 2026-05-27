// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_scnslide_HPP
#define OPENCV_slideio_scnslide_HPP

#include "scnscene.hpp"
#include "slideio/drivers/scn/scn_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_SCN_EXPORTS SCNSlide : public slideio::CVSlide
    {
        friend class SCNImageDriver;
    protected:
        SCNSlide(const std::string& filePath, const std::string& driverId);
        SCNSlide(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId);
        void init();
        void constructScenes();
    public:
        // Factories: open by local path or by stream (remote URIs). The stream
        // variant keeps the stream alive for the slide lifetime so every
        // stream-backed TIFF handle (slide + scenes) stays valid.
        static std::shared_ptr<SCNSlide> openFile(const std::string& path, const std::string& driverId);
        static std::shared_ptr<SCNSlide> openFile(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId);
        virtual ~SCNSlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
    private:
        std::vector<std::shared_ptr<slideio::SCNScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        // Held so the stream-backed handles (m_tiff and the scene/aux handles)
        // always have a live stream to call back into; null for local-path opens.
        std::shared_ptr<RandomAccessStream> m_stream;
        std::string m_filePath;
        TIFFKeeper m_tiff;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif