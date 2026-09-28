// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/slideio_enums.hpp"
#include <string>
#include <memory>
#include <optional>
#include <opencv2/core.hpp>

#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/resolution.hpp"
#include "slideio/core/colorprofile.hpp"

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

        /**@brief BitsStored (0028,0101), 0 where the file states none.
         *
         * How many of the BitsAllocated bits carry data: a 12 bit detector kept in
         * 16 bit samples states 12. Bio-Formats reports BitsAllocated instead
         * (DicomReader, BITS_ALLOCATED -> bitsPerPixel), which is the storage
         * width getChannelDataType() already gives.*/
        int getBitsStored() const {
            return m_bitsStored;
        }

        /**@brief AcquisitionDateTime (0008,002A), or AcquisitionDate (0008,0022)
         * with AcquisitionTime (0008,0032), as seconds since the Unix epoch.
         *
         * When the acquisition of the object started, which is the origin
         * CVScene::getPlaneTimestamp() measures from.*/
        const std::optional<double>& getAcquisitionTime() const {
            return m_acquisitionTime;
        }

        /**@brief ContentDate (0008,0023) with ContentTime (0008,0033), as seconds
         * since the Unix epoch.
         *
         * When this object's pixel data was created, which for a series of one
         * file per slice is the one value that differs from slice to slice --
         * AcquisitionTime is commonly written once for the whole series. It is
         * also what Bio-Formats reports as the OME AcquisitionDate for DICOM.*/
        const std::optional<double>& getContentTime() const {
            return m_contentTime;
        }

        /**@brief A DICOM DA plus TM, or a DT alone with an empty time, as
         * seconds since 1970-01-01T00:00:00Z; nullopt if it cannot be read.
         *
         * Only a DT states an offset of its own. For everything else the
         * instance states one in TimezoneOffsetFromUTC (0008,0201), which is
         * what @p zoneOffset carries; where neither names one the value is read
         * as UTC, because assuming the reader's zone would make one file yield
         * different instants on different machines. Static and public so the
         * formats can be tested directly -- the corpus holds both the current
         * spelling and the dotted, colonned one DICOM's predecessor allowed.
         * @param zoneOffset : "&ZZXX" or "&ZZ:XX", applied only where the value
         * states no offset itself; empty means read as UTC.*/
        /**@brief BitsStored as a significant bit count, or 0 where the pair cannot
         * be one.
         *
         * BitsStored counts how many of the BitsAllocated bits carry data, so a
         * file stating more of the one than it allocated of the other contradicts
         * itself and reports unknown. Clamping to the allocated width instead
         * would be indistinguishable from a file saying every stored bit is
         * significant, which cvscene.hpp reserves 0 to avoid. Static and public
         * so the rule can be tested -- no corpus file states such a pair.
         * @param bitsAllocated : 0 where the width is not known, which disables
         * the check rather than failing it.*/
        static int significantBits(int bitsStored, int bitsAllocated);

        static std::optional<double> dicomDateTimeToEpochSeconds(const std::string& date,
                                                                const std::string& time,
                                                                const std::string& zoneOffset = std::string());

        /**@brief The time a DT tag and a DA/TM pair state between them, or
         * nullopt where neither can be read.
         *
         * The DT is preferred where it reads -- it is the more specific
         * statement -- but a DT the reader cannot follow, a date with no time
         * among them, falls through to the pair rather than suppressing it: an
         * enhanced object states both, and the pair is often the readable one.
         * Static and public so the choice can be tested without a file that
         * states an unreadable DT, which the corpus does not hold.*/
        static std::optional<double> timeFromTags(const std::string& dateTime,
                                                  const std::string& date,
                                                  const std::string& time,
                                                  const std::string& zoneOffset);

        const std::string& getModality() const {
            return m_modality;
        }

        void logData();
        void readPixelValues(std::vector<cv::Mat>& frames, int startFrame = 0, int numFrames = 1);

        bool isWSIFile() const {
            return m_WSISlide;
        }

        std::string getMetadata();
        double getMagnification() const {
            return m_magnification;
        }
        const Resolution& getResolution() const {
            return m_resolution;
        }
        static bool isDicomDirFile(const std::string& filePath);
        static bool isWSIFile(const std::string& filePath);

        const cv::Size& getTileSize() const {
            return m_tileSize;
        }

        int getNumFrames() const {
            return m_frames;
        }
        bool getTileRect(int tileIndex, cv::Rect& tileRect) const;
        bool readFrame(int tileIndex, cv::OutputArray tileRaster);
        double getScale() const {
            return m_scale;
        }
        void setScale(double scale) {
            m_scale = scale;
        }
        bool isTiled() const {
            return m_bTiled;
        }
        const std::string& getImageType() const {
            return m_imageType;
        }
        bool isAuxImage() const {
            return m_imageType != "VOLUME";
        }

        /**@brief reads ICC Profile (0028,2000).
         *
         * Looked for first inside Optical Path Sequence (0048,0105), where
         * DICOM WSI places it, then at dataset level as a fallback for
         * non-WSI objects that carry it directly.*/
        ColorProfile readColorProfile() const;
    private:
        void readFrames(std::vector<cv::Mat>& frames, int startFrame, int numFrames);
        void extractPixelsWholeFileDecompression(std::vector<cv::Mat>& mats, int startFrame, int numFrames);
        std::shared_ptr<DicomImage> createImage(int firstSlice = 0, int numSlices = 1);
        void initPhotoInterpretaion();
        void readTimes();
        void defineCompression();
        DcmDataset* getDataset() const;
        DcmDataset* getValidDataset() const;
        bool getIntTag(const DcmTagKey& tag, int& value, int pos = 0) const;
        bool getStringTag(const DcmTagKey& tag, std::string& value) const;
        bool getStringTag(const DcmTagKey& tag, int index, std::string& value) const;
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
        int m_bitsStored = 0;
        std::optional<double> m_acquisitionTime;
        std::optional<double> m_contentTime;
        std::string m_modality;
        bool m_WSISlide = false;
        int m_frames = 1;
        cv::Size m_tileSize = {0, 0};
        double m_magnification = 0.;
        Resolution m_resolution = { 0., 0. };
        double m_scale = 1.;
        bool m_bTiled = false;
        std::string m_imageType;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
