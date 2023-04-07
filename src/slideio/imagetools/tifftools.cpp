// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/imagetools/tifftools.hpp"

#include <codecvt>

#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include <opencv2/core.hpp>
#include <boost/format.hpp>
#include "slideio/imagetools/libtiff.hpp"
#include <boost/filesystem.hpp>
#include "slideio/core/tools/tools.hpp"

using namespace slideio;

static slideio::DataType dataTypeFromTIFFDataType(libtiff::TIFFDataType dt)
{
    switch(dt)
    {
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
        RAISE_RUNTIME_ERROR << "Invalid tiff data type " << dt;
    }
}

libtiff::TIFFDataType TIFFDataTypeFromDataType(slideio::DataType dt)
{
    switch (dt)
    {
    case DataType::DT_None:
        return libtiff::TIFF_NOTYPE;
    case DataType::DT_Byte:
        return libtiff::TIFF_BYTE;
    case DataType::DT_UInt16:
        return libtiff::TIFF_SHORT;
    case DataType::DT_Int8:
        return libtiff::TIFF_SBYTE;
    case DataType::DT_Unknown:
        return libtiff::TIFF_UNDEFINED;
    case DataType::DT_Int16:
        return libtiff::TIFF_SSHORT;
    case DataType::DT_Float32:
        return libtiff::TIFF_FLOAT;
    case DataType::DT_Float64:
        return libtiff::TIFF_DOUBLE;
    default:
        RAISE_RUNTIME_ERROR << "Invalid data type: " << dt;
    }
}

static slideio::Compression compressTiffToSlideio(int tiffCompression)
{
    Compression compression = Compression::Unknown;
    switch(tiffCompression)
    {
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
    case 0x80ED:
    case 0x80EB:
    case 0x80EA:
        compression = Compression::Jpeg2000;
        break;
    case 0x8799:
        compression = Compression::NikonNEF;
        break;
    case 0x879b:
        compression = Compression::JBIG2;
        break;
    default:
        RAISE_RUNTIME_ERROR << "Invalid compression type: " << tiffCompression;
    }
    return compression;
}

int compressSlideioToTiff(slideio::Compression compression)
{
    int tiffCompression = -1;
    switch (compression)
    {
    case slideio::Compression::Uncompressed:
        tiffCompression = 0x1;
        break;
    case slideio::Compression::HuffmanRL:
        tiffCompression = 0x2;
        break;
    case slideio::Compression::CCITT_T4:
        tiffCompression = 0x3;
        break;
    case slideio::Compression::CCITT_T6:
        tiffCompression = 0x4;
        break;
    case slideio::Compression::LZW:
        tiffCompression = 0x5;
        break;
    case slideio::Compression::JpegOld:
        tiffCompression = 0x6;
        break;
    case slideio::Compression::Jpeg:
        tiffCompression = 0x7;
        break;
    case slideio::Compression::Zlib:
        tiffCompression = 0x8;
        break;
    case slideio::Compression::JBIG85:
        tiffCompression = 0x9;
        break;
    case slideio::Compression::JBIG43:
        tiffCompression = 0xa;
        break;
    case slideio::Compression::NextRLE:
        tiffCompression = 0x7ffe;
        break;
    case slideio::Compression::PackBits:
        tiffCompression = 0x8005;
        break;
    case slideio::Compression::ThunderScanRLE:
        tiffCompression = 0x8029;
        break;
    case slideio::Compression::RasterPadding:
        tiffCompression = 0x807f;
        break;
    case slideio::Compression::RLE_LW:
        tiffCompression = 0x8080;
        break;
    case slideio::Compression::RLE_HC:
        tiffCompression = 0x8081;
        break;
    case slideio::Compression::RLE_BL:
        tiffCompression = 0x8082;
        break;
    case slideio::Compression::PKZIP:
        tiffCompression = 0x80b2;
        break;
    case slideio::Compression::KodakDCS:
        tiffCompression = 0x80b3;
        break;
    case slideio::Compression::JBIG:
        tiffCompression = 0x8765;
        break;
    case slideio::Compression::Jpeg2000:
        //tiffCompression = 0x8798;
        tiffCompression = 33005;
        break;
    case slideio::Compression::NikonNEF:
        tiffCompression = 0x8799;
        break;
    case slideio::Compression::JBIG2:
        tiffCompression = 0x879b;
        break;
    default:
        RAISE_RUNTIME_ERROR << "Invalid compression type: " << compression;
    }
    return tiffCompression;
}

