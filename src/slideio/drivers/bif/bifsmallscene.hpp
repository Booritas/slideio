// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/bif/bif_api_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief an auxiliary image of a BIF slide (label, thumbnail, probability mask): one
     * TIFF directory, tiled or striped, read whole and resized.
     *
     * Deliberately not SVSSmallScene, which would link the bif driver to the svs one.
     * Label images can carry patient identifiers: nothing here logs pixels or puts
     * them in a message.*/
    class SLIDEIO_BIF_EXPORTS BIFSmallScene : public CVScene
    {
    public:
        BIFSmallScene(const std::string& filePath, const std::string& name, const TiffDirectory& dir);
        std::string getFilePath() const override { return m_filePath; }
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override { return m_driverId; }
        std::string getName() const override { return m_name; }
        cv::Rect getRect() const override;
        int getNumChannels() const override { return m_directory.channels; }
        DataType getChannelDataType(int) const override { return m_dataType; }
        Resolution getResolution() const override { return Resolution(0., 0.); }
        double getMagnification() const override { return 0.; }
        Compression getCompression() const override { return m_directory.slideioCompression; }
        ColorProfile getColorProfile() const override { return m_colorProfile; }
        bool supportsConcurrentReads() const override { return true; }
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
            cv::OutputArray output) override;
    private:
        std::string m_filePath;
        std::string m_driverId;
        std::string m_name;
        TiffDirectory m_directory;
        DataType m_dataType;
        ColorProfile m_colorProfile;
        // Declared last on purpose, and it must stay last: ~ContextPool blocks until
        // every borrow is returned, and members die in reverse declaration order, so the
        // pool has to be destroyed before the m_directory an in-flight read uses.
        ContextPool m_contextPool;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
