// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/pke/pke_api_def.hpp"
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

    protected:
        /// Borrows a handle for the duration of one block read. Acquire once per
        /// read and pass the context down through userData -- never re-acquire
        /// mid-read.
        ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }

        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        DataType m_dataType;
		int m_sceneIndex;
    private:
        ContextPool m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