//std::ostream& operator<<(std::ostream& os, const TiffDirectory& dir)
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
//std::ostream& operator<<(std::ostream& os, const std::vector<slideio::TiffDirectory>& dirs) {
//    os << "---Tiff directories. Size:" << dirs.size() << std::endl;
//    std::vector<slideio::TiffDirectory>::const_iterator iDir;
//    for (iDir = dirs.begin(); iDir != dirs.end(); ++iDir) {
//        auto& dir = *iDir;
//        os << dir;
//    }
//    return os;
//}

libtiff::TIFF* slideio::TiffTools::openTiffFile(const std::string& path, bool readOnly)
{
    namespace fs = boost::filesystem;
    libtiff::TIFF* hfile(nullptr);
#if defined(WIN32)
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wsPath = converter.from_bytes(path);
    boost::filesystem::path filePath(wsPath);
    if (readOnly && !fs::exists(wsPath)) {
        RAISE_RUNTIME_ERROR << "File " << path << " does not exist";
    }
    hfile = libtiff::TIFFOpenW(wsPath.c_str(), readOnly ? "r" : "w");
#else
    boost::filesystem::path filePath(path);
    if (readOnly && !fs::exists(filePath)) {
        RAISE_RUNTIME_ERROR << "File " << path << " does not exist";
    }
    hfile = libtiff::TIFFOpen(path.c_str(),  readOnly ? "r" : "w");
#endif
    if(!hfile) {
        RAISE_RUNTIME_ERROR << "Cannot open file " << path << " for " << (readOnly ? "reading" : "writing.");
    }
    SLIDEIO_LOG(INFO) << "File " << path << " successfully opened for " << (readOnly ? "reading" : "writing");
    return hfile;
}

void slideio::TiffTools::closeTiffFile(libtiff::TIFF* file)
{
    if(file)
        libtiff::TIFFClose(file);
}

void  slideio::TiffTools::scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir)
{
    if (libtiff::TIFFCurrentDirectory(tiff) != dirIndex) {
        libtiff::TIFFSetDirectory(tiff, (short)dirIndex);
    }
    if(dirOffset)
        libtiff::TIFFSetSubDirectory(tiff, dirOffset);

    dir.dirIndex = dirIndex;
    dir.offset = dirOffset;
    dir.compressionQuality = 0;

    char *description(nullptr);
    short dirchnls(0), dirbits(0);
    uint16_t compress(0);
    short  planar_config(0);
    int width(0), height(0), tile_width(0), tile_height(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &dirchnls);
    libtiff::TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &dirbits);
    libtiff::TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compress);
    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    libtiff::TIFFGetField(tiff,TIFFTAG_TILEWIDTH ,&tile_width);
    libtiff::TIFFGetField(tiff,TIFFTAG_TILELENGTH,&tile_height);
    libtiff::TIFFGetField(tiff, TIFFTAG_IMAGEDESCRIPTION, &description);
    libtiff::TIFFGetField(tiff, TIFFTAG_PLANARCONFIG ,&planar_config);
    float resx(0), resy(0);
    uint16_t units(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_XRESOLUTION, &resx);
    libtiff::TIFFGetField(tiff, TIFFTAG_YRESOLUTION, &resy);
    libtiff::TIFFGetField(tiff, TIFFTAG_RESOLUTIONUNIT, &units);
    dir.interleaved = planar_config==PLANARCONFIG_CONTIG;
    float posx(0), posy(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_XPOSITION, &posx);
    libtiff::TIFFGetField(tiff, TIFFTAG_YPOSITION, &posy);
    int32_t rowsPerStripe(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStripe);
    libtiff::TIFFDataType dt(libtiff::TIFF_NOTYPE);
    libtiff::TIFFGetField(tiff, TIFFTAG_DATATYPE, &dt);
    int32_t compressionQuality = 0;
    libtiff::TIFFGetField(tiff, TIFFTAG_JPEGQUALITY, &compressionQuality);
    short ph(0);
    libtiff::TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &ph);
    dir.photometric = ph;
    dir.stripSize = (int)libtiff::TIFFStripSize(tiff);
    dir.dataType = dataTypeFromTIFFDataType(dt);
    short YCbCrSubsampling[2] = { 2,2 };
    libtiff::TIFFGetField(tiff, TIFFTAG_YCBCRSUBSAMPLING, &YCbCrSubsampling[0], &YCbCrSubsampling[0]);
    dir.YCbCrSubsampling[0] = YCbCrSubsampling[0];
    dir.YCbCrSubsampling[1] = YCbCrSubsampling[1];

    if(units==RESUNIT_INCH && resx>0 && resy>0){
        dir.res.x = 0.01/resx;
        dir.res.y = 0.01/resy;
    }
    else if(units==RESUNIT_INCH && resx>0 && resy>0){
        dir.res.x = 0.0254/resx;
        dir.res.y = 0.0254/resy;
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
    if(description)
        dir.description = description;
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
    dir.compressionQuality = compressionQuality;


}

