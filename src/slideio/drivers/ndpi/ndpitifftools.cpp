// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <opencv2/core.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <jxrcodec/jxrcodec.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpilibtiff.hpp"
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "slideio/core/cvtools.hpp"
#include "jpeglib.h"

#if defined(WIN32)
#define FSEEK64 _fseeki64
#elif __APPLE__
#define FSEEK64 fseeko
#else
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#define FSEEK64 fseeko64
#endif


using namespace slideio;

static int getCvType(jpegxr_image_info& info)
{
    int type = -1;
    if (info.sample_type == jpegxr_sample_type::Uint) {
        switch (info.sample_size) {
        case 1:
            type = CV_8U;
            break;
        case 2:
            type = CV_16U;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Int) {
        switch (info.sample_size) {
        case 2:
            type = CV_16S;
            break;
        case 4:
            type = CV_32S;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Float) {
        switch (info.sample_size) {
        case 2:
            type = CV_16F;
            break;
        case 4:
            type = CV_32F;
            break;
        }
    }
    if (type < 0) {
        RAISE_RUNTIME_ERROR << "Unsupported type of jpegxr compression: " << (int)info.sample_type;
    }

    return type;
}

static DataType getSlideioType(jpegxr_image_info& info)
{
    DataType type = DataType::DT_Unknown;
    if (info.sample_type == jpegxr_sample_type::Uint) {
        switch (info.sample_size) {
        case 1:
            type = DataType::DT_Byte;
            break;
        case 2:
            type = DataType::DT_UInt16;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Int) {
        switch (info.sample_size) {
        case 2:
            type = DataType::DT_Int16;
            break;
        case 4:
            type = DataType::DT_Int32;
            break;
        }
    }
    else if (info.sample_type == jpegxr_sample_type::Float) {
        switch (info.sample_size) {
        case 2:
            type = DataType::DT_Float16;
            break;
        case 4:
            type = DataType::DT_Float32;
            break;
        }
    }
    if (type == DataType::DT_Unknown) {
        RAISE_RUNTIME_ERROR << "Unsupported type of jpegxr compression: " << (int)info.sample_type;
    }

    return type;
}

static slideio::DataType dataTypeFromTIFFDataType(libtiff::TIFFDataType dt)
{
    switch (dt) {
    case libtiff::TIFF_NOTYPE:
        return DataType::DT_None;
    case libtiff::TIFF_LONG8:
    case libtiff::TIFF_BYTE:
        return DataType::DT_Byte;
    case libtiff::TIFF_ASCII:
        return DataType::DT_None;
    case libtiff::TIFF_SHORT:
        return DataType::DT_UInt16;
    case libtiff::TIFF_SLONG8:
    case libtiff::TIFF_SBYTE:
        return DataType::DT_Int8;
    case libtiff::TIFF_UNDEFINED:
        return DataType::DT_Unknown;
    case libtiff::TIFF_SSHORT:
        return DataType::DT_Int16;
    case libtiff::TIFF_SRATIONAL:
        return DataType::DT_Unknown;
    case libtiff::TIFF_FLOAT:
        return DataType::DT_Float32;
    case libtiff::TIFF_DOUBLE:
        return DataType::DT_Float64;
    case libtiff::TIFF_IFD:
    case libtiff::TIFF_RATIONAL:
    case libtiff::TIFF_IFD8:
    default: ;
        return DataType::DT_Unknown;
    }
}

static slideio::Compression compressTiffToSlideio(int tiffCompression)
{
    Compression compression = Compression::Unknown;
    switch (tiffCompression) {
    case 0x1:
        compression = Compression::Uncompressed;
        break;
    case 0x2:
        compression = Compression::HuffmanRL;
        break;
    case 0x3:
        compression = Compression::CCITT_T4;
        break;
    case 0x4:
        compression = Compression::CCITT_T6;
        break;
    case 0x5:
        compression = Compression::LempelZivWelch;
        break;
    case 0x6:
        compression = Compression::JpegOld;
        break;
    case 0x7:
        compression = Compression::Jpeg;
        break;
    case 0x8:
        compression = Compression::Zlib;
        break;
    case 0x9:
        compression = Compression::JBIG85;
        break;
    case 0xa:
        compression = Compression::JBIG43;
        break;
    case 0x7ffe:
        compression = Compression::NextRLE;
        break;
    case 0x8005:
        compression = Compression::PackBits;
        break;
    case 0x8029:
        compression = Compression::ThunderScanRLE;
        break;
    case 0x807f:
        compression = Compression::RasterPadding;
        break;
    case 0x8080:
        compression = Compression::RLE_LW;
        break;
    case 0x8081:
        compression = Compression::RLE_HC;
        break;
    case 0x8082:
        compression = Compression::RLE_BL;
        break;
    case 0x80b2:
        compression = Compression::PKZIP;
        break;
    case 0x80b3:
        compression = Compression::KodakDCS;
        break;
    case 0x8765:
        compression = Compression::JBIG;
        break;
    case 0x8798:
        compression = Compression::Jpeg2000;
        break;
    case 0x8799:
        compression = Compression::NikonNEF;
        break;
    case 0x879b:
        compression = Compression::JBIG2;
        break;
    case 0x5852:
        compression = Compression::JpegXR;
        break;
    }
    return compression;
}

//std::ostream& operator<<(std::ostream& os, const NDPITiffDirectory& dir)
//{
//    os << "---Tiff directory " << dir.dirIndex << std::endl;
//    os << "width:" << dir.width << std::endl;
//    os << "height:" << dir.height << std::endl;
//    os << "tiled:" << dir.tiled << std::endl;
//    os << "tileWidth:" << dir.tileWidth << std::endl;
//    os << "tileHeight:" << dir.tileHeight << std::endl;
//    os << "channels:" << dir.channels << std::endl;
//    os << "bitsPerSample:" << dir.bitsPerSample << std::endl;
//    os << "photometric:" << dir.photometric << std::endl;
//    os << "YcbCrSubsampling:" << dir.YCbCrSubsampling[0] << "," << dir.YCbCrSubsampling[1] << std::endl;
//    os << "compression:" << dir.compression << std::endl;
//    os << "slideioCompression" << dir.slideioCompression << std::endl;
//    os << "offset:" << dir.offset << std::endl;
//    os << "description" << dir.description << std::endl;
//    os << "Number of subdirectories:" << dir.subdirectories.size() << std::endl;
//    os << "Resoulution:" << dir.res.x << "," << dir.res.y << std::endl;
//    os << "interleaved:" << dir.interleaved << std::endl;
//    os << "rowsPerStrip:" << dir.rowsPerStrip << std::endl;
//    os << "dataType:" << dir.dataType << std::endl;
//    os << "stripSize:" << dir.stripSize << std::endl;
//
//    for(size_t subDirIndex=0; subDirIndex<dir.subdirectories.size(); ++subDirIndex) {
//        os << "--Sub-directory:" << subDirIndex << std::endl;
//        os << dir.subdirectories[subDirIndex];
//    }
//    return os;
//}
//
//std::ostream& operator<<(std::ostream& os, const std::vector<slideio::NDPITiffDirectory>& dirs) {
//    os << "---Tiff directories. Size:" << dirs.size() << std::endl;
//    std::vector<slideio::NDPITiffDirectory>::const_iterator iDir;
//    for (iDir = dirs.begin(); iDir != dirs.end(); ++iDir) {
//        auto& dir = *iDir;
//        os << dir;
//    }
//    return os;
//}

libtiff::TIFF* slideio::NDPITiffTools::openTiffFile(const std::string& path)
{
    namespace fs = boost::filesystem;
    boost::filesystem::path filePath(path);
    if (!fs::exists(filePath)) {
        RAISE_RUNTIME_ERROR << "File " << path << " does not exist";
    }
    return libtiff::TIFFOpen(path.c_str(), "r");
}

void slideio::NDPITiffTools::closeTiffFile(libtiff::TIFF* file)
{
    libtiff::TIFFClose(file);
}

void slideio::NDPITiffTools::scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset,
                                             slideio::NDPITiffDirectory& dir)
{
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags-begin " << dirIndex;

    libtiff::TIFFSetDirectory(tiff, static_cast<uint16_t>(dirIndex));
    if (dirOffset)
        libtiff::TIFFSetSubDirectory(tiff, dirOffset);

    dir.dirIndex = dirIndex;
    dir.offset = dirOffset;

    char* description(nullptr);
    char* userLabel(nullptr);
    char* comments(nullptr);
    short dirchnls(0), dirbits(0);
    uint32_t *blankLines(nullptr), nblanklines(0);
    uint16_t compress(0);
    short planar_config(0);
    uint32_t width(0), height(0), tile_width(0), tile_height(0);
    float magnification(0);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags-start scanning " << dirIndex;

    libtiff::TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &dirchnls);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_SAMPLESPERPIXEL: " << dirchnls;
    libtiff::TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &dirbits);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_BITSPERSAMPLE: " << dirbits;
    libtiff::TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compress);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_COMPRESSION: " << compress;


    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_IMAGEWIDTH: " << width;
    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_IMAGELENGTH: " << height;
    libtiff::TIFFGetField(tiff,TIFFTAG_TILEWIDTH, &tile_width);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_TILEWIDTH: " << tile_width;
    libtiff::TIFFGetField(tiff,TIFFTAG_TILELENGTH, &tile_height);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_TILELENGTH: " << tile_height;
    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &description);
    if(description) {
        SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_IMAGEDESCRIPTION: " << description;
    }
    libtiff::TIFFGetField(tiff, NDPITAG_USERGIVENSLIDELABEL, &userLabel);
    if (userLabel) {
        SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags NDPITAG_USERGIVENSLIDELABEL: " << userLabel;
    }
    libtiff::TIFFGetField(tiff, NDPITAG_COMMENTS, &userLabel);
    if (comments) {
        SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags NDPITAG_COMMENTS: " << comments;
    }
    libtiff::TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planar_config);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags TIFFTAG_PLANARCONFIG: " << planar_config;
    libtiff::TIFFGetField(tiff, NDPITAG_MAGNIFICATION, &magnification);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags NDPITAG_MAGNIFICATION: " << magnification;
    libtiff::TIFFGetField(tiff, NDPITAG_BLANKLANES, &nblanklines, &blankLines);
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags NDPITAG_BLANKLANES: " << nblanklines;


    float resx(0), resy(0);
    uint16_t units(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_XRESOLUTION, &resx);
    libtiff::TIFFGetField(tiff, TIFFTAG_YRESOLUTION, &resy);
    libtiff::TIFFGetField(tiff, TIFFTAG_RESOLUTIONUNIT, &units);
    dir.interleaved = planar_config == PLANARCONFIG_CONTIG;
    float posx(0), posy(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_XPOSITION, &posx);
    libtiff::TIFFGetField(tiff, TIFFTAG_YPOSITION, &posy);
    uint32_t rowsPerStripe(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStripe);
    libtiff::TIFFDataType dt(libtiff::TIFF_NOTYPE);
    libtiff::TIFFGetField(tiff, TIFFTAG_DATATYPE, &dt);
    short ph(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &ph);
    dir.photometric = ph;
    dir.stripSize = (int)libtiff::TIFFStripSize(tiff);
    dir.dataType = dataTypeFromTIFFDataType(dt);
    if (dir.dataType == DataType::DT_None)
        dir.dataType = DataType::DT_Byte;

    short YCbCrSubsampling[2] = {2, 2};
    libtiff::TIFFGetField(tiff, TIFFTAG_YCBCRSUBSAMPLING, &YCbCrSubsampling[0], &YCbCrSubsampling[0]);
    dir.YCbCrSubsampling[0] = YCbCrSubsampling[0];
    dir.YCbCrSubsampling[1] = YCbCrSubsampling[1];

    if (units == RESUNIT_INCH && resx > 0 && resy > 0) {
        dir.res.x = 0.01 / resx;
        dir.res.y = 0.01 / resy;
    }
    else if (units == RESUNIT_INCH && resx > 0 && resy > 0) {
        dir.res.x = 0.0254 / resx;
        dir.res.y = 0.0254 / resy;
    }
    else if (units == RESUNIT_CENTIMETER && resx > 0 && resy > 0) {
        dir.res.x = 0.01 / resx;
        dir.res.y = 0.01 / resy;
    }
    else {
        dir.res.x = resx;
        dir.res.y = resy;
    }
    dir.position = {posx, posy};
    bool tiled = libtiff::TIFFIsTiled(tiff);
    if (description)
        dir.description = description;
    if(dirchnls == 0 && (ph == 0 || ph == 1)) {
        dirchnls = 1;
    }
    dir.bitsPerSample = dirbits;
    dir.channels = dirchnls;
    dir.height = height;
    dir.width = width;
    dir.tileHeight = tile_height;
    dir.tileWidth = tile_width;
    dir.tiled = tiled;
    dir.compression = compress;
    dir.rowsPerStrip = rowsPerStripe;
    dir.slideioCompression = compressTiffToSlideio(compress);
    dir.magnification = magnification;
    if (comments)
        dir.comments = comments;
    if (userLabel)
        dir.userLabel = userLabel;
    dir.blankLines = nblanklines;
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags-end " << dirIndex;
}

