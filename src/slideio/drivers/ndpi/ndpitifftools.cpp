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

#include <codecvt>
#include <opencv2/imgproc.hpp>

#include "slideio/imagetools/cvtools.hpp"
#include "jpeglib.h"
#include "ndpifile.hpp"
#include "slideio/core/tools/blocktiler.hpp"
#include "slideio/core/tools/cachemanager.hpp"
#include "slideio/imagetools/imagetools.hpp"

const int NDPI_RESTART_MARKERS = 65426;

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
        compression = Compression::LZW;
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
    Tools::throwIfPathNotExist(path, "NDPITiffTools::openTiffFile");
    libtiff::TIFF* hfile(nullptr);
#if defined(WIN32)
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wsPath = converter.from_bytes(path);
    hfile = libtiff::TIFFOpenW(wsPath.c_str(), "r");
#else
    hfile = libtiff::TIFFOpen(path.c_str(), "r");
#endif
    return hfile;
}

void slideio::NDPITiffTools::closeTiffFile(libtiff::TIFF* file)
{
    libtiff::TIFFClose(file);
}


static slideio::DataType retrieveTiffDataType(libtiff::TIFF* tiff)
{
    int bitsPerSample = 0;
    int sampleFormat = 0;

    slideio::DataType dataType = slideio::DataType::DT_Unknown;
    if (!libtiff::TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample)) {
        RAISE_RUNTIME_ERROR << "Cannot retrieve bits per sample from tiff image";
    }
    if (!libtiff::TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat)) {
        sampleFormat = SAMPLEFORMAT_UINT;
    }

    if (bitsPerSample == 8) {
        if (sampleFormat == SAMPLEFORMAT_UINT) {
            dataType = slideio::DataType::DT_Byte;
        }
        else if (sampleFormat == SAMPLEFORMAT_INT) {
            dataType = slideio::DataType::DT_Int8;
        }
        else {
            RAISE_RUNTIME_ERROR << "Unsupported sample format for 8bit images: " << sampleFormat;
        }
    }
    else if (bitsPerSample == 16) {
        if (sampleFormat == SAMPLEFORMAT_UINT) {
            dataType = slideio::DataType::DT_UInt16;
        }
        else if (sampleFormat == SAMPLEFORMAT_INT) {
            dataType = slideio::DataType::DT_Int16;
        }
        else if (sampleFormat == SAMPLEFORMAT_IEEEFP) {
            dataType = slideio::DataType::DT_Float16;
        }
        else {
            RAISE_RUNTIME_ERROR << "Unsupported sample format for 16bit images: " << sampleFormat;
        }
    }
    else if (bitsPerSample == 32) {
        if (sampleFormat == SAMPLEFORMAT_INT) {
            dataType = slideio::DataType::DT_Int32;
        }
        else if (sampleFormat == SAMPLEFORMAT_IEEEFP) {
            dataType = slideio::DataType::DT_Float32;
        }
        else {
            RAISE_RUNTIME_ERROR << "Unsupported sample format for 32bit images: " << sampleFormat;
        }
    }
    else if (bitsPerSample == 64) {
        if (sampleFormat == SAMPLEFORMAT_IEEEFP) {
            dataType = slideio::DataType::DT_Float64;
        }
        else {
            RAISE_RUNTIME_ERROR << "Unsupported sample format for 64bit images: " << sampleFormat;
        }
    }
    else {
        RAISE_RUNTIME_ERROR << "Unsupported bits per sample: " << bitsPerSample;
    }
    return dataType;
}