void slideio::TiffTools::scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir)
{
    if(libtiff::TIFFCurrentDirectory(tiff) != dirIndex) {
        libtiff::TIFFSetDirectory(tiff, (short)dirIndex);
    }
    if(dirOffset>0)
        libtiff::TIFFSetSubDirectory(tiff, dirOffset);

    dir.dirIndex = dirIndex;
    dir.offset = dirOffset;

    scanTiffDirTags(tiff, dirIndex, dirOffset, dir);
    dir.offset = 0;
    long subdirs(0);
    int64 *offsets_raw(nullptr);
    if(libtiff::TIFFGetField(tiff, TIFFTAG_SUBIFD, &subdirs, &offsets_raw))
    {
        std::vector<int64> offsets(offsets_raw, offsets_raw+subdirs);
        if(subdirs>0)
        {
            dir.subdirectories.resize(subdirs);
        }
        for(int subdir=0; subdir<subdirs; subdir++)
        {
            if(libtiff::TIFFSetSubDirectory(tiff, offsets[subdir]))
            {
                scanTiffDirTags(tiff, dirIndex, dir.subdirectories[subdir].offset, dir.subdirectories[subdir]);
            }
        }
    }
}

void slideio::TiffTools::scanFile(libtiff::TIFF* tiff, std::vector<TiffDirectory>& directories)
{
    int dirs = libtiff::TIFFNumberOfDirectories(tiff);
    directories.resize(dirs);
    for(int dir=0; dir<dirs; dir++)
    {
        directories[dir].dirIndex = dir;
        scanTiffDir(tiff, dir, 0, directories[dir]);
    }
}

void slideio::TiffTools::scanFile(const std::string& filePath, std::vector<TiffDirectory>& directories)
{
    libtiff::TIFF* file(nullptr);
    try
    {
        file = openTiffFile(filePath);
        if(file==nullptr)
            throw std::runtime_error(std::string("TiffTools: cannot open tiff file") + filePath);
        scanFile(file, directories);
    }
    catch(std::exception& ex)
    {
        if(file)
            closeTiffFile(file);
        throw ex;
    }
    if(file)
        closeTiffFile(file);
}

