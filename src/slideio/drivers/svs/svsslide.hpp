// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include <map>

#include "slideio/imagetools/tiffkeeper.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SVSSlide;
}


namespace slideio
{
    class SLIDEIO_SVS_EXPORTS SVSSlide : public slideio::CVSlide
    {
    protected:
        SVSSlide();
    public:
        virtual ~SVSSlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        // Unchanged, and used by the afi driver with its own id.
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id);
        static void closeFile(libtiff::TIFF* hfile);
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        void log();
    protected:
        MetadataBuilder buildMetadataTree() const override;
        // Opens the file, scans the directories and hands them to slide->init(). The
        // caller supplies the instance, which is what selects the format.
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id,
                                                  std::shared_ptr<SVSSlide> slide);
        // Builds the scenes from the scanned directories. The keeper owns the tiff handle
        // until the scene that reads from it is created: everything before that point can
        // throw, and a handle nobody owns is a handle nobody closes. Ownership is passed on
        // with TIFFKeeper::release at the one place it is handed to a scene.
        virtual void init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper);
    protected:
        std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