void NDPITiffTools::updateJpegXRCompressedDirectoryMedatata(libtiff::TIFF* tiff, NDPITiffDirectory& dir)
{
    if (dir.tiled) {
        const int tile = 0;
        cv::Size tileSize = computeTileSize(dir, tile);
        const int tileBufferSize = dir.channels * tileSize.width * tileSize.height * Tools::dataTypeSize(dir.dataType);
        std::vector<uint8_t> rawTile(tileBufferSize);
        libtiff::tmsize_t readBytes = libtiff::TIFFReadRawTile(tiff, tile, rawTile.data(), (int)rawTile.size());
        if (readBytes <= 0) {
            RAISE_RUNTIME_ERROR << "TiffTools: Error reading raw tile";
        }
        jpegxr_image_info info;
        jpegxr_get_image_info((uint8_t*)rawTile.data(), (uint32_t)rawTile.size(), info);
        dir.channels = info.channels;
        dir.dataType = getSlideioType(info);
    }
}

void slideio::NDPITiffTools::scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset,
                                         slideio::NDPITiffDirectory& dir)
{
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDir-begin " << dir.dirIndex;

    libtiff::TIFFSetDirectory(tiff, (short)dirIndex);
    if (dirOffset > 0)
        libtiff::TIFFSetSubDirectory(tiff, dirOffset);

    dir.dirIndex = dirIndex;
    dir.offset = dirOffset;

    scanTiffDirTags(tiff, dirIndex, dirOffset, dir);
    // check if we have to refine data for jpegxr compressed files
    if(dir.slideioCompression == Compression::JpegXR) {
        updateJpegXRCompressedDirectoryMedatata(tiff, dir);
    }
    dir.offset = 0;
    long subdirs(0);
    int64* offsets_raw(nullptr);
    if (libtiff::TIFFGetField(tiff, TIFFTAG_SUBIFD, &subdirs, &offsets_raw)) {
        std::vector<int64> offsets(offsets_raw, offsets_raw + subdirs);
        if (subdirs > 0) {
            dir.subdirectories.resize(subdirs);
        }
        for (int subdir = 0; subdir < subdirs; subdir++) {
            if (libtiff::TIFFSetSubDirectory(tiff, offsets[subdir])) {
                scanTiffDirTags(tiff, dirIndex, dir.subdirectories[subdir].offset, dir.subdirectories[subdir]);
            }
        }
    }
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDir-end " << dir.dirIndex;
}