void TiffTools::readNotRGBStripedDir(libtiff::TIFF* file, const TiffDirectory& dir, cv::_OutputArray output)
{
    std::vector<uint8_t> rgbaRaster(4 * dir.rowsPerStrip * dir.width);

    int buff_size = dir.width * dir.height * dir.channels * ImageTools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = { dir.width, dir.height };
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

    for (int strip = 0, row = 0; row < dir.height; strip++, row += dir.rowsPerStrip, buffBegin += stripBuffSize)
    {
        if ((strip + stripBuffSize) > buff_size)
            stripBuffSize = buff_size - strip;

            int stripeRows = dir.rowsPerStrip;
            if (row + stripeRows > dir.height) {
                stripeRows = dir.height - row;
            }

            int read = libtiff::TIFFReadRGBAStrip(file, row, (uint32_t*)rgbaRaster.data());
            if (read != 1) {
                throw std::runtime_error("TiffTools: Error by reading of tif strip");
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

void slideio::TiffTools::readRegularStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output)
{

    int buff_size = dir.width * dir.height * dir.channels * ImageTools::dataTypeSize(dir.dataType);
    cv::Size sizeImage = { dir.width, dir.height };
    slideio::DataType dt = dir.dataType;
    output.create(sizeImage, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    cv::Mat imageRaster = output.getMat();
    setCurrentDirectory(file, dir);
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(file, dir.offset);
    }
    uint8_t* buffBegin = imageRaster.data;
    int stripBuffSize = dir.stripSize;

    for (int strip = 0, row = 0; row < dir.height; strip++, row += dir.rowsPerStrip, buffBegin += stripBuffSize)
    {
        if ((strip + stripBuffSize) > buff_size)
            stripBuffSize = buff_size - strip;

        int read = (int)libtiff::TIFFReadEncodedStrip(file, strip, buffBegin, stripBuffSize);
        if (read <= 0) {
            throw std::runtime_error("TiffTools: Error by reading of tif strip");
        }
    }
    return;
}


void slideio::TiffTools::readStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output)
{
    if(!dir.interleaved)
        throw std::runtime_error("Planar striped images are not supported");

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


void slideio::TiffTools::readTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    if(!dir.tiled){
        throw std::runtime_error("TiffTools: Expected tiled configuration, received striped");
    }
    setCurrentDirectory(hFile, dir);

    if(dir.compression==34712 || dir.compression==33003 || dir.compression == 33005)
    {
        readJ2KTile(hFile, dir, tile, channelIndices, output);
    }
    else if(dir.photometric==6 || dir.photometric==8 || dir.photometric==9 || dir.photometric==10)
    {
        readNotRGBTile(hFile, dir, tile, channelIndices, output);
    }
    else
    {
        readRegularTile(hFile, dir, tile, channelIndices, output);
    }
}

void slideio::TiffTools::readRegularTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output)
{
    cv::Size tileSize = { dir.tileWidth, dir.tileHeight };
    slideio::DataType dt = dir.dataType;
    cv::Mat tileRaster;
    tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
    setCurrentDirectory(hFile, dir);
    if (dir.offset > 0) {
        libtiff::TIFFSetSubDirectory(hFile, dir.offset);
    }
    // if(dir.compression==7) {
    //     std::vector<uint8_t> buff(libtiff::TIFFTileSize(hFile));
    //     int64_t size = libtiff::TIFFReadRawTile(hFile, tile, buff.data(), buff.size());
    //     buff.resize(size);
    //     ImageTools::decodeJpegStream(buff.data(), buff.size(), tileRaster);
    // }
    else 
    {
        uint8_t* buff_begin = tileRaster.data;
        auto buf_size = tileRaster.total() * tileRaster.elemSize();
        auto readBytes = libtiff::TIFFReadEncodedTile(hFile, tile, buff_begin, buf_size);
        if (readBytes <= 0)
            throw std::runtime_error(
                (boost::format(
                    "TiffTools: error reading encoded tiff tile %1% of directory %2%."
                    "Compression: %3%") % tile % dir.dirIndex % dir.compression).str());
    }
    if(channelIndices.empty())
    {
        tileRaster.copyTo(output);
    }
    else if(channelIndices.size()==1)
    {
        cv::extractChannel(tileRaster, output, channelIndices[0]);
    }
    else
    {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for(int channelIndex : channelIndices)
        {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, output);
    }
}


