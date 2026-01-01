// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "tifftools.hpp"
#include "slideio/base/size.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/imagetools/SmallImage.hpp"
#include <FreeImage.h>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <opencv2/core/mat.hpp>


struct FIBITMAP;
struct FIMULTIBITMAP;

namespace slideio
{
	class SLIDEIO_IMAGETOOLS_EXPORTS FIWrapper : public SmallImage
	{
	public:
		class SLIDEIO_IMAGETOOLS_EXPORTS Page : public SmallImagePage
		{
		public:
			Page(FIWrapper* parent, int pageIndex);
			Page(FIWrapper* parent, FIBITMAP* bitmap);
			Page(const Page&) = delete;
			Page& operator=(const Page&) = delete;
            Page(Page&& other) noexcept = delete;
			Page& operator=(Page&& other) noexcept = delete;
			virtual ~Page();
			Size getSize() const override;
			DataType getDataType() const override{
				return m_dataType;
			}
            int getNumChannels() const override{
				return m_numChannels;
			}
            const std::string& getMetadata() const override{
				return m_metadata;
			}
			Compression getCompression() const override{
				return m_compression;
			}
			void readRaster(cv::OutputArray) override;
			Resolution getResolution() const override;
		private:
            void detectMetadata();
			void extractCommonMetadata();
			void extractTiffMetadata();
			void preparePage();
			void detectNumChannels();
			void detectDataType();
            Compression extractTiffCompression();
			void detectCompression();
		private:
			FIBITMAP* m_pBitmap = nullptr;
			FIMULTIBITMAP* m_hFile = nullptr;
			int m_pageIndex = -1;
            int m_numChannels = 0;
			DataType m_dataType = DataType::DT_Unknown;
			std::string m_metadata;
			Compression m_compression = Compression::Unknown;
            FREE_IMAGE_FORMAT m_fiFormat = FIF_UNKNOWN;
			FIWrapper* m_parent = nullptr;
        };
	public:
		FIWrapper(const std::string& filePath);
		FIWrapper(const FIWrapper&) = delete;
		FIWrapper& operator=(const FIWrapper&) = delete;
		FIWrapper(FIWrapper&& other) noexcept = delete;
		FIWrapper& operator=(FIWrapper&& other) noexcept = delete;
		virtual ~FIWrapper();
		bool isValid() const override;
		int getNumPages() const override;
		SmallImagePage* readPage(int page) override;
		FIMULTIBITMAP* getFIHandle() const{
			return m_hFile;
		}
		FREE_IMAGE_FORMAT getFIFormat() const {
			return m_fiFormat;
		}
		const std::string& getFilePath() const {
			return m_filePath;
		}
		int getNumTiffDirectories() const {
			return (int)m_tiffDirectories.size();
		}
		const TiffDirectory& getTiffDirectory(int index) const;
        static void writeRaster(const std::string& filePath, Compression compression, const cv::Mat& raster);
    private:
		FIMULTIBITMAP* m_hFile = nullptr;
		FIBITMAP* m_pBitmap = nullptr;
		std::map<int, std::shared_ptr<Page>> m_pages;
		FREE_IMAGE_FORMAT m_fiFormat = FIF_UNKNOWN;
		std::string m_filePath;
        std::vector<TiffDirectory> m_tiffDirectories;
    };
};