void slideio::NDPITiffTools::scanFile(libtiff::TIFF* tiff, std::vector<NDPITiffDirectory>& directories)
{
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile-begin";

    int dirs = libtiff::TIFFNumberOfDirectories(tiff);
    SLIDEIO_LOG(INFO) << "Total number of directories: " << dirs;
    directories.resize(dirs);
    for (int dir = 0; dir < dirs; dir++) {
        SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile processing directory " << dir;
        directories[dir].dirIndex = dir;
        scanTiffDir(tiff, dir, 0, directories[dir]);
    }
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanFile-end";
}

void slideio::NDPITiffTools::scanFile(const std::string& filePath, std::vector<NDPITiffDirectory>& directories)
{
    libtiff::TIFF* file(nullptr);
    try {
        file = libtiff::TIFFOpen(filePath.c_str(), "r");
        if (file == nullptr) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: cannot open tiff file " << filePath;
        }
        scanFile(file, directories);
    }
    catch (std::exception& ex) {
        if (file)
            libtiff::TIFFClose(file);
        throw ex;
    }
    if (file)
        libtiff::TIFFClose(file);
}

void NDPITiffTools::readNotRGBStripedDir(libtiff::TIFF* file, const NDPITiffDirectory& dir, cv::_OutputArray output)
{
    std::vector<uint8_t> rgbaRaster(4 * dir.rowsPerStrip * dir.width);

    int buff_size = dir.width * dir.height * dir.channels * Tools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = {dir.width, dir.height};
    slideio::DataType dt = dir.dataType;
    output.create(sizeImage, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    cv::Mat imageRaster = output.getMat();
    libtiff::TIFFSetDirectory(file, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(file, dir.offset);
    }
    uint8_t* buffBegin = imageRaster.data;
    int stripBuffSize = dir.stripSize;
    const int imageWidth3 = dir.width * 3;
    const int imageWidth4 = dir.width * 4;

    for (int strip = 0, row = 0; row < dir.height; strip++, row += dir.rowsPerStrip, buffBegin += stripBuffSize) {
        if ((strip + stripBuffSize) > buff_size)
            stripBuffSize = buff_size - strip;

        int stripeRows = dir.rowsPerStrip;
        if (row + stripeRows > dir.height) {
            stripeRows = dir.height - row;
        }

        int read = libtiff::TIFFReadRGBAStrip(file, row, (uint32_t*)rgbaRaster.data());
        if (read != 1) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: Error by reading of tif strip " << strip;
        }
        uint8_t* lineBegin = buffBegin;
        uint8_t* stripeLineBegin = rgbaRaster.data();
        for (int stripeRow = 0; stripeRow < stripeRows; ++stripeRow) {
            uint8_t* pixelBegin = lineBegin;
            uint8_t* stripePixelBegin = stripeLineBegin;
            for (int column = 0; column < dir.width; ++column, pixelBegin += 3, stripePixelBegin += 4) {
                memcpy(pixelBegin, stripePixelBegin, 3);
            }
            lineBegin += imageWidth3;
            stripeLineBegin += imageWidth4;
        }
    }
}