void slideio::TiffTools::readJ2KTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
                                     const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const auto tileSize = libtiff::TIFFTileSize(hFile);
    std::vector<uint8_t> rawTile(tileSize);
    if(dir.interleaved)
    {
        // process interleaved channels
        libtiff::tmsize_t readBytes = libtiff::TIFFReadRawTile(hFile, tile, rawTile.data(), (int)rawTile.size());
        if(readBytes<=0){
            throw std::runtime_error("TiffTools: Error reading raw tile");
        }
        bool yuv = dir.compression==33003;
        slideio::ImageTools::decodeJp2KStream(rawTile, output, channelIndices, yuv);
    }
    else if(channelIndices.size()==1)
    {
        // process a single planar channel
        throw std::runtime_error("Not implemented");
    }
    else
    {
        throw std::runtime_error("Not implemented");
        //// process planar channels
        //std::vector<cv::Mat> channelRasters;
        //for(const auto& channelIndex : channelIndices)
        //{
        //
        //}
    }
}

void TiffTools::readNotRGBTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    cv::Size tileSize = { dir.tileWidth, dir.tileHeight };
    slideio::DataType dt = dir.dataType;
    cv::Mat tileRaster;
    tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), 4));
    setCurrentDirectory(hFile, dir);
    uint32_t* buffBegin = reinterpret_cast<uint32_t*>(tileRaster.data);

    int cols = (dir.width - 1) / dir.tileWidth + 1;
    int rows = (dir.height - 1) / dir.tileHeight + 1;
    int row = tile / cols;
    int col = tile - row * cols;
    int tileX = col * dir.tileWidth;
    int tileY = row * dir.tileHeight;
    auto readBytes = libtiff::TIFFReadRGBATile(hFile, tileX, tileY, buffBegin);
    if (readBytes <= 0)
        throw std::runtime_error(
            (boost::format(
                "TiffTools: error reading encoded tiff tile %1% of directory %2%."
                "Compression: %3%") % tile % dir.dirIndex % dir.compression).str());

    cv::Mat flipped;
    if (channelIndices.empty())
    {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(3);
        for (int channelIndex=0; channelIndex<3; ++channelIndex)
        {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndex);
        }
        cv::merge(channelRasters, flipped);
    }
    else if (channelIndices.size() == 1)
    {
        cv::extractChannel(tileRaster, flipped, channelIndices[0]);
    }
    else
    {
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for (int channelIndex : channelIndices)
        {
            cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
        }
        cv::merge(channelRasters, flipped);
    }
    cv::flip(flipped, output, 0);
}

uint16_t TiffTools::slideioDataTypeToTiffDataType(slideio::DataType dt)
{
    uint16_t tiffDataType = libtiff::TIFF_NOTYPE;

    switch (dt) {
    case DataType::DT_Byte:
        tiffDataType = libtiff::TIFF_BYTE;
        break;
    case DataType::DT_Int16:
        tiffDataType = libtiff::TIFF_SSHORT;
        break;
    case DataType::DT_UInt16:
        tiffDataType = libtiff::TIFF_SHORT;
        break;
    case DataType::DT_Int8:
        tiffDataType = libtiff::TIFF_SBYTE;
        break;
    case DataType::DT_Int32:
        tiffDataType = libtiff::TIFF_SBYTE;
        break;
    case DataType::DT_Float32:
        tiffDataType = libtiff::TIFF_FLOAT;
        break;
    case DataType::DT_Float64:
        tiffDataType = libtiff::TIFF_DOUBLE;
        break;
    default:
        RAISE_RUNTIME_ERROR << "Converter: Unsupported type by TIFF: " << (int)dt;
    }

    return tiffDataType;
}

void TiffTools::writeDirectory(libtiff::TIFF* tiff)
{
    libtiff::TIFFWriteDirectory(tiff);
}

inline uint16_t bitsPerSampleDataType(DataType dt)
{
    int ds = ImageTools::dataTypeSize(dt);
    return (uint16_t)(ds * 8);
}

uint16_t computeDirectoryPhotometric(TiffDirectory dir)
{
    const DataType dt = dir.dataType;
    const int numChannels = dir.channels;
    uint16_t photometric = PHOTOMETRIC_SEPARATED;
    if(dt==DataType::DT_Byte && numChannels==3) {
        photometric = PHOTOMETRIC_RGB;
    }
    return photometric;
}


