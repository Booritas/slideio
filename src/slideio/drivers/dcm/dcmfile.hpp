// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmfile_HPP
#define OPENCV_slideio_dcmfile_HPP

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <string>
#include <memory>
#include <opencv2/core.hpp>

#include "slideio/base/slideio_enums.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

class DicomImage;
class DcmDataset;
class DcmFileFormat;
class DcmTagKey;

namespace slideio
{
    enum class EPhotoInterpetation
    {
        PHIN_UNKNOWN,
        PHIN_MONOCHROME1,
        PHIN_MONOCHROME2,
        PHIN_RGB,
        PHIN_PALETTE,
        PHIN_YCBCR,
        PHIN_YBR_FULL,
        PHIN_YBR_422_FULL,
        PHIN_HSV,
        PHIN_ARGB,
        PHIN_CMYK,
        PHIN_YBR_FULL_422,
        PHIN_YBR_PARTIAL_420,
        PHIN_YBR_ICT,
        PHIN_YBR_RCT
    };

    class SLIDEIO_DCM_EXPORTS DCMFile
    {
    public:
        DCMFile(const std::string& filePath);
        void loadFile();
        void init();

        int getWidth() const {
            return m_width;
        }

        int getHeight() const {
            return m_height;
        }

        int getNumSlices() const {
            return m_slices;
        }

        const std::string& getFilePath() const {
            return m_filePath;
        }

        const std::string& getSeriesUID() const {
            return m_seriesUID;
        }

        int getInstanceNumber() const {
            return m_instanceNumber;
        }

        int getNumChannels() const {
            return m_numChannels;
        }

        const std::string& getSeriesDescription() const {
            return m_seriesDescription;
        }

        DataType getDataType() const {
            return m_dataType;
        }

        bool getPlanarConfiguration() const {
            return m_planarConfiguration;
        }

        EPhotoInterpetation getPhotointerpretation() const {
            return m_photoInterpretation;
        }

        Compression getCompression() const {
            return m_compression;
        }

        const std::string& getModality() const {
            return m_modality;
        }

        void logData();
        void readPixelValues(std::vector<cv::Mat>& frames, int startFrame = 0, int numFrames = 1);

        bool isWSIFile() const {
            return m_WSISlide;
        }

        std::string getMetadata();
        static bool isDicomDirFile(const std::string& filePath);
        static bool isWSIFile(const std::string& filePath);

        const cv::Size& getTileSize() const {
            return m_tileSize;
        }

        int getNumFrames() const {
            return m_frames;
        }
        bool getTileRect(int tileIndex, cv::Rect& tileRect) const;
        bool readTile(int tileIndex, cv::OutputArray tileRaster);
    private:
        void extractPixelsPartialy(std::vector<cv::Mat>& frames, int startFrame, int numFrames);
        void extractPixelsWholeFileDecompression(std::vector<cv::Mat>& mats, int startFrame, int numFrames);
        std::shared_ptr<DicomImage> createImage(int firstSlice = 0, int numSlices = 1);
        void initPhotoInterpretaion();
        void defineCompression();
        DcmDataset* getDataset() const;
        DcmDataset* getValidDataset() const;
        bool getIntTag(const DcmTagKey& tag, int& value, int pos = 0) const;
        bool getStringTag(const DcmTagKey& tag, std::string& value) const;
        bool getDblTag(const DcmTagKey& tag, double& value, double defaultValue);

    private:
        std::string m_filePath;
        std::shared_ptr<DcmFileFormat> m_file;
        int m_width = 0;
        int m_height = 0;
        int m_slices = 1;
        int m_instanceNumber = -1;
        std::string m_seriesUID;
        std::string m_seriesDescription;
        int m_numChannels = 0;
        DataType m_dataType = DataType::DT_Unknown;
        bool m_planarConfiguration = false;
        EPhotoInterpetation m_photoInterpretation = EPhotoInterpetation::PHIN_UNKNOWN;
        double m_windowCenter = -1;
        double m_windowWidth = -1;
        double m_rescaleSlope = 1.;
        double m_rescaleIntercept = 0.;
        bool m_useWindowing = false;
        bool m_useRescaling = false;
        Compression m_compression = Compression::Unknown;
        bool m_decompressWholeFile = false;
        int m_bitsAllocated = 0;
        std::string m_modality;
        bool m_WSISlide = false;
        int m_frames = 1;
        cv::Size m_tileSize = {0, 0};
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
