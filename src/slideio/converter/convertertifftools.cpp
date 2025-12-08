// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "convertertifftools.hpp"

#include "converterparameters.hpp"
#include "convertertools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/tools.hpp"


using namespace slideio;
using namespace slideio::converter;

void ConverterTiffTools::createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, const std::string& description, const CVScenePtr& scene, ConverterParameters& parameters, const std::function<void(int, int)>& cb)
{
    cv::Rect sceneRect = scene->getRect();
    sceneRect.x = sceneRect.y = 0;
    if (parameters.getRect().valid()) {
        const auto& block = parameters.getRect();
        sceneRect.x = block.x;
        sceneRect.y = block.y;
        sceneRect.width = block.width;
        sceneRect.height = block.height;
    }

    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(parameters.getContainerParameters());
    cv::Size tileSize(tiffParams->getTileWidth(), tiffParams->getTileHeight());
    cv::Size levelImageSize = ConverterTools::scaleSize(sceneRect.size(), zoomLevel);

    slideio::TiffDirectory dir;
    dir.tiled = true;
    dir.channels = scene->getNumChannels();
    dir.dataType = scene->getChannelDataType(0);
    if (parameters.getEncoding() == Compression::Jpeg || parameters.getEncoding() == Compression::Jpeg2000) {
        dir.slideioCompression = parameters.getEncoding();
    }
    else {
        RAISE_RUNTIME_ERROR << "Unexpected compression for SVS converter: " << (int)parameters.getEncoding();
    }
    dir.width = levelImageSize.width;
    dir.height = levelImageSize.height;
    dir.tileWidth = tileSize.width;
    dir.tileHeight = tileSize.height;
    if (parameters.getEncoding() == Compression::Jpeg) {
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(parameters.getEncodeParameters());
        dir.compressionQuality = jpegParams->getQuality();
    }
    if (!description.empty()) {
        dir.description = description;
    }

    dir.res = scene->getResolution();
    file->setTags(dir);
    if (parameters.getFormat()==OME_TIFF && zoomLevel==0) {
		const int numLevels = tiffParams->getNumZoomLevels();
        if (numLevels > 1) {
            file->initSubDirs(numLevels - 1);
        }
	}

    cv::Size sceneTileSize = ConverterTools::scaleSize(tileSize, zoomLevel, false);
    std::vector<uint8_t> buffer;
    if (parameters.getEncoding() == Compression::Jpeg2000) {
        int dataSize = tileSize.width * tileSize.height * scene->getNumChannels() * Tools::dataTypeSize(scene->getChannelDataType(0));
        buffer.resize(dataSize);
    }
    cv::Mat tile;
    int cvType = CVTools::toOpencvType(dir.dataType);
    std::shared_ptr<EncodeParameters> encoding = parameters.getEncodeParameters();
    int tileCount = 0;
    const int xEnd = sceneRect.x + sceneRect.width;
    const int yEnd = sceneRect.y + sceneRect.height;
    const int slice = parameters.getZSlice();
    const int frame = parameters.getTFrame();
    for (int y = sceneRect.y; y < yEnd; y += sceneTileSize.height) {
        for (int x = sceneRect.x; x < xEnd; x += sceneTileSize.width) {
            cv::Rect blockRect(x, y, sceneTileSize.width, sceneTileSize.height);
            ConverterTools::readTile(scene, zoomLevel, blockRect, slice, frame, tile);
            if (tile.rows != tileSize.height || tile.cols != tileSize.width) {
                RAISE_RUNTIME_ERROR << "Converter: Unexpected tile size ("
                    << tile.cols << ","
                    << tile.rows << "). Expected tile size: ("
                    << tileSize.width << ","
                    << tileSize.height << ").";
            }
            blockRect.x -= sceneRect.x;
            blockRect.y -= sceneRect.y;
            cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
            file->writeTile(zoomLevelRect.x, zoomLevelRect.y, dir.slideioCompression, *encoding, tile, buffer.data(), (int)buffer.size());
            tileCount++;
            if (cb) {
                cb(zoomLevel, tileCount);
            }
        }
    }
    file->writeDirectory();
}