void TiffTools::setTags(libtiff::TIFF* tiff, const TiffDirectory& dir, bool newDirectory)
{
    if(newDirectory) {
        writeDirectory(tiff);
    }

    const uint16_t numChannels = (uint16_t)dir.channels;
    libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, numChannels);
    const uint16_t bitsPerSample = bitsPerSampleDataType(dir.dataType);
    libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
    const uint16_t compression = compressSlideioToTiff(dir.slideioCompression);
    libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, dir.width);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, dir.height);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, dir.tileWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, dir.tileHeight);;
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, dir.description.c_str());
    libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, ((numChannels==1)?2:1));
    auto res = dir.res;
    libtiff::TIFFSetField(tiff, TIFFTAG_XRESOLUTION, (float)res.x);
    libtiff::TIFFSetField(tiff, TIFFTAG_YRESOLUTION, (float)res.y);
    libtiff::TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XPOSITION, 0);
    libtiff::TIFFSetField(tiff, TIFFTAG_YPOSITION, 0);
    uint16_t dt = TIFFDataTypeFromDataType(dir.dataType);
    libtiff::TIFFSetField(tiff, TIFFTAG_DATATYPE, dt);
    uint16_t phm = computeDirectoryPhotometric(dir);
    libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, phm);
    libtiff::TIFFSetField(tiff, TIFFTAG_JPEGQUALITY, dir.compressionQuality);
}

void TiffTools::writeTile(libtiff::TIFF* tiff, int x, int y, Compression compression, 
                        const cv::Mat& tileRaster, const EncodeParameters& parameters,
                        uint8_t* buffer, int bufferSize)
{
    if(compression==Compression::Jpeg) {
        const uint32_t tile = libtiff::TIFFComputeTile(tiff, x, y, 0, 0);
        const size_t dataSize = tileRaster.total() * tileRaster.elemSize();
        const int64_t written = libtiff::TIFFWriteEncodedTile(tiff, tile, tileRaster.data, dataSize);
        if ((int64_t)dataSize != written) {
            RAISE_RUNTIME_ERROR << "Error by writing tiff tile";
        }
    }
    else if(compression==Compression::Jpeg2000) {
        const uint32_t tile = libtiff::TIFFComputeTile(tiff, x, y, 0, 0);
        std::vector<uint8_t> buff;
        if(buffer==nullptr || bufferSize<=0) {
            const size_t dataSize = tileRaster.total() * tileRaster.elemSize();
            buff.resize(dataSize);
            buffer = buff.data();
            bufferSize = (int)dataSize;
        }
        const JP2KEncodeParameters& jp2param = 
            static_cast<const JP2KEncodeParameters &>(parameters);
        int dataSize = ImageTools::encodeJp2KStream(tileRaster, buffer, bufferSize, jp2param);
        if(dataSize <= 0) {
            RAISE_RUNTIME_ERROR << "JPEG 2000 Encoding failed";
        }
        libtiff::TIFFWriteRawTile(tiff, tile, buffer, dataSize);
    }
    else {
        RAISE_RUNTIME_ERROR << "Unsupported compression: " << compression;
    }
    // uint32_t tile = libtiff::TIFFComputeTile(tiff, x, y, 0, 0);
    // int64_t written = libtiff::TIFFWriteRawTile(tiff, tile, encodedStream.data(), encodedStream.size());
    // if((int64_t)encodedStream.size() != written) {
    //      RAISE_RUNTIME_ERROR << "Error by writing tiff tile";
    //}
}


void slideio::TiffTools::setCurrentDirectory(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir)
{
    if(libtiff::TIFFCurrentDirectory(hFile) != static_cast<uint16_t>(dir.dirIndex)) {
        if (!libtiff::TIFFSetDirectory(hFile, static_cast<uint16_t>(dir.dirIndex))) {
            throw std::runtime_error("TiffTools: error by setting current directory");
        }
    }
    if(dir.offset>0){
        if(!libtiff::TIFFSetSubDirectory(hFile, dir.offset)){
            throw std::runtime_error("TiffTools: error by setting current sub-directory");
        }
    }
}
