// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_afislide_HPP
#define OPENCV_slideio_afislide_HPP

#include "slideio/drivers/afi/afi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/base/randomaccessstream.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_AFI_EXPORTS AFISlide : public slideio::CVSlide
    {
    protected:
        AFISlide();
    public:
        using Scenes = std::vector<std::shared_ptr<slideio::CVScene>>;
        using Slides = std::vector<std::shared_ptr<slideio::CVSlide>>;
        using SlidesScenes = std::pair<Slides, Scenes>;
        virtual ~AFISlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        static std::shared_ptr<AFISlide> openFile(const std::string& path, const std::string& driverId);
        static std::shared_ptr<AFISlide> openFile(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId);
        static std::vector<std::string> getFileList(std::string xmlString);
        static SlidesScenes getSlidesScenesFromFiles(const std::vector<std::string>& files, std::string mainFile);
        // Stream-mode counterpart of getSlidesScenesFromFiles: resolves each
        // referenced SVS as a sibling of `baseUri` and opens it over its own stream.
        static SlidesScenes getSlidesScenesFromStreams(const std::vector<std::string>& files, const std::string& baseUri);
    private:
        Slides m_slides;
        Scenes m_scenes;
        std::string m_filePath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif