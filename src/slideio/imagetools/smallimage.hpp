// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <string>
#include <opencv2/core/mat.hpp>
#include "slideio/base/resolution.hpp"

namespace slideio
{
	class Size;
	enum class DataType;
	enum class Compression;

	class SmallImagePage
	{
	public:
		virtual Size getSize() const = 0;
		virtual DataType getDataType() const = 0;
		virtual int getNumChannels() const = 0;
		virtual Compression getCompression() const = 0;
		virtual const std::string& getMetadata() const = 0;
		virtual void readRaster(cv::OutputArray raster) = 0;
		virtual Resolution getResolution() const {
			return {};
		}
	};
	class SmallImage
	{
	public:
        virtual ~SmallImage() = default;
        virtual int getNumPages() const = 0;
		virtual bool isValid() const = 0;
		virtual SmallImagePage* readPage(int pageIndex) = 0;
		virtual void readImageStack(cv::OutputArray raster);
	};

}
