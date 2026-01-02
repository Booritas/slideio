// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "smallimage.hpp"
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/base/size.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"

namespace libtiff
{
	struct tiff;
	typedef tiff TIFF;
}

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS SmallTiffWrapper : public SmallImage
	{
    public:
		class SLIDEIO_IMAGETOOLS_EXPORTS SmallTiffPage : public SmallImagePage
		{
		public:
			SmallTiffPage(SmallTiffWrapper* parent, int pageIndex);
			Size getSize() const override;
			DataType getDataType() const override;
			int getNumChannels() const override;
			Compression getCompression() const override;
			const std::string& getMetadata() const override;
			void readRaster(cv::OutputArray raster) override;
			Resolution getResolution() const override;
		private:
			void extractMetadata();
		private:
			SmallTiffWrapper* m_parent;
			int m_pageIndex;
			Size m_size = {};
			DataType m_dataType = DataType::DT_Unknown;
			int m_numChannels;
            std::string m_metadata;
        };
	public:
		SmallTiffWrapper(const std::string& filePath);
		int getNumPages() const override;
		bool isValid() const override;
		SmallImagePage* readPage(int pageIndex) override;
		const TiffDirectory& getDirectory(int dirIndex);
        libtiff::TIFF* getHandle() const;
	private:
		TIFFKeeper m_pTiff;
		std::vector<TiffDirectory> m_directories;
		std::vector<std::shared_ptr<SmallTiffPage>> m_pages;
	};

}