void slideio::NDPITiffTools::scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset,
                                             slideio::NDPITiffDirectory& dir)
{
    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags-begin " << dirIndex;

    if (libtiff::TIFFCurrentDirectory(tiff) != dirIndex) {
        libtiff::TIFFSetDirectory(tiff, (short)dirIndex);
    }
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
    libtiff::TIFFGetField(tiff, NDPITAG_COMMENTS, &comments);
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

    uint32_t* stripOffset = 0;
    auto r = libtiff::TIFFGetField(tiff, TIFFTAG_STRIPOFFSETS, &stripOffset);

    uint32_t rowsPerStripe(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStripe);
    libtiff::TIFFDataType dt(libtiff::TIFF_NOTYPE);
    libtiff::TIFFGetField(tiff, TIFFTAG_DATATYPE, &dt);
    short ph(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &ph);
    dir.photometric = ph;
    dir.stripSize = (int)libtiff::TIFFStripSize(tiff);
    dir.dataType = retrieveTiffDataType(tiff);

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

    int32_t markers = 0;
    uint32_t *offsets(nullptr);
    libtiff::TIFFGetField(tiff, NDPI_RESTART_MARKERS, &markers, &offsets);
    for(int index = 0; index < markers; ++index) {
        dir.mcuStarts.push_back(offsets[index] + *stripOffset);
    }

    SLIDEIO_LOG(INFO) << "NDPITiffTools::scanTiffDirTags-end " << dirIndex;
}

void NDPITiffTools::updateJpegXRCompressedDirectoryMedatata(libtiff::TIFF* tiff, NDPITiffDirectory& dir)
{
    if (dir.tiled) {
        const int tile = 0;
        cv::Size tileSize = computeTileSize(dir, tile);
        const int tileBufferSize = dir.channels * tileSize.width * tileSize.height * ImageTools::dataTypeSize(dir.dataType);
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

    if (libtiff::TIFFCurrentDirectory(tiff) != dirIndex) {
        libtiff::TIFFSetDirectory(tiff, (short)dirIndex);
    }
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
        file = openTiffFile(filePath);
        if (file == nullptr) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: cannot open tiff file " << filePath;
        }
        scanFile(file, directories);
    }
    catch (std::exception& ex) {
        if (file)
            closeTiffFile(file);
        throw ex;
    }
    if (file)
        closeTiffFile(file);
}

