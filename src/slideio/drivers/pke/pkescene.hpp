// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/pke/pke_api_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /// One libtiff handle. A fresh handle is cheap here because
    /// TiffTools::setCurrentDirectory positions with TIFFSetSubDirectory(offset),
    /// so it jumps straight to the right IFD with no directory walk and no
    /// re-parse of the pyramid -- a context duplicates the descriptor, not the
    /// parsed model.
    class PKEReadContext : public ReadContext
    {
    public:
        explicit PKEReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };

    class SLIDEIO_PKE_EXPORTS PKEScene : public CVScene
    {
    public:
        /**
         * \brief Constructor
         * \param filePath: path to the slide file
         * \param name: scene name
         * \param hfile: tiff file handle of the slide
         */
        PKEScene(const std::string& filePath, int sceneIndex, const std::string& driverId, const std::string& name);
        PKEScene(const std::string& filePath, int sceneIndex, const std::string& driverId, libtiff::TIFF* hFile, const std::string& name);

        virtual ~PKEScene();

        bool supportsConcurrentReads() const override { return true; }

        std::string getFilePath() const override {
            return m_filePath;
        }
        int getSceneIndex() const override {
            return m_sceneIndex;
		}
        const std::string& getDriverId() const override {
            return m_driverId;
        }
        std::string getName() const override {
            return m_name;
        }
        Compression getCompression() const override{
            return m_compression;
        }
        slideio::Resolution getResolution() const override{
            return m_resolution;
        }
        double getMagnification() const override{
            return m_magnification;
        }
        DataType getChannelDataType(int) const override{
            return m_dataType;
        }
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
        }

    protected:
        // The handle pool deliberately lives in the concrete scenes
        // (PKETiledScene, PKESmallScene) rather than here. ~ContextPool blocks
        // until every borrow is returned, but members are destroyed in reverse
        // declaration order and a base's members die after a derived class's --
        // so a pool declared here would only block after the derived state an
        // in-flight read still dereferences (PKETiledScene::m_directories and
        // m_zoomDirectoryIndices, which readTiffTile indexes) had already been
        // freed. Declared last in the concrete scene, the pool blocks first.
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        DataType m_dataType;
		int m_sceneIndex;
        ColorProfile m_colorProfile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
