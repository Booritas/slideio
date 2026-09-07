// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
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
    class SVSReadContext : public ReadContext
    {
    public:
        explicit SVSReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "SVSImageDriver: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };

    class SLIDEIO_SVS_EXPORTS SVSScene : public CVScene
    {
    public:
        /**
         * \brief Constructor
         * \param filePath: path to the slide file
         * \param name: scene name
         * \param hfile: tiff file handle of the slide
         */
        SVSScene(const std::string& filePath, const std::string& driverId, const std::string& name);
        SVSScene(const std::string& filePath, const std::string& driverId, libtiff::TIFF* hFile, const std::string& name);

        virtual ~SVSScene();

        bool supportsConcurrentReads() const override { return true; }

        std::string getFilePath() const override {
            return m_filePath;
        }
		void setFilePath(const std::string& filePath) {
            m_filePath = filePath;
        }
        int getSceneIndex() const override {
			return m_sceneIndex;
        }
		const std::string& getDriverId() const override {
            return m_driverId;
		}
		void setSceneIndex(int index) {
			m_sceneIndex = index;
		}
        void setDriverId(const std::string& driverId) {
            m_driverId = driverId;
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
    protected:
        // The handle pool deliberately lives in the concrete scenes
        // (SVSTiledScene, SVSSmallScene) rather than here. ~ContextPool blocks
        // until every borrow is returned, but a member is destroyed in reverse
        // declaration order and a base's members die after a derived class's --
        // so a pool declared here would be destroyed only after the derived
        // state an in-flight readTile is still dereferencing (SVSTiledScene's
        // m_directories, which the borrowed userData points into) had already
        // been freed. Declared last in the concrete scene, the pool blocks
        // first and that state outlives the read.
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        DataType m_dataType;
        int m_sceneIndex;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
