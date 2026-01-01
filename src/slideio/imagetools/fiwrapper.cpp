// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/fiwrapper.hpp"
#include "slideio/base/log.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include <filesystem>
#include <FreeImage.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>

#include "tifftools.hpp"


using namespace slideio;
using json = nlohmann::json;

namespace {
    void addTag(json& root, FITAG* tag) {
        if (!tag)
            return;
        const char* key = FreeImage_GetTagKey(tag);
        const char* desc = FreeImage_GetTagDescription(tag);
        const WORD  id = FreeImage_GetTagID(tag);
        const WORD  type = FreeImage_GetTagType(tag);
        const DWORD count = FreeImage_GetTagCount(tag);
        const void* value = FreeImage_GetTagValue(tag);

        if (!value || count == 0 || !key) {
            return;
        }

        json tagObj = json::object();

        switch (type) {
        case FIDT_BYTE: {
            const uint8_t* data = static_cast<const uint8_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_ASCII: {
            tagObj["value"] = static_cast<const char*>(value);
            break;
        }
        case FIDT_SHORT: {
            const uint16_t* data = static_cast<const uint16_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_LONG: {
            const uint32_t* data = static_cast<const uint32_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_RATIONAL: {
            const uint32_t* data = static_cast<const uint32_t*>(value);
            if (count == 1) {
                tagObj["value"] = std::to_string(data[0]) + "/" + std::to_string(data[1]);
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
					const int i2 = 2 * static_cast<int>(i);
                    arr.push_back(std::to_string(data[i2]) + "/" + std::to_string(data[i2 + 1]));
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_SBYTE: {
            const int8_t* data = static_cast<const int8_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_UNDEFINED: {
            const uint8_t* data = static_cast<const uint8_t*>(value);
            std::string strValue;
            for (DWORD i = 0; i < count && i < 16; ++i) {
                if (i > 0) strValue += " ";
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X", data[i]);
                strValue += hex;
            }
            if (count > 16) {
                strValue += "...";
            }
            tagObj["value"] = strValue;
            break;
        }
        case FIDT_SSHORT: {
            const int16_t* data = static_cast<const int16_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_SLONG: {
            const int32_t* data = static_cast<const int32_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_SRATIONAL: {
            const int32_t* data = static_cast<const int32_t*>(value);
            if (count == 1) {
                tagObj["value"] = std::to_string(data[0]) + "/" + std::to_string(data[1]);
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
					int i2 = 2 * static_cast<int>(i);
                    arr.push_back(std::to_string(data[i2]) + "/" + std::to_string(data[i2 + 1]));
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_FLOAT: {
            const float* data = static_cast<const float*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_DOUBLE: {
            const double* data = static_cast<const double*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_IFD: {
            const uint32_t* data = static_cast<const uint32_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_LONG8: {
            const uint64_t* data = static_cast<const uint64_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_SLONG8: {
            const int64_t* data = static_cast<const int64_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        case FIDT_IFD8: {
            const uint64_t* data = static_cast<const uint64_t*>(value);
            if (count == 1) {
                tagObj["value"] = data[0];
            }
            else {
                json arr = json::array();
                for (DWORD i = 0; i < count; ++i) {
                    arr.push_back(data[i]);
                }
                tagObj["value"] = arr;
            }
            break;
        }
        default:
            tagObj["value"] = "(unknown type)";
            break;
        }

        if (desc && desc[0] != '\0') {
            tagObj["description"] = desc;
        }

        tagObj["id"] = id;
        root[key] = tagObj;
    }
}

FIWrapper::Page::Page(FIWrapper* parent, int pageIndex) {
	m_parent = parent;
    m_hFile = parent->getFIHandle();
    m_pageIndex = pageIndex;
    m_pBitmap = FreeImage_LockPage(m_hFile, m_pageIndex);
    m_fiFormat = parent->getFIFormat();
    if (!m_pBitmap) {
        RAISE_RUNTIME_ERROR << "FIWrapper::FIPage: Failed to lock page " << pageIndex;
    }
	preparePage();
}

FIWrapper::Page::Page(FIWrapper* parent, FIBITMAP* bitmap) {
    m_parent = parent;
    m_hFile = parent->getFIHandle();
    m_pageIndex = 0;
    m_pBitmap = bitmap;
    m_fiFormat = parent->getFIFormat();
    preparePage();
}

FIWrapper::Page::~Page() {
    if (m_pBitmap && m_hFile) {
        FreeImage_UnlockPage(m_hFile, m_pBitmap, FALSE);
        m_pBitmap = nullptr;
    }
}

Size FIWrapper::Page::getSize() const {
    int width = static_cast<int>(FreeImage_GetWidth(m_pBitmap));
    int height = static_cast<int>(FreeImage_GetHeight(m_pBitmap));
    return {width, height};
}

Compression FIWrapper::Page::extractTiffCompression() {
    if (m_fiFormat != FIF_TIFF) {
		RAISE_RUNTIME_ERROR << "FIWrapper::Page::extractTiffCompression: Not a TIFF format";
    }
	const TiffDirectory& dir = m_parent->getTiffDirectory(m_pageIndex);
	return dir.slideioCompression;
}

void FIWrapper::Page::detectCompression() {
    switch (m_fiFormat) {
    case FIF_TIFF:
        m_compression = extractTiffCompression();
        break;
    case FIF_JPEG:
        m_compression = Compression::Jpeg;
        break;
    case FIF_PNG:
        m_compression = Compression::Png;
        break;
    case FIF_GIF:
        m_compression = Compression::GIF;
        break;
    case FIF_BMP:
        m_compression = Compression::Uncompressed;
        break;
    case FIF_WEBP:
        m_compression = Compression::VP8;
        break;
    case FIF_JP2:
    case FIF_J2K:
        m_compression = Compression::Jpeg2000;
        break;
    default:
        m_compression = Compression::Unknown;
        break;
    }
}

void FIWrapper::Page::detectNumChannels() {
    FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(m_pBitmap);
    unsigned bpp = FreeImage_GetBPP(m_pBitmap);
    FREE_IMAGE_COLOR_TYPE colorType = FreeImage_GetColorType(m_pBitmap);
    // Standard bitmap types
    if (imageType == FIT_BITMAP) {
        switch (bpp) {
        case 1:
        case 4:
        case 8:
            if (colorType == FIC_MINISBLACK || colorType == FIC_MINISWHITE) {
                m_numChannels = 1; // Grayscale
            }
            else if (colorType == FIC_PALETTE) {
                m_numChannels = 3; // Palette (treated as RGB)
            }
            m_numChannels = 1; // Default to 1 for 8-bit
            break;
        case 16:
            // Could be RGB555, RGB565, or grayscale
            if (FreeImage_GetRedMask(m_pBitmap) != 0) {
                m_numChannels = 3; // RGB
            }
            else {
                m_numChannels = 1; // Grayscale
            }
            break; // 16-bit grayscale
        case 24:
            m_numChannels = 3; // RGB
            break;
        case 32:
            m_numChannels = 4; // RGBA
            break;
        }
    }
    // Special types
    else if (imageType == FIT_RGB16 || imageType == FIT_RGBF || imageType == FIT_RGBAF) {
        m_numChannels = 3; // RGB (16-bit per channel, float, etc.)
    }
    else if (imageType == FIT_RGBA16 || imageType == FIT_RGBAF) {
        m_numChannels = 4; // RGBA
    }
    else if (imageType == FIT_UINT16 || imageType == FIT_INT16 ||
        imageType == FIT_UINT32 || imageType == FIT_INT32 ||
        imageType == FIT_FLOAT || imageType == FIT_DOUBLE) {
        m_numChannels = 1; // Single channel (grayscale variants)
    }
    else if (imageType == FIT_COMPLEX) {
        m_numChannels = 2; // Complex (real + imaginary)
    }
}

void FIWrapper::Page::detectDataType() {
    switch (FREE_IMAGE_TYPE fit = FreeImage_GetImageType(m_pBitmap)) {
    case FIT_BITMAP:
        m_dataType = DataType::DT_Byte;
        break;
    case FIT_UINT16:
        m_dataType = DataType::DT_UInt16;
        break;
    case FIT_INT16:
        m_dataType = DataType::DT_Int16;
        break;
    case FIT_INT32:
        m_dataType = DataType::DT_Int32;
        break;
    case FIT_UINT32:
        m_dataType = DataType::DT_UInt32;
        break;
    case FIT_FLOAT:
        m_dataType = DataType::DT_Float32;
        break;
    case FIT_DOUBLE:
        m_dataType = DataType::DT_Float64;
        break;
    case FIT_RGB16:
    case FIT_RGBA16:
        m_dataType = DataType::DT_UInt16;
        break;
    case FIT_RGBF:
    case FIT_RGBAF:
        m_dataType = DataType::DT_Float32;
        break;
    default:
		RAISE_RUNTIME_ERROR << "FIWrapper::Page::detectDataType: Unsupported FreeImage type: " << fit;
    }
}



void FIWrapper::Page::preparePage() {
    detectDataType();
    detectNumChannels();
    detectMetadata();
	detectCompression();
}

void FIWrapper::Page::readRaster(cv::OutputArray raster) {
    if (!m_pBitmap) {
        RAISE_RUNTIME_ERROR << "FIWrapper::Page::readRaster: Invalid bitmap";
    }

    const Size size = getSize();
    const slideio::DataType dt = getDataType();
    const int numChannels = getNumChannels();
    const int cvType = slideio::CVTools::toOpencvType(dt);

    raster.create(size.height, size.width, CV_MAKETYPE(cvType, numChannels));
    cv::Mat& mat = raster.getMatRef();

    const unsigned pitch = FreeImage_GetPitch(m_pBitmap);
    const unsigned lineSize = FreeImage_GetLine(m_pBitmap);
    BYTE* bits = FreeImage_GetBits(m_pBitmap);

    if (!bits) {
        RAISE_RUNTIME_ERROR << "FIWrapper::Page::readRaster: Cannot get bitmap bits";
    }

    // FreeImage stores images upside down, we need to flip rows
    for (int y = 0; y < size.height; ++y) {
        // Calculate the source and destination rows
        // FreeImage: row 0 is bottom, we want: row 0 is top
        int srcRow = size.height - 1 - y;
        BYTE* srcLine = bits + srcRow * pitch;
        BYTE* dstLine = mat.ptr<BYTE>(y);
        // Copy the line
        std::memcpy(dstLine, srcLine, lineSize);
    }
    // convert BGR(A) to RGB(A) if needed
    if (mat.channels() == 3 && dt == DataType::DT_Byte) {
        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
    }
    else if (mat.channels() == 4 && dt == DataType::DT_Byte) {
        cv::cvtColor(mat, mat, cv::COLOR_BGRA2RGBA);
    }
}

Resolution FIWrapper::Page::getResolution() const {
    double pixPerMX = FreeImage_GetDotsPerMeterX(m_pBitmap);
    double pixPerMY = FreeImage_GetDotsPerMeterY(m_pBitmap);
    double metersPerPixelX(0), metersPerPixelY(0);
    if (pixPerMX != 0) {
		metersPerPixelX = 1.0 / pixPerMX;
	}
    if (pixPerMY != 0) {
		metersPerPixelY = 1.0 / pixPerMY;
    }
	return { metersPerPixelX, metersPerPixelY };
}

void FIWrapper::Page::extractTiffMetadata() {
    if (m_fiFormat == FIF_TIFF) {
        const TiffDirectory& dir = m_parent->getTiffDirectory(m_pageIndex);
        json mtdObj = json::object();
        mtdObj["width"] = dir.width;
		mtdObj["height"] = dir.height;
		mtdObj["tiled"] = dir.tiled;
		mtdObj["interleaved"] = dir.interleaved;
        if (dir.tiled) {
            mtdObj["tileWidth"] = dir.tileWidth;
            mtdObj["tileHeight"] = dir.tileHeight;
		} else {
			mtdObj["rowsPerStrip"] = dir.rowsPerStrip;
			mtdObj["stripSize"] = dir.stripSize;
		}
        mtdObj["channels"] = dir.channels;
		mtdObj["dataType"] = dir.dataType;
		mtdObj["compression"] = compressionToString(dir.slideioCompression);
		mtdObj["bitsPerSample"] = dir.bitsPerSample;
		mtdObj["photometric"] = dir.photometric;
		mtdObj["subFileType"] = dir.subFileType;
		mtdObj["subIFDs"] = dir.subdirectories.size();
        if (!dir.description.empty()) {
            mtdObj["Description"] = dir.description;
        }
        if (!dir.software.empty()) {
            mtdObj["Software"] = dir.software;
        }
        m_metadata = mtdObj.dump();
    }
}

void FIWrapper::Page::detectMetadata() {
    if (m_fiFormat == FIF_TIFF) {
        extractTiffMetadata();
    }
    else {
        extractCommonMetadata();
    }
}

void FIWrapper::Page::extractCommonMetadata() {

    static constexpr FREE_IMAGE_MDMODEL models[] = {
            FIMD_EXIF_MAIN,
            FIMD_EXIF_EXIF,
            FIMD_EXIF_GPS,
            FIMD_EXIF_MAKERNOTE,
            FIMD_EXIF_INTEROP,
            FIMD_IPTC,
            FIMD_XMP,
            FIMD_ANIMATION,
            FIMD_COMMENTS,
            FIMD_CUSTOM,
            FIMD_GEOTIFF
    };

    constexpr int numMetadataModels = (int)std::size(models);

    json mtdObj = json::object();

    for (auto model : models) {
        const int numTags = static_cast<int>(FreeImage_GetMetadataCount(model, m_pBitmap));
        if (!numTags) {
            continue;
        }
        FIMETADATA* mdHandle = nullptr;
        FITAG* tag = nullptr;
        mdHandle = FreeImage_FindFirstMetadata(model, m_pBitmap, &tag);
        if (mdHandle) {
            do {
                addTag(mtdObj, tag);
            } while (FreeImage_FindNextMetadata(mdHandle, &tag));
            FreeImage_FindCloseMetadata(mdHandle);
        }
    }
    m_metadata = mtdObj.dump();
}


FIWrapper::FIWrapper(const std::string& filePath) {
    SLIDEIO_LOG(INFO) << "Opening file with FreeImage: " << filePath;

    bool forceUnicode = false;
#if defined(WIN32)
    std::wstring wsPath = Tools::toWstring(filePath);
    if (!std::filesystem::exists(filePath)) {
        forceUnicode = std::filesystem::exists(wsPath);
    }
#endif

#if defined(WIN32)
    if (!std::filesystem::exists(wsPath)) {
        RAISE_RUNTIME_ERROR << "FIWrapper: File " << filePath << " does not exist";
    }
    m_fiFormat = FreeImage_GetFileTypeU(wsPath.c_str(), 0);
    if (m_fiFormat == FIF_UNKNOWN) {
        m_fiFormat = FreeImage_GetFIFFromFilenameU(wsPath.c_str());
    }
#else
    if (readOnly && !std::filesystem::exists(filePath)) {
        RAISE_RUNTIME_ERROR << "FIWrapper: File " << filePath << " does not exist";
    }
    m_fiFormat = FreeImage_GetFileType(filePath.c_str(), 0);
    if (m_fiFormat == FIF_UNKNOWN) {
        m_fiFormat = FreeImage_GetFIFFromFilename(filePath.c_str());
    }
#endif

    if (m_fiFormat == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(m_fiFormat)) {
        RAISE_RUNTIME_ERROR << "FIWrapper: Unsupported file format: " << m_fiFormat;
    }
#if !defined(WIN32)

    if (forceUnicode) {
        RAISE_RUNTIME_ERROR << "FIWrapper: Unexpected error: Cannot force unicode path on non-Windows platform: " << filePath;
	}
#endif

    if (m_fiFormat == FIF_TIFF) {
        TiffTools::scanFile(filePath, m_tiffDirectories);
    }

    if (!forceUnicode && (m_fiFormat == FIF_TIFF || m_fiFormat == FIF_GIF || m_fiFormat == FIF_ICO)) {
        m_hFile = FreeImage_OpenMultiBitmap(m_fiFormat, filePath.c_str(),
            FALSE,      // create_new
            TRUE,       // read_only
            FALSE,      // keep_cache_in_memory
            0);   // flags
        if (!m_hFile) {
            RAISE_RUNTIME_ERROR << "FIWrapper: Failed to open multipage image";
        }
    }
    else {
#if defined(WIN32)
        m_pBitmap = FreeImage_LoadU(m_fiFormat, wsPath.c_str(), 0);
#else
        m_pBitmap = FreeImage_Load(m_fiFormat, filePath.c_str(), 0);
#endif
        std::shared_ptr<Page> pagePtr = std::make_shared<Page>(this, m_pBitmap);
        m_pages[0] = pagePtr;
        if (!m_pBitmap) {
            RAISE_RUNTIME_ERROR << "FIWrapper: Failed to load image";
        }
    }
}

FIWrapper::~FIWrapper() {
    if (m_hFile) {
        m_pages.clear();
        FreeImage_CloseMultiBitmap(m_hFile, 0);
        m_hFile = nullptr;
    }

    if (m_pBitmap) {
        FreeImage_Unload(m_pBitmap);
        m_pBitmap = nullptr;
	}
}

bool FIWrapper::isValid() const {
    return m_hFile != nullptr || m_pBitmap != nullptr;
}

int FIWrapper::getNumPages() const {
    if (m_hFile) {
        return FreeImage_GetPageCount(m_hFile);
	}
	if (m_pBitmap) {
        return 1;
    }
	return 0;
}

SmallImagePage* FIWrapper::readPage(int page) {
    if (page >= getNumPages()) {
        RAISE_RUNTIME_ERROR << "FIWrapper::getPage: Page index out of range: "
            << page << ". Number of pages: " << getNumPages();
    }
    auto it = m_pages.find(page);
    if (it != m_pages.end()) {
        return it->second.get();
    }
    if (m_hFile != nullptr) {
        auto pagePtr = std::make_shared<Page>(this, page);
        m_pages[page] = pagePtr;
        return pagePtr.get();
    }
	RAISE_RUNTIME_ERROR << "FIWrapper::getPage: Unexpected request for image page " << page;
}

const TiffDirectory& FIWrapper::getTiffDirectory(int index) const {
    if (index < 0 || index >= static_cast<int>(m_tiffDirectories.size())) {
        RAISE_RUNTIME_ERROR << "FIWrapper::getTiffDirectory: Index out of range: "
            << index << ". Number of TIFF directories: " << m_tiffDirectories.size();
	}
    return m_tiffDirectories[index];
}

namespace
{
    FREE_IMAGE_FORMAT mapCompressionToImageFormat(Compression compression) {
		FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
        // Map compression to FreeImage format
        switch (compression) {
        case Compression::Jpeg:
            fif = FIF_JPEG;
            break;
        case Compression::Png:
            fif = FIF_PNG;
            break;
        case Compression::Uncompressed:
        case Compression::LZW:
        case Compression::Zlib:
            fif = FIF_TIFF;
            break;
        case Compression::Jpeg2000:
            fif = FIF_JP2;
            break;
        case Compression::GIF:
            fif = FIF_GIF;
            break;
        case Compression::VP8:
            fif = FIF_WEBP;
            break;
        }
        return fif;
    }

    FREE_IMAGE_TYPE defineImageType(const int channels, const int depth, int& bpp) {
        FREE_IMAGE_TYPE fiType = FIT_UNKNOWN;
        bpp = 0;

        if (channels == 1) {
            // Grayscale
            if (depth == CV_8U) {
                fiType = FIT_BITMAP;
                bpp = 8;
            }
            else if (depth == CV_16U) {
                fiType = FIT_UINT16;
                bpp = 16;
            }
            else if (depth == CV_16S) {
                fiType = FIT_INT16;
                bpp = 16;
            }
            else if (depth == CV_32S) {
                fiType = FIT_INT32;
                bpp = 32;
            }
            else if (depth == CV_32F) {
                fiType = FIT_FLOAT;
                bpp = 32;
            }
            else if (depth == CV_64F) {
                fiType = FIT_DOUBLE;
                bpp = 64;
            }
        }
        else if (channels == 3) {
            // RGB
            if (depth == CV_8U) {
                fiType = FIT_BITMAP;
                bpp = 24;
            }
            else if (depth == CV_16U || depth == CV_16S) {
                fiType = FIT_RGB16;
                bpp = 48;
            }
            else if (depth == CV_32F) {
                fiType = FIT_RGBF;
                bpp = 96;
            }
        }
        else if (channels == 4) {
            // RGBA
            if (depth == CV_8U) {
                fiType = FIT_BITMAP;
                bpp = 32;
            }
            else if (depth == CV_16U || depth == CV_16S) {
                fiType = FIT_RGBA16;
                bpp = 64;
            }
            else if (depth == CV_32F) {
                fiType = FIT_RGBAF;
                bpp = 128;
            }
        }
        return fiType;
    }
}


void FIWrapper::writeRaster(const std::string& filePath, Compression compression, const cv::Mat& raster) {
    if (raster.empty()) {
        RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Input raster is empty";
    }

    FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(filePath.c_str());

    if (fif == FIF_UNKNOWN) {
        fif = mapCompressionToImageFormat(compression);
    }

    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsWriting(fif)) {
        RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Cannot detect file format for file: " 
        << filePath
        << ". Cannot map compression to image format: " << compression;
    }

    FIBITMAP* bitmap = nullptr;
    
    const int channels = raster.channels();
    const int width = raster.cols;
    const int height = raster.rows;
    const int depth = raster.depth();

    int bpp = 0;
    FREE_IMAGE_TYPE fiType = defineImageType(channels, depth, bpp);

    if (fiType == FIT_UNKNOWN || bpp == 0) {
        RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Cannot define image type from raster. Channels: "
            << channels << ", Depth: " << depth;
	}

    if (fiType == FIT_BITMAP) {
        bitmap = FreeImage_Allocate(width, height, bpp);
    } else {
        bitmap = FreeImage_AllocateT(fiType, width, height, bpp);
    }

    if (!bitmap) {
        RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Failed to allocate FreeImage bitmap";
    }

    try {
        const unsigned pitch = FreeImage_GetPitch(bitmap);
        const unsigned lineSize = FreeImage_GetLine(bitmap);
        BYTE* bits = FreeImage_GetBits(bitmap);

        if (!bits) {
            FreeImage_Unload(bitmap);
            RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Failed to get bitmap bits";
        }

        // FreeImage stores images upside down, so we need to flip rows
        for (int y = 0; y < height; ++y) {
            // FreeImage: row 0 is bottom, OpenCV: row 0 is top
            int fiRow = height - 1 - y;
            BYTE* dstLine = bits + fiRow * pitch;
            const BYTE* srcLine = raster.ptr<BYTE>(y);
            std::memcpy(dstLine, srcLine, std::min(lineSize, static_cast<unsigned>(raster.step)));
        }

        int flags = 0;
        switch (compression) {
            case Compression::Jpeg:
                flags = JPEG_QUALITYGOOD; // Quality 75
                break;
            case Compression::Png:
                flags = PNG_DEFAULT;
                break;
            case Compression::LZW:
                flags = TIFF_LZW;
                break;
            case Compression::Zlib:
                flags = TIFF_DEFLATE;
                break;
            case Compression::Uncompressed:
                flags = TIFF_NONE;
                break;
            case Compression::Jpeg2000:
                flags = JP2_DEFAULT;
                break;
            default:
                flags = 0;
                break;
        }

        BOOL success = FALSE;
#if defined(WIN32)
        std::wstring wsPath = Tools::toWstring(filePath);
        success = FreeImage_SaveU(fif, bitmap, wsPath.c_str(), flags);
#else
        success = FreeImage_Save(fif, bitmap, filePath.c_str(), flags);
#endif

        FreeImage_Unload(bitmap);
		bitmap = nullptr;

        if (!success) {
            RAISE_RUNTIME_ERROR << "FIWrapper::writeRaster: Failed to save image to " << filePath;
        }

        const int cvType = raster.type() & CV_MAT_DEPTH_MASK;
        const slideio::DataType dt = slideio::CVTools::fromOpencvType(cvType);

        if (raster.channels() == 3 && dt == DataType::DT_Byte) {
            cv::cvtColor(raster, raster, cv::COLOR_RGB2BGR);
        }
        else if (raster.channels() == 4 && dt == DataType::DT_Byte) {
            cv::cvtColor(raster, raster, cv::COLOR_RGBA2BGRA);
        }

        SLIDEIO_LOG(INFO) << "Successfully wrote raster to " << filePath;
    }
    catch (...) {
        if (bitmap) {
            FreeImage_Unload(bitmap);
        }
        throw;
    }
}