void slideio::NDPITiffTools::readRegularStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir,
                                                   cv::OutputArray output)
{
    int buff_size = dir.width * dir.height * dir.channels * Tools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = {dir.width, dir.height};
    slideio::DataType dt = dir.dataType;
    output.create(sizeImage, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    cv::Mat imageRaster = output.getMat();
    libtiff::TIFFSetDirectory(file, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(file, dir.offset);
    }
    uint8_t* buffBegin = imageRaster.data;
    int stripBuffSize = dir.stripSize;

    for (int strip = 0, row = 0; row < dir.height; strip++, row += dir.rowsPerStrip, buffBegin += stripBuffSize) {
        if ((strip + stripBuffSize) > buff_size)
            stripBuffSize = buff_size - strip;

        int read = (int)libtiff::TIFFReadEncodedStrip(file, strip, buffBegin, stripBuffSize);
        if (read <= 0) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: Error by reading of tif striped directory";
        }
    }
    return;
}


void slideio::NDPITiffTools::readStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir,
                                            cv::OutputArray output)
{
    if (!dir.interleaved) {
        RAISE_RUNTIME_ERROR << "Planar striped images are not supported";
    }

    std::vector<uint8_t> rgbaRaster;
    bool notRGB = dir.photometric == 6 || dir.photometric == 8 || dir.photometric == 9 || dir.photometric == 10;
    if (notRGB) {
        readNotRGBStripedDir(file, dir, output);
    }
    else {
        readRegularStripedDir(file, dir, output);
    }

    return;
}