void NDPITiffTools::readNotRGBStripedDir(libtiff::TIFF* file, const NDPITiffDirectory& dir, cv::_OutputArray output)
{
    std::vector<uint8_t> rgbaRaster(4 * dir.rowsPerStrip * dir.width);

    int buff_size = dir.width * dir.height * dir.channels * ImageTools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = {dir.width, dir.height};
    slideio::DataType dt = dir.dataType;
    output.create(sizeImage, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    cv::Mat imageRaster = output.getMat();
    setCurrentDirectory(file, dir);
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
    int buff_size = dir.width * dir.height * dir.channels * ImageTools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = {dir.width, dir.height};
    slideio::DataType dt = dir.dataType;
    output.create(sizeImage, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    cv::Mat imageRaster = output.getMat();
    setCurrentDirectory(file, dir);
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

    const bool notSupportedColorSpace = dir.photometric == 6 || dir.photometric == 8 || dir.photometric == 9 || dir.photometric == 10;
    if (notSupportedColorSpace) {
        // let libtiff handle the color space conversion
        readNotRGBStripedDir(file, dir, output);
    }
    else {
        readRegularStripedDir(file, dir, output);
    }

    if(dir.photometric == 6) {
        const cv::Mat imageYCbCr = output.getMat();
        cv::Mat image;
        cv::cvtColor(imageYCbCr, image, cv::COLOR_YCrCb2RGB);
        output.assign(image);
    }

    return;
}

void NDPITiffTools::readJpegXRTile(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir, int tile,
                                   const std::vector<int>& vector, cv::OutputArray output)
{
    cv::Size tileSize = computeTileSize(dir, tile);
    const int tileBufferSize = dir.channels * tileSize.width * tileSize.height * ImageTools::dataTypeSize(dir.dataType);
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
    int ret = Tools::setFilePos(file, stripeOffset, SEEK_SET);
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

void NDPITiffTools::cacheScanlines(NDPIFile* ndpifile, const NDPITiffDirectory& dir,
    cv::Size tileSize, CacheManager* cacheManager)
{
    libtiff::TIFF* tiff = ndpifile->getTiffHandle();
    std::unique_ptr<FILE, Tools::FileDeleter> sfile(Tools::openFile(ndpifile->getFilePath(), "rb"));

    FILE* file = sfile.get();

    if(!file) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: file pointer is not set";
    }
    if(!cacheManager) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: cache manager is not set";
    }
    if(dir.tiled) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: Attempt to use stripped cache for tiled directory.";
    }

    if(dir.rowsPerStrip != dir.height) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: Attempt to use stripped cache for directory with rows per strip: " <<
            dir.rowsPerStrip << ". Expected: " << dir.height;
    }

    const int firstScanline = 0;
    const int numberScanlines = dir.height;

    setCurrentDirectory(tiff, dir);

    uint64_t stripeOffset = libtiff::TIFFGetStrileOffset(tiff, 0);
    int ret = Tools::setFilePos(file, stripeOffset, SEEK_SET);
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
    int rowStride = cinfo.output_width * cinfo.output_components;

    cv::Mat stripe;
    stripe.create(tileSize.height, cinfo.output_width, CV_MAKETYPE(CV_8U, cinfo.output_components));
    stripe.setTo(cv::Scalar(0));
    uint8_t* rowBegin = stripe.data;

    int stripeLine = 0;
    int stripeRows = tileSize.height;
    int stripeTop = 0;
    for (int scanline = 0; scanline < numberScanlines; ++scanline, ++stripeLine) {
        int read = jpeg_read_scanlines(&cinfo, &rowBegin, 1);
        if (read != 1) {
            RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by reading scanline " << scanline << " of " <<
                numberScanlines;
        }
        rowBegin += rowStride;
        if(stripeLine >= (stripeRows-1) || (scanline+1) == numberScanlines) {
            //if(dir.photometric==6) {
            //    cv::Mat stripeRGB;
            //    cv::cvtColor(stripe, stripeRGB, cv::COLOR_YCrCb2BGR);
            //    stripeRGB.copyTo(stripe);
            //}
            BlockTiler tiler(stripe, tileSize);
            tiler.apply([&cacheManager, stripeTop, &dir, tileSize](int x, int y, const cv::Mat& tile){
                cacheManager->addTile(dir.dirIndex, cv::Point(x*tileSize.width, stripeTop), tile);
            });
            if ((scanline + 1) < numberScanlines) {
                if (scanline + stripeRows >= numberScanlines) {
                    stripeRows = numberScanlines - scanline;
                    stripe.create(stripeRows, cinfo.output_width, CV_MAKETYPE(CV_8U, cinfo.output_components));
                }
                // prepare next stripe
                stripe.setTo(cv::Scalar(0));
            }
            rowBegin = stripe.data;
            stripeLine = -1;
            stripeTop = scanline + 1;
        }
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
    SLIDEIO_LOG(INFO)   << "NDPITiffTools::readJpegDirectoryRegion:"
                        << "Reading JPEG directory region: ("
                        << region.x << "," << region.y << ", "
                        << region.width << ", " << region.height << ")";
    if (dir.tiled) {
        RAISE_RUNTIME_ERROR << "Stripped directory expected";
    }
    if (dir.rowsPerStrip != dir.height) {
        RAISE_RUNTIME_ERROR << "One strip directory is expected. Rows per strip: " << dir.rowsPerStrip << ". Height:" <<
            dir.height;
    }
    setCurrentDirectory(tiff, dir);

    std::unique_ptr<FILE, Tools::FileDeleter> sfile(Tools::openFile(filePath.c_str(), "rb"));
    FILE* file = sfile.get();
    if (!file) {
        RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot open file " << filePath;
    }

    const bool allChannels = Tools::isCompleteChannelList(channelIndices, dir.channels);

    const slideio::DataType dt = dir.dataType;
    const int dataSize = slideio::ImageTools::dataTypeSize(dt);
    const int cvType = slideio::CVTools::toOpencvType(dt);
    output.create(region.size(), CV_MAKETYPE(cvType, allChannels?dir.channels:static_cast<int>(channelIndices.size())));
    cv::Mat outputMat = output.getMat();
    outputMat.setTo(0);

    const int firstScanline = region.y;
    const int numberScanlines = region.height;

    uint64_t stripeOffset = libtiff::TIFFGetStrileOffset(tiff, 0);
    int ret = Tools::setFilePos(file, stripeOffset, SEEK_SET);
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
        SLIDEIO_LOG(INFO) << "NDPITiffTools::readJpegDirectoryRegion: skipping " << firstScanline << " scanlines";
        int skipped = jpeg_skip_scanlines(&cinfo, firstScanline);
        if (skipped != firstScanline) {
            RAISE_RUNTIME_ERROR << "NDPIImageDriver: error by skipping scanlines. Expected:" << firstScanline <<
                ". Skipped: " << skipped;
        }
    }
    int bufferRowStride = cinfo.output_width * cinfo.output_components * slideio::ImageTools::dataTypeSize(dt);
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
    SLIDEIO_LOG(INFO) << "NDPITiffTools::readJpegDirectoryRegion: reading " << numberScanlines << " scanlines";
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
}

