// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "convertersvstools.hpp"

#include "converter.hpp"
#include "converterparameters.hpp"
#include "convertertools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"

void slideio::ConverterSVSTools::checkSVSRequirements(const CVScenePtr& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    for (int channel = 1; channel < numChannels; ++channel) {
        if (dt != scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS!";
        }
    }
}

std::string slideio::ConverterSVSTools::createDescription(const CVScenePtr& scene, const SVSConverterParameters& parameters)
{
    auto rect = scene->getRect();
    std::stringstream buff;
    buff << "SlideIO Library 2.0" << std::endl;
    buff << rect.width << "x" << rect.height;
    buff << "(" << parameters.getTileWidth() << "x" << parameters.getTileHeight() << ") ";
    if(parameters.getEncoding() == Compression::Jpeg) {
        buff << "JPEG/RGB " << "Q=" << ((SVSJpegConverterParameters&)parameters).getQuality();
    }
    else if(parameters.getEncoding() == Compression::Jpeg2000) {
        buff << "J2K";
    }
    buff << std::endl;
    double magn = scene->getMagnification();
    if (magn > 0) {
        buff << "AppMag = " << magn;
    }
    return buff.str();
}

void slideio::ConverterSVSTools::createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, const CVScenePtr& scene, const SVSConverterParameters& parameters)
{
    cv::Rect sceneRect = scene->getRect();
    cv::Size tileSize(parameters.getTileWidth(), parameters.getTileHeight());
    sceneRect.x = sceneRect.y = 0;
    cv::Size levelImageSize = ConverterTools::scaleSize(sceneRect.size(), zoomLevel);
    cv::Rect levelRect = ConverterTools::computeZoomLevelRect(sceneRect, tileSize, zoomLevel);

    slideio::TiffDirectory dir;
    dir.tiled = true;
    dir.channels = scene->getNumChannels();
    dir.dataType = scene->getChannelDataType(0);
    if(parameters.getEncoding()==Compression::Jpeg || parameters.getEncoding() == Compression::Jpeg2000) {
        dir.slideioCompression = parameters.getEncoding();
    }
    else {
        RAISE_RUNTIME_ERROR << "Unexpected compression for SVS converter: " << (int)parameters.getEncoding();
    }
    dir.width = levelImageSize.width;
    dir.height = levelImageSize.height;
    dir.tileWidth = tileSize.width;
    dir.tileHeight = tileSize.height;
    if(parameters.getEncoding()==Compression::Jpeg) {
        dir.compressionQuality = static_cast<const SVSJpegConverterParameters&>(parameters).getQuality();
    }
    if (zoomLevel == 0) {
        dir.description = createDescription(scene, parameters);
    }
    else {
        dir.description = "";
    }
    dir.res = scene->getResolution();
    file->setTags(dir, zoomLevel > 0);

    cv::Size sceneTileSize = slideio::ConverterTools::scaleSize(tileSize, zoomLevel, false);
    std::vector<uint8_t> buffer;
    if(parameters.getEncoding() == Compression::Jpeg2000) {
        int dataSize = tileSize.width * tileSize.height * scene->getNumChannels() * ImageTools::dataTypeSize(scene->getChannelDataType(0));
        buffer.resize(dataSize);
    }
    cv::Mat tile;
    int cvType = CVTools::toOpencvType(dir.dataType);
    const EncodeParameters& encoding = parameters.getEncodeParameters();
    for (int y = 0; y < sceneRect.height; y += sceneTileSize.height) {
        for (int x = 0; x < sceneRect.width; x += sceneTileSize.width) {
            cv::Rect blockRect(x, y, sceneTileSize.width, sceneTileSize.height);
            tile.create(tileSize.height, tileSize.width, CV_MAKE_TYPE(cvType,scene->getNumChannels()));
            tile.setTo(0);
            ConverterTools::readTile(scene, zoomLevel, blockRect, tile);
            cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
            file->writeTile(zoomLevelRect.x, zoomLevelRect.y, dir.slideioCompression, encoding, tile, buffer.data(), (int)buffer.size());
        }
    }
}

void slideio::ConverterSVSTools::createSVS(TIFFKeeperPtr& file, const CVScenePtr& scene, const SVSConverterParameters& parameters)
{
    if(parameters.getNumZoomLevels() <1) {
        RAISE_RUNTIME_ERROR << "Expected positive number of zoom levels. Received: " << parameters.getNumZoomLevels();
    }
    if(parameters.getTileWidth()<=0 || parameters.getTileHeight()<=0) {
        RAISE_RUNTIME_ERROR << "Expected not empty tile size. Received: "
            << parameters.getTileWidth() << "x" << parameters.getTileHeight();
    }
    if(!file->isValid()) {
        RAISE_RUNTIME_ERROR << "Received invalid tiff file handle!";
    }
    if(!scene) {
        RAISE_RUNTIME_ERROR << "Received invalid scene object!";
    }
    slideio::ConverterSVSTools::checkSVSRequirements(scene);
    for (int zoomLevel = 0; zoomLevel < parameters.getNumZoomLevels(); ++zoomLevel) {
        slideio::ConverterSVSTools::createZoomLevel(file, zoomLevel, scene, parameters);
    }
}
