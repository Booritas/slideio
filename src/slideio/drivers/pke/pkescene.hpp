// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/pke/pke_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
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
        void makeSureFileIsOpened();

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
        libtiff::TIFF* getFileHandle();
        // Associate a stream so the scene can reopen the TIFF from it (remote URIs).
        // When null (local-path open), reopen falls back to the file path.
        void setStream(std::shared_ptr<RandomAccessStream> stream) {
            m_stream = std::move(stream);
        }

    protected:
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        DataType m_dataType;
		int m_sceneIndex;
    private:
        // Declared before m_tiffKeeper so it is destroyed AFTER the TIFF handle:
        // the stream-backed handle must call back into a live stream during teardown.
        // Held so the stream-backed TIFF handle always has a live stream to call
        // into; null for local-path opens (m_tiffKeeper opened by path).
        std::shared_ptr<RandomAccessStream> m_stream;
        TIFFKeeper m_tiffKeeper;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