void NDPITiffTools::readDirectoryJpegHeaders(NDPIFile* ndpi, NDPITiffDirectory& dir)
{
    if (dir.height == dir.rowsPerStrip) {
        const auto dirIndex = dir.dirIndex;

        libtiff::TIFF* tiff = ndpi->getTiffHandle();
        setCurrentDirectory(tiff, dir);

        std::unique_ptr<FILE, Tools::FileDeleter> sfile(Tools::openFile(ndpi->getFilePath(), "rb"));
        FILE* file = sfile.get();
        if (!file) {
            RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot open file " << ndpi->getFilePath();
        }

        const auto stripeOffset = libtiff::TIFFGetStrileOffset(tiff, 0);

        int ret = Tools::setFilePos(file, stripeOffset, SEEK_SET);
        if (ret) {
            RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot seek file " << ndpi->getFilePath() << " to offset "
                << stripeOffset << ". For directory " << dirIndex << ". Code: " << ret;
        }
        cv::Size tileSize = NDPITiffTools::computeMCUTileSize(file, cv::Size(dir.width, dir.height));

        ret = Tools::setFilePos(file, stripeOffset, SEEK_SET);
        if (ret) {
            RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot seek file " << ndpi->getFilePath() << " to offset "
                << stripeOffset << ". For directory " << dirIndex << ". Code: " << ret;
        }
        const std::pair<uint64_t, uint64_t> headerInfo = NDPITiffTools::getJpegHeaderPos(file);
        dir.tileWidth = tileSize.width;
        dir.tileHeight = tileSize.height;
        dir.jpegHeaderOffset = stripeOffset;
        dir.jpegHeaderSize = static_cast<uint32_t>(headerInfo.second - stripeOffset);
        dir.jpegSOFMarker = headerInfo.first;
    }
}