void NDPITiffTools::readJpegXRTile(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir, int tile,
                                   const std::vector<int>& vector, cv::OutputArray output)
{
    cv::Size tileSize = computeTileSize(dir, tile);
    const int tileBufferSize = dir.channels * tileSize.width * tileSize.height * Tools::dataTypeSize(dir.dataType);
    std::vector<uint8_t> rawTile(tileBufferSize);
    if (dir.interleaved) {
        // process interleaved channels
        libtiff::tmsize_t readBytes = libtiff::TIFFReadRawTile(tiff, tile, rawTile.data(), (int)rawTile.size());
        if (readBytes <= 0) {
            RAISE_RUNTIME_ERROR << "TiffTools: Error reading raw tile";
        }
        decodeJxrBlock(rawTile.data(), readBytes, output);
    }
    else {
        RAISE_RUNTIME_ERROR << "TiffTools: jpegxr compressed directory must be interleaved!";
    }
}


void slideio::NDPITiffTools::readTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                                      const std::vector<int>& channelIndices, cv::OutputArray output)
{
    if (!dir.tiled) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: Expected tiled configuration, received striped";
    }
    setCurrentDirectory(hFile, dir);

    if (dir.compression == 0x5852) {
        readJpegXRTile(hFile, dir, tile, channelIndices, output);
    }
    else if (dir.photometric == 6 || dir.photometric == 8 || dir.photometric == 9 || dir.photometric == 10) {
        readNotRGBTile(hFile, dir, tile, channelIndices, output);
    }
    else {
        readRegularTile(hFile, dir, tile, channelIndices, output);
    }
}

int NDPITiffTools::computeStripHeight(int height, int rowsPerStrip, int strip)
{
    const int stripCount = (height - 1) / rowsPerStrip + 1;
    if (strip >= stripCount || strip < 0) {
        RAISE_RUNTIME_ERROR << "Invalid strip number: " << strip << ". Number of strips: " << stripCount;
    }
    int lineCount = rowsPerStrip;
    if (strip == (stripCount - 1)) {
        lineCount = height - strip * rowsPerStrip;
    }
    return lineCount;
}

cv::Size NDPITiffTools::computeTileSize(const NDPITiffDirectory& dir, int tile)
{
    cv::Size const tileCounts = computeTileCounts(dir);
    const int tileCount = tileCounts.width * tileCounts.height;
    if (tile >= tileCount || tile < 0) {
        RAISE_RUNTIME_ERROR << "Invalid tile number: " << tile << ". Number of tiles: " << tileCount;
    }
    const int tileRows = tileCounts.height;
    const int tileCols = tileCounts.width;
    const int tileRow = tile / tileCols;
    const int tileCol = tile - tileRow * tileCols;
    cv::Size tileSize(dir.tileWidth, dir.tileHeight);
    if (tileRow == (tileRows - 1)) {
        tileSize.height = dir.height - dir.tileHeight * tileRow;

    }
    if (tileCol == (tileCols - 1)) {
        tileSize.width = dir.width - dir.tileWidth * tileCol;
    }
    return tileSize;
}

cv::Size NDPITiffTools::computeTileCounts(const NDPITiffDirectory& dir)
{
    const int tileRows = (dir.height - 1) / dir.tileHeight + 1;
    const int tileCols = (dir.width - 1) / dir.tileWidth + 1;
    cv::Size tileCounts(tileCols, tileRows);
    return tileCounts;
}

struct ErrorManager
{
    struct jpeg_error_mgr pub; /* "public" fields */
    jmp_buf setjmp_buffer; /* for return to caller */
};

typedef struct ErrorManager* my_error_ptr;

void ErrorExit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a ErrorManager struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message)(cinfo);
    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

