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

void slideio::ConverterSVSTools::createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, const CVScenePtr& scene, SVSConverterParameters& parameters, const std::function<void(int, int)>& cb)
{
    cv::Rect sceneRect = scene->getRect();
    sceneRect.x = sceneRect.y = 0;
    if(parameters.getRect().isValid()) {
        const auto& block = parameters.getRect();
        sceneRect.x = block.x;
        sceneRect.y = block.y;
        sceneRect.width = block.width;
        sceneRect.height = block.height;
    }

    cv::Size tileSize(parameters.getTileWidth(), parameters.getTileHeight());
    cv::Size levelImageSize = ConverterTools::scaleSize(sceneRect.size(), zoomLevel);
    
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
    int tileCount = 0;
    const int xEnd = sceneRect.x + sceneRect.width;
    const int yEnd = sceneRect.y + sceneRect.height;
    for (int y = sceneRect.y; y < yEnd; y += sceneTileSize.height) {
        for (int x = sceneRect.x; x < xEnd; x += sceneTileSize.width) {
            cv::Rect blockRect(x, y, sceneTileSize.width, sceneTileSize.height);
            tile.create(tileSize.height, tileSize.width, CV_MAKE_TYPE(cvType,scene->getNumChannels()));
            tile.setTo(0);
            ConverterTools::readTile(scene, zoomLevel, blockRect, 0, 0, tile);
            blockRect.x -= sceneRect.x;
            blockRect.y -= sceneRect.y;
            cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
            file->writeTile(zoomLevelRect.x, zoomLevelRect.y, dir.slideioCompression, encoding, tile, buffer.data(), (int)buffer.size());
            tileCount++;
            if(cb) {
                cb(zoomLevel, tileCount);
            }
        }
    }
}

void slideio::ConverterSVSTools::createSVS(TIFFKeeperPtr& file, const CVScenePtr& scene, SVSConverterParameters& parameters, ConverterCallback cb)
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
    checkSVSRequirements(scene);

    int tileCount = 0;

    if(cb) {
        cv::Rect sceneRect = scene->getRect();
        cv::Size sceneSize = scene->getRect().size();
        if(parameters.getRect().isValid()) {
            const auto& block = parameters.getRect();
            sceneSize.width = block.width;
            sceneSize.height = block.height;
        }
        for(int zoomLevel=0; zoomLevel<parameters.getNumZoomLevels(); ++zoomLevel) {
            const cv::Size tileSize(parameters.getTileWidth(), parameters.getTileHeight());
            const cv::Size levelImageSize = ConverterTools::scaleSize(sceneSize, zoomLevel);
            const int sx = (levelImageSize.width - 1) / tileSize.width + 1;
            const int sy = (levelImageSize.height - 1) / tileSize.height + 1;
            tileCount += sx * sy;
        }
    }

    int percents = 0;
    int processedTiles = 0;
    auto lambda = [cb, tileCount, &processedTiles, &percents](int, int)
    {
        const int newPercents = (processedTiles * 100) / tileCount;
        processedTiles++;
        if(newPercents != percents) {
            cb(newPercents);
            percents = newPercents;
        }
    };

    for (int zoomLevel = 0; zoomLevel < parameters.getNumZoomLevels(); ++zoomLevel) {
        if(cb) {
            createZoomLevel(file, zoomLevel, scene, parameters, lambda);
        }
        else {
            createZoomLevel(file, zoomLevel, scene, parameters, nullptr);
        }
    }
}
