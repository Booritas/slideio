// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
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
        // The Aperio Date and Time properties of the scene's description, in
        // seconds since the Unix epoch; 0 where the file states none. SVS states
        // one time for the slide and nothing per plane, so hasPlaneTimestamps()
        // keeps the base class's false; see TECH_DEBT #28.
        int64_t getAcquisitionTime() const override { return m_acquisitionTime; }
        // Aperio's "Acquisition Bit Depth", 0 where the description states none.
        // One value covers every channel: it describes the camera behind the
        // samples, not a component of them.
        int getChannelSignificantBits(int channelIndex) const override {
            if (channelIndex < 0 || channelIndex >= getNumChannels()) {
                return 0;
            }
            return m_significantBits;
        }
        // Applied by SVSSlide to an auxiliary scene whose own directory states no
        // time: Aperio repeats the property block on the thumbnail but not on
        // the label or the macro, and all of them belong to the one scan.
        void setAcquisitionTime(int64_t epochSeconds) { m_acquisitionTime = epochSeconds; }
        // Applied by PHTIFFSlide to a philips scene: the value comes from the
        // philips xml, which the slide parses once, not from this directory.
        void setSignificantBits(int bits) { m_significantBits = bits; }
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
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
        int64_t m_acquisitionTime = 0;
        int m_significantBits = 0;
        int m_sceneIndex;
        ColorProfile m_colorProfile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