void NDPITiffTools::readScanlines(libtiff::TIFF* tiff, FILE* file, const NDPITiffDirectory& dir, int firstScanline,
                                  int numberScanlines, const std::vector<int>& channelIndices, cv::_OutputArray output)
{
    setCurrentDirectory(tiff, dir);

    uint64_t stripeOffset = libtiff::TIFFGetStrileOffset(tiff, 0);
    int ret = FSEEK64(file, stripeOffset, SEEK_SET);
    jpeg_decompress_struct cinfo;
    ErrorManager jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ErrorExit;

    if (setjmp(jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.*/
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    cinfo.image_width = dir.width;
    cinfo.image_height = dir.height;
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    int row_stride = cinfo.output_width * cinfo.output_components;

    if (firstScanline) {
        int skipped = jpeg_skip_scanlines(&cinfo, firstScanline);
        if (skipped != firstScanline) {
            RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by skipping scanlines. Expected:" << firstScanline <<
                ". Skipped: " << skipped;
        }
    }
    output.create(numberScanlines, cinfo.output_width, CV_MAKETYPE(CV_8U, cinfo.output_components));
    cv::Mat mat = output.getMat();
    mat.setTo(cv::Scalar(0));
    uint8_t* rowBegin = mat.data;

    for (int scanline = 0; scanline < numberScanlines; ++scanline) {
        int read = jpeg_read_scanlines(&cinfo, &rowBegin, 1);
        if (read != 1) {
            RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by reading scanline " << scanline << " of " <<
                numberScanlines;
        }
        rowBegin += row_stride;
    }
    const int rowsLeft = dir.height - firstScanline - numberScanlines;
    if (rowsLeft > 0) {
        jpeg_skip_scanlines(&cinfo, rowsLeft);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void NDPITiffTools::readJpegDirectoryRegion(libtiff::TIFF* tiff, const std::string& filePath, const cv::Rect& region,
                                            const NDPITiffDirectory& dir, const std::vector<int>& channelIndices,
                                            cv::_OutputArray output)
{
    if (dir.tiled) {
        RAISE_RUNTIME_ERROR << "Stripped directory expected";
    }
    if (dir.rowsPerStrip != dir.height) {
        RAISE_RUNTIME_ERROR << "One strip directory is expected. Rows per strip: " << dir.rowsPerStrip << ". Height:" <<
            dir.height;
    }
    setCurrentDirectory(tiff, dir);

    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file) {
        RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot open file " << filePath;
    }

    const bool allChannels = Tools::isCompleteChannelList(channelIndices, dir.channels);

    try {
        const slideio::DataType dt = dir.dataType;
        const int dataSize = slideio::Tools::dataTypeSize(dt);
        const int cvType = slideio::CVTools::toOpencvType(dt);
        output.create(region.size(), CV_MAKETYPE(cvType, allChannels?dir.channels:static_cast<int>(channelIndices.size())));
        cv::Mat outputMat = output.getMat();
        outputMat.setTo(0);

        const int firstScanline = region.y;
        const int numberScanlines = region.height;

        uint64_t stripeOffset = libtiff::TIFFGetStrileOffset(tiff, 0);
        int ret = FSEEK64(file, stripeOffset, SEEK_SET);
        jpeg_decompress_struct cinfo{};
        ErrorManager jErr{};

        cinfo.err = jpeg_std_error(&jErr.pub);
        jErr.pub.error_exit = ErrorExit;

        if (setjmp(jErr.setjmp_buffer)) {
            /* If we get here, the JPEG code has signaled an error.
             * We need to clean up the JPEG object, close the input file, and return.*/
            jpeg_destroy_decompress(&cinfo);
            return;
        }

        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, file);
        cinfo.image_width = dir.width;
        cinfo.image_height = dir.height;
        jpeg_read_header(&cinfo, TRUE);
        jpeg_start_decompress(&cinfo);


        if (firstScanline) {
            int skipped = jpeg_skip_scanlines(&cinfo, firstScanline);
            if (skipped != firstScanline) {
                RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by skipping scanlines. Expected:" << firstScanline <<
                    ". Skipped: " << skipped;
            }
        }
        int bufferRowStride = cinfo.output_width * cinfo.output_components * slideio::Tools::dataTypeSize(dt);
        const int MAX_BUFFER_SIZE = 10 * 1024 * 1024;
        const int numBufferLines = std::min(dir.height, MAX_BUFFER_SIZE / bufferRowStride);
        cv::Mat imageBuffer(numBufferLines, dir.width, CV_MAKETYPE(cvType, cinfo.output_components));
        imageBuffer.setTo(cv::Scalar(255, 255, 255));

        // channel mapping
        std::vector<int> fromTo(channelIndices.size() * 2);
        for (int index = 0; index < channelIndices.size(); ++index) {
            int location = index * 2;
            fromTo[location] = channelIndices[index];
            fromTo[location + 1] = index;
        }

        uint8_t* rowBegin = imageBuffer.data;
        int imageLine(0), bufferLine(0), bufferIndex(0);
        bool startNewBlock(true);
        for (; imageLine < numberScanlines; ++imageLine, ++bufferLine, rowBegin += bufferRowStride) {
            if(startNewBlock) {
                startNewBlock = false;
                rowBegin = imageBuffer.data;
                bufferLine = 0;
            }
            int read = jpeg_read_scanlines(&cinfo, &rowBegin, 1);
            if (read != 1) {
                RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by reading scanline " << imageLine << " of " <<
                    numberScanlines;
            }
            if (bufferLine == (numBufferLines - 1) || imageLine == (numberScanlines - 1)) {
                // copy buffer to output raster
                const int firstLine = bufferIndex * numBufferLines;
                const int numbLeftLines = numberScanlines - firstLine;
                const int numValidLines = std::min(numbLeftLines, numBufferLines);
                
                cv::Rect srcRoi = {region.x, 0, region.width, numValidLines };
                cv::Rect dstRoi = {0, firstLine, region.width, numValidLines };
                cv::Mat srcImage(imageBuffer, srcRoi);
                cv::Mat dstImage(outputMat, dstRoi);
                if (allChannels) {
                    srcImage.copyTo(dstImage);
                }
                else {
                    cv::mixChannels(&srcImage, 1, &dstImage, 1, fromTo.data(), channelIndices.size());
                }
                bufferIndex++;
                startNewBlock = true;
            }
        }
        const int rowsLeft = dir.height - firstScanline - numberScanlines;
        if (rowsLeft > 0) {
            jpeg_skip_scanlines(&cinfo, rowsLeft);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        fclose(file);
    }
    catch (std::exception&) {
        fclose(file);
    }
}

void NDPITiffTools::readJpegXRStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip,
                                    const std::vector<int>& vector, cv::_OutputArray output)
{
    const int lineCount = computeStripHeight(dir.height, dir.rowsPerStrip, strip);
    const int stripSize = dir.channels * lineCount * dir.width * Tools::dataTypeSize(dir.dataType);
    std::vector<uint8_t> rawStrip(stripSize);
    if (dir.interleaved) {
        // process interleaved channels
        libtiff::tmsize_t readBytes = libtiff::TIFFReadRawStrip(tiff, strip, rawStrip.data(), (int)rawStrip.size());
        if (readBytes <= 0) {
            RAISE_RUNTIME_ERROR << "TiffTools: Error reading raw strip!";
        }
        decodeJxrBlock(rawStrip.data(), readBytes, output);
    }
    else {
        RAISE_RUNTIME_ERROR << "JpegXR compressed strip must be interleaved";
    }
}

void NDPITiffTools::readNotRGBStrip(libtiff::TIFF* hFile, const NDPITiffDirectory& dir, int strip,
                                    const std::vector<int>& channelIndices, cv::_OutputArray output)
{
    const int lineCount = computeStripHeight(dir.height, dir.rowsPerStrip, strip);
    cv::Size stripSize = {dir.width, lineCount};
    slideio::DataType dt = dir.dataType;
    cv::Mat stripRaster;
    stripRaster.create(stripSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), 4));
    libtiff::TIFFSetDirectory(hFile, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(hFile, dir.offset);
    }
    uint32_t* buffBegin = reinterpret_cast<uint32_t*>(stripRaster.data);
    auto readBytes = libtiff::TIFFReadRGBAStrip(hFile, strip, buffBegin);
    if (readBytes <= 0) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error reading encoded strip " << strip
            << " of directory " << dir.dirIndex << ". Compression: " << (int)(dir.compression);

    }

    if (channelIndices.empty()) {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(3);
        for (int channelIndex = 0; channelIndex < 3; ++channelIndex) {
            cv::extractChannel(stripRaster, channelRasters[channelIndex], channelIndex);
        }
        cv::merge(channelRasters, output);
    }
    else if (channelIndices.size() == 1) {
        cv::extractChannel(stripRaster, output, channelIndices[0]);
    }
    else {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for (int channelIndex : channelIndices) {
            cv::extractChannel(stripRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, output);
    }
}

