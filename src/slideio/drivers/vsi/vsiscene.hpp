// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_vsiscene_hpp
#define OPENCV_slideio_vsiscene_hpp

#include "vsifile.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/drivers/vsi/etsfile.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class VSIFile;
    }

    class SLIDEIO_VSI_EXPORTS VSIScene : public CVScene, public Tiler
    {
    public:
        VSIScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile);

        std::string getFilePath() const override {
            return m_filePath;
        }
        std::string getName() const override {
            return m_name;
        }
        Compression getCompression() const override{
            return m_compression;
        }
        Resolution getResolution() const override{
            return m_resolution;
        }
        double getMagnification() const override{
            return m_magnification;
        }
        DataType getChannelDataType(int channelIndex) const override{
            return m_channelDataType[channelIndex];
        }
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        std::string getChannelName(int channel) const override;
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output) override;
    protected:
        std::string m_filePath;
        std::string m_name;
        std::string m_rawMetadata;
        Compression m_compression;
        Resolution m_resolution;
        double m_magnification;
        cv::Rect m_rect;
        int m_numChannels;
        std::vector<std::string> m_channelNames;
        std::vector<DataType> m_channelDataType;
        std::shared_ptr<vsi::VSIFile> m_vsiFile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