void NDPITiffTools::readJpegXRStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip,
                                    const std::vector<int>& vector, cv::_OutputArray output)
{
    const int lineCount = computeStripHeight(dir.height, dir.rowsPerStrip, strip);
    const int stripSize = dir.channels * lineCount * dir.width * ImageTools::dataTypeSize(dir.dataType);
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
    setCurrentDirectory(hFile, dir);
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
    setCurrentDirectory(tiff, dir);
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
    setCurrentDirectory(hFile, dir);
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
    setCurrentDirectory(hFile, dir);
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

cv::Size NDPITiffTools::computeMCUTileSize(FILE* file, const cv::Size& dirSize)
{
    int tileWidth = 0;
    int tileHeight = 0;
    jpeg_decompress_struct cinfo = {};
    ErrorManager jerr = {};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ErrorExit;
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    cinfo.image_width = dirSize.width;
    cinfo.image_height = dirSize.height;
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    cinfo.output_scanline = cinfo.output_width;
    uint32_t mcuWidth = cinfo.max_h_samp_factor * DCTSIZE;
    uint32_t mcuHeight = cinfo.max_v_samp_factor * DCTSIZE;
    uint32_t mcuPerRow = (dirSize.width + mcuWidth - 1) / mcuWidth;
    if(cinfo.restart_interval >0 && cinfo.restart_interval < mcuPerRow) {
        if ((mcuPerRow % cinfo.restart_interval) == 0) {
            tileWidth = mcuWidth * cinfo.restart_interval;
            tileHeight = mcuHeight;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return {tileWidth, tileHeight};
}

std::pair<uint64_t, uint64_t> NDPITiffTools::getJpegHeaderPos(FILE* file)
{
    uint64_t headerStop = 0;
    uint64_t SOFmarker = 0;
    uint8_t buff[2];
    while(true) {
        uint64_t pos = Tools::getFilePos(file);
        size_t count = fread(buff, sizeof(uint8_t), 2, file);
        if(count != 2) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading marker from jpeg stream.";
        }
        if (buff[0] != 0xFF) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading jpeg stream. Expected 0xFF";
        }
        uint8_t marker = buff[1];
        if(marker == 0xD8) {
           continue;  // SOI marker
        }
        // SOF marker
        if ((marker >= 0xC0 && marker <= 0xC3) ||
            (marker >= 0xC5 && marker <= 0xC7) ||
            (marker >= 0xC9 && marker <= 0xCB) ||
            (marker >= 0xCD && marker <= 0xCF)) {
            SOFmarker = pos;
        }
        uint16_t length = 0;
        count = fread(&length, sizeof(length), 1, file);
        if(count != 1) {
            RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading marker length from jpeg stream.";
        }
        if(Tools::isLittleEndian()) {
            length = Tools::bigToLittleEndian16(length);
        }
        Tools::setFilePos(file, pos + sizeof(buff) + length, SEEK_SET);
        if(marker == 0xDA) {
            headerStop = Tools::getFilePos(file);
            break;
        }
    }

    return {SOFmarker, headerStop };
}

void NDPITiffTools::readMCUTile(FILE* file, const NDPITiffDirectory& dir, int tile, cv::OutputArray output)
{
    if(tile>=dir.mcuStarts.size()) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: tile index is out of range (0-"
            << dir.mcuStarts.size() << "). Received:" << tile;
    }
    // read jpeg header
    const uint64_t headerOffset = dir.jpegHeaderOffset;
    const uint32_t headerSize = dir.jpegHeaderSize;
    const uint64_t tileOffset = dir.mcuStarts[tile];
    uint32_t tileSize = 0;
    if(tile < dir.mcuStarts.size()-1) {
        tileSize = static_cast<uint32_t>(dir.mcuStarts[tile + 1] - tileOffset);
    }
    else {
        const uint64_t fileSize = Tools::getFileSize(file);
        tileSize = static_cast<uint32_t>(fileSize - tileOffset);
    }
    std::vector<uint8_t> tileData(headerSize + tileSize);
    Tools::setFilePos(file, headerOffset, SEEK_SET);
    auto count = fread(tileData.data(), sizeof(uint8_t), headerSize, file);
    if(count != headerSize) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading jpeg header. Expected:" << headerSize << ". Read:" << count;
    }
    Tools::setFilePos(file, tileOffset, SEEK_SET);
    count = fread(tileData.data() + headerSize, sizeof(uint8_t), tileSize, file);
    if(count != tileSize) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading jpeg tile. Expected:" << tileSize << ". Read:" << count;
    }
    if(tileData[tileData.size() - 2] != 0xFF) {
        RAISE_RUNTIME_ERROR << "NDPITiffTools: error by reading jpeg tile. Expected 0xFF.";
    }
    tileData[tileData.size() - 1] = JPEG_EOI; // End of image marker
    jpeglibDecodeTile(tileData.data(), tileData.size(), cv::Size(dir.tileWidth, dir.tileHeight), output);

}