void NDPITiffTools::readRegularStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip,
                                     const std::vector<int>& channelIndices, cv::_OutputArray output)
{
    const int lineCount = computeStripHeight(dir.height, dir.rowsPerStrip, strip);
    cv::Size stripSize = {dir.width, lineCount};
    slideio::DataType dt = dir.dataType;
    cv::Mat stripRaster;
    stripRaster.create(stripSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    libtiff::TIFFSetDirectory(tiff, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(tiff, dir.offset);
    }
    uint8_t* buff_begin = stripRaster.data;
    auto buf_size = stripRaster.total() * stripRaster.elemSize();
    auto readBytes = libtiff::TIFFReadEncodedStrip(tiff, strip, buff_begin, buf_size);
    if (readBytes <= 0) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error reading encoded strip " << strip
            << " of directory " << dir.dirIndex << ". Compression: " << (int)(dir.compression);
    }
    if (channelIndices.empty()) {
        stripRaster.copyTo(output);
    }
    else if (channelIndices.size() == 1) {
        cv::extractChannel(stripRaster, output, channelIndices[0]);
    }
    else {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for (int channelIndex : channelIndices) {
            cv::extractChannel(stripRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, output);
    }
}

void NDPITiffTools::readStrip(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int strip,
                              const std::vector<int>& channelIndices, cv::OutputArray output)
{
    if (dir.tiled) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: Expected tiled configuration, received striped";
    }
    setCurrentDirectory(hFile, dir);

    if (dir.compression == 0x5852) {
        readJpegXRStrip(hFile, dir, strip, channelIndices, output);
    }
    else if (dir.photometric == 6 || dir.photometric == 8 || dir.photometric == 9 || dir.photometric == 10) {
        readNotRGBStrip(hFile, dir, strip, channelIndices, output);
    }
    else {
        readRegularStrip(hFile, dir, strip, channelIndices, output);
    }
}

void slideio::NDPITiffTools::readRegularTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                                             const std::vector<int>& channelIndices, cv::OutputArray output)
{
    cv::Size tileSize = {dir.tileWidth, dir.tileHeight};
    slideio::DataType dt = dir.dataType;
    cv::Mat tileRaster;
    tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    libtiff::TIFFSetDirectory(hFile, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(hFile, dir.offset);
    }
    uint8_t* buff_begin = tileRaster.data;
    auto buf_size = tileRaster.total() * tileRaster.elemSize();
    auto readBytes = libtiff::TIFFReadEncodedTile(hFile, tile, buff_begin, buf_size);
    if (readBytes <= 0) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error reading encoded tiff tile " << tile
            << " of directory " << dir.dirIndex << ". Compression: " << dir.compression;
    }
    if (channelIndices.empty()) {
        tileRaster.copyTo(output);
    }
    else if (channelIndices.size() == 1) {
        cv::extractChannel(tileRaster, output, channelIndices[0]);
    }
    else {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for (int channelIndex : channelIndices) {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, output);
    }
}


void NDPITiffTools::readNotRGBTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                                   const std::vector<int>& channelIndices, cv::OutputArray output)
{
    cv::Size tileSize = computeTileSize(dir, tile);
    slideio::DataType dt = dir.dataType;
    cv::Mat tileRaster;
    tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), 4));
    libtiff::TIFFSetDirectory(hFile, static_cast<uint16_t>(dir.dirIndex));
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(hFile, dir.offset);
    }
    uint32_t* buffBegin = reinterpret_cast<uint32_t*>(tileRaster.data);

    int cols = (dir.width - 1) / dir.tileWidth + 1;
    int rows = (dir.height - 1) / dir.tileHeight + 1;
    int row = tile / cols;
    int col = tile - row * cols;
    int tileX = col * dir.tileWidth;
    int tileY = row * dir.tileHeight;
    auto readBytes = libtiff::TIFFReadRGBATile(hFile, tileX, tileY, buffBegin);
    if (readBytes <= 0) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error reading encoded tiff tile " << tile
            << " of directory " << dir.dirIndex << " Compression: " << dir.compression;
    }
    cv::Mat flipped;
    if (channelIndices.empty()) {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(3);
        for (int channelIndex = 0; channelIndex < 3; ++channelIndex) {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndex);
        }
        cv::merge(channelRasters, flipped);
    }
    else if (channelIndices.size() == 1) {
        cv::extractChannel(tileRaster, flipped, channelIndices[0]);
    }
    else {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for (int channelIndex : channelIndices) {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, flipped);
    }
    cv::flip(flipped, output, 0);
}


void slideio::NDPITiffTools::setCurrentDirectory(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir)
{
    if (!libtiff::TIFFSetDirectory(hFile, static_cast<uint16_t>(dir.dirIndex))) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error by setting current directory " << dir.dirIndex;
    }
    if (dir.offset > 0) {
        if (!libtiff::TIFFSetSubDirectory(hFile, dir.offset)) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: error by setting current sub-directory. Directory:"
                << dir.dirIndex
                << ".Offset:" << dir.offset;
        }
    }
}

void slideio::NDPITiffTools::decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output)
{
    jpegxr_image_info info;
    jpegxr_get_image_info((uint8_t*)data, (uint32_t)dataBlockSize, info);
    int type = getCvType(info);
    output.create(info.height, info.width, CV_MAKETYPE(type, info.channels));
    cv::Mat mat = output.getMat();
    mat.setTo(cv::Scalar(0));
    uint8_t* outputBuff = mat.data;
    uint32_t ouputBuffSize = (int)(mat.total() * mat.elemSize());
    jpegxr_decompress((uint8_t*)data, (uint32_t)dataBlockSize, outputBuff, ouputBuffSize);
}

slideio::NDPITIFFKeeper::NDPITIFFKeeper(libtiff::TIFF* hfile) : m_hFile(hfile)
{
}


NDPITIFFKeeper::~NDPITIFFKeeper()
{
    if (m_hFile)
        libtiff::TIFFClose(m_hFile);
}