void NDPITiffTools::jpeglibDecodeTile(const uint8_t* jpg_buffer, size_t jpg_size, const cv::Size& tileSize, cv::OutputArray output)
{
    // code derived from: https://gist.github.com/PhirePhly/3080633
    struct jpeg_decompress_struct cinfo {};
    struct jpeg_error_mgr jerr {};

    cinfo.err = jpeg_std_error(&jerr);
    // Allocate a new decompress struct, with the default error handler.
    // The default error handler will exit() on pretty much any issue,
    // so it's likely you'll want to replace it or supplement it with
    // your own.
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpg_buffer, static_cast<unsigned long>(jpg_size));
    // Have the decompressor scan the jpeg header. This won't populate
    // the cinfo struct output fields, but will indicate if the
    // jpeg is valid.
    auto rc = jpeg_read_header(&cinfo, TRUE);
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    cinfo.image_width = tileSize.width;
    cinfo.image_height = tileSize.height;

    if (rc != 1) {
        jpeg_destroy_decompress(&cinfo);
        throw std::runtime_error(
            (boost::format("Invalid jpeg stream. JpegLib returns code:  %1%") % rc).str()
        );
    }

    // By calling jpeg_start_decompress, you populate cinfo
    // and can then allocate your output bitmap buffers for
    // each scanline.
    jpeg_start_decompress(&cinfo);

    const JDIMENSION width = cinfo.output_width;
    const JDIMENSION height = cinfo.output_height;
    const int channels = cinfo.output_components;

    const size_t bmpSize = width * height * channels;

    output.create(height, width, CV_MAKETYPE(CV_8U, channels));
    cv::Mat mat = output.getMat();

    // The row_stride is the total number of bytes it takes to store an
    // entire scanline (row). 
    const unsigned int rowStride = width * channels;

    // Now that you have the decompressor entirely configured, it's time
    // to read out all of the scanlines of the jpeg.
    //
    // By default, scanlines will come out in RGBRGBRGB...  order, 
    // but this can be changed by setting cinfo.out_color_space
    //
    // jpeg_read_scanlines takes an array of buffers, one for each scanline.
    // Even if you give it a complete set of buffers for the whole image,
    // it will only ever decompress a few lines at a time. For best 
    // performance, you should pass it an array with cinfo.rec_outbuf_height
    // scanline buffers. rec_outbuf_height is typically 1, 2, or 4, and 
    // at the default high quality decompression setting is always 1.
    while (cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char* bufferArray[1];
        bufferArray[0] = mat.data +
            (cinfo.output_scanline) * rowStride;

        jpeg_read_scanlines(&cinfo, bufferArray, 1);
    }
    // Once done reading *all* scanlines, release all internal buffers,
    // etc by calling jpeg_finish_decompress. This lets you go back and
    // reuse the same cinfo object with the same settings, if you
    // want to decompress several jpegs in a row.
    //
    // If you didn't read all the scanlines, but want to stop early,
    // you instead need to call jpeg_abort_decompress(&cinfo)
    jpeg_finish_decompress(&cinfo);

    // At this point, optionally go back and either load a new jpg into
    // the jpg_buffer, or define a new jpeg_mem_src, and then start 
    // another decompress operation.

    // Once you're really really done, destroy the object to free everything
    jpeg_destroy_decompress(&cinfo);
}


slideio::NDPITIFFKeeper::NDPITIFFKeeper(libtiff::TIFF* hfile) : m_hFile(hfile)
{
}


NDPITIFFKeeper::~NDPITIFFKeeper()
{
    if (m_hFile)
        libtiff::TIFFClose(m_hFile);
}

