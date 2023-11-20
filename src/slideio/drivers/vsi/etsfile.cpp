// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/etsfile.hpp"

#include "vsistream.hpp"
#include "vsistruct.hpp"
#include "vsitools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;

slideio::vsi::EtsFile::EtsFile(const std::string& filePath) : m_filePath(filePath)
{
}

void slideio::vsi::EtsFile::read()
{
#if defined(WIN32)
    std::wstring filePathW = Tools::toWstring(m_filePath);
    std::ifstream ifs(filePathW, std::ios::binary);
#else
    std::ifstream ifs(m_filePath, std::ios::binary);
#endif
    vsi::VSIStream ets(ifs);
    vsi::EtsVolumeHeader header = {0};
    ets.read<vsi::EtsVolumeHeader>(header);
    if (strncmp((char*)header.magic, "SIS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'SIS', got: "
            << header.magic;
    }
    if (header.headerSize != 64) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected header size: 64, got: "
            << header.headerSize;
    }
    ets.setPos(header.additionalHeaderPos);
    ETSAdditionalHeader additionalHeader = {0};
    ets.read<vsi::ETSAdditionalHeader>(additionalHeader);
    if (strncmp((char*)additionalHeader.magic, "ETS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'ETS', got: "
            << header.magic;
    }
    m_dimensions.push_back(header.numDimensions);
    m_dataType = VSITools::toSlideioPixelType(additionalHeader.componentType);
    m_numChannels = static_cast<int>(additionalHeader.componentCount);
    m_colorSpace = static_cast<ColorSpace>(additionalHeader.colorSpace);
    m_compression = VSITools::toSlideioCompression(static_cast<vsi::Compression>(additionalHeader.format));
    m_compressionQuality = static_cast<int>(additionalHeader.quality);
    m_tileWidth = static_cast<int>(additionalHeader.sizeX);
    m_tileHeight = static_cast<int>(additionalHeader.sizeY);
    m_numZSlices = static_cast<int>(additionalHeader.sizeZ);
    std::memcpy(m_pixelInfoHints, additionalHeader.pixInfoHints, sizeof(m_pixelInfoHints));
    std::memcpy(m_backgroundColor, additionalHeader.background, sizeof(m_backgroundColor));
    m_usePyramid = additionalHeader.usePyramid != 0;


    ets.setPos(header.usedChunksPos);
    m_tiles.resize(header.numUsedChunks);
    const int dimensions = static_cast<int>(m_dimensions.back());
    for(uint chunk=0; chunk<header.numUsedChunks; ++chunk) {
        TileInfo& tileInfo = m_tiles[chunk];
        ets.skipBytes(4);
        tileInfo.coordinates.resize(dimensions);
        for(int i=0; i<dimensions; ++i) {
            tileInfo.coordinates[i] = ets.readValue<int32_t>();
        }
        tileInfo.offset = ets.readValue<int64_t>();
        tileInfo.size = ets.readValue<uint32_t>();
        ets.skipBytes(4);
    }

    int maxResolution = 0;

    if (m_usePyramid) {
        for (auto t : m_tiles) {
            if (t.coordinates[t.coordinates.size() - 1] > maxResolution) {
                maxResolution = t.coordinates[t.coordinates.size() - 1];
            }
        }
    }

    maxResolution++;

    std::vector<int> maxX(maxResolution);
    std::vector<int> maxY(maxResolution);
    std::vector<int> maxZ(maxResolution);
    std::vector<int> maxC(maxResolution);
    std::vector<int> maxT(maxResolution);

    //HashMap<String, Integer> dimOrder = pyramids.get(s).dimensionOrdering;

    //for (TileCoordinate t : tmpTiles) {
    //    int resolution = usePyramid ? t.coordinate[t.coordinate.length - 1] : 0;

    //    Integer tv = dimOrder.get("T");
    //    Integer zv = dimOrder.get("Z");
    //    Integer cv = dimOrder.get("C");

    //    int tIndex = tv == null ? -1 : tv + 2;
    //    int zIndex = zv == null ? -1 : zv + 2;
    //    int cIndex = cv == null ? -1 : cv + 2;

    //    if (usePyramid && tIndex == t.coordinate.length - 1) {
    //        tv = null;
    //        tIndex = -1;
    //    }
    //    if (usePyramid && zIndex == t.coordinate.length - 1) {
    //        zv = null;
    //        zIndex = -1;
    //    }

    //    int upperLimit = usePyramid ? t.coordinate.length - 1 : t.coordinate.length;
    //    if ((tIndex < 0 || tIndex >= upperLimit) &&
    //        (zIndex < 0 || zIndex >= upperLimit) &&
    //        (cIndex < 0 || cIndex >= upperLimit))
    //    {
    //        tIndex--;
    //        zIndex--;
    //        cIndex--;
    //        if (dimOrder.containsKey("T")) {
    //            dimOrder.put("T", tIndex - 2);
    //        }
    //        if (dimOrder.containsKey("Z")) {
    //            dimOrder.put("Z", zIndex - 2);
    //        }
    //        if (dimOrder.containsKey("C")) {
    //            dimOrder.put("C", cIndex - 2);
    //        }
    //    }

    //    if (tv == null && zv == null) {
    //        if (t.coordinate.length > 4 && cv == null) {
    //            cIndex = 2;
    //            dimOrder.put("C", cIndex - 2);
    //        }

    //        if (t.coordinate.length > 4) {
    //            if (cv == null) {
    //                tIndex = 3;
    //            }
    //            else {
    //                tIndex = cIndex + 2;
    //            }
    //            if (tIndex < t.coordinate.length) {
    //                dimOrder.put("T", tIndex - 2);
    //            }
    //            else {
    //                tIndex = -1;
    //            }
    //        }

    //        if (t.coordinate.length > 5) {
    //            if (cv == null) {
    //                zIndex = 4;
    //            }
    //            else {
    //                zIndex = cIndex + 1;
    //            }
    //            if (zIndex < t.coordinate.length) {
    //                dimOrder.put("Z", zIndex - 2);
    //            }
    //            else {
    //                zIndex = -1;
    //            }
    //        }
    //    }

    //    if (t.coordinate[0] > maxX[resolution]) {
    //        maxX[resolution] = t.coordinate[0];
    //    }
    //    if (t.coordinate[1] > maxY[resolution]) {
    //        maxY[resolution] = t.coordinate[1];
    //    }

    //    if (tIndex >= 0 && t.coordinate[tIndex] > maxT[resolution]) {
    //        maxT[resolution] = t.coordinate[tIndex];
    //    }
    //    if (zIndex >= 0 && t.coordinate[zIndex] > maxZ[resolution]) {
    //        maxZ[resolution] = t.coordinate[zIndex];
    //    }
    //    if (cIndex >= 0 && t.coordinate[cIndex] > maxC[resolution]) {
    //        maxC[resolution] = t.coordinate[cIndex];
    //    }
    //}

    //if (pyramids.get(s).width != null) {
    //    ms.sizeX = pyramids.get(s).width;
    //}
    //if (pyramids.get(s).height != null) {
    //    ms.sizeY = pyramids.get(s).height;
    //}
    //ms.sizeZ = maxZ[0] + 1;
    //if (maxC[0] > 0) {
    //    ms.sizeC *= (maxC[0] + 1);
    //}
    //ms.sizeT = maxT[0] + 1;
    //if (ms.sizeZ == 0) {
    //    ms.sizeZ = 1;
    //}
    //ms.imageCount = ms.sizeZ * ms.sizeT;
    //if (maxC[0] > 0) {
    //    ms.imageCount *= (maxC[0] + 1);
    //}

    //if (maxY[0] >= 1) {
    //    rows.add(maxY[0] + 1);
    //}
    //else {
    //    rows.add(1);
    //}
    //if (maxX[0] >= 1) {
    //    cols.add(maxX[0] + 1);
    //}
    //else {
    //    cols.add(1);
    //}

    //ArrayList<TileCoordinate> map = new ArrayList<TileCoordinate>();
    //for (int i = 0; i < tmpTiles.size(); i++) {
    //    map.add(tmpTiles.get(i));
    //}
    //tileMap.add(map);

    //ms.pixelType = convertPixelType(pixelType);
    //if (usePyramid) {
    //    int finalResolution = 1;
    //    int initialCoreSize = core.size();
    //    for (int i = 1; i < maxResolution; i++) {
    //        CoreMetadata newResolution = new CoreMetadata(ms);

    //        int previousX = core.get(core.size() - 1).sizeX;
    //        int previousY = core.get(core.size() - 1).sizeY;
    //        int maxSizeX = tileX.get(tileX.size() - 1) * (maxX[i] < 1 ? 1 : maxX[i] + 1);
    //        int maxSizeY = tileY.get(tileY.size() - 1) * (maxY[i] < 1 ? 1 : maxY[i] + 1);

    //        newResolution.sizeX = previousX / 2;
    //        if (previousX % 2 == 1 && newResolution.sizeX < maxSizeX) {
    //            newResolution.sizeX++;
    //        }
    //        else if (newResolution.sizeX > maxSizeX) {
    //            newResolution.sizeX = maxSizeX;
    //        }
    //        newResolution.sizeY = previousY / 2;
    //        if (previousY % 2 == 1 && newResolution.sizeY < maxSizeY) {
    //            newResolution.sizeY++;
    //        }
    //        else if (newResolution.sizeY > maxSizeY) {
    //            newResolution.sizeY = maxSizeY;
    //        }
    //        newResolution.sizeZ = maxZ[i] + 1;
    //        if (maxC[i] > 0 && newResolution.sizeC != (maxC[i] + 1)) {
    //            newResolution.sizeC *= (maxC[i] + 1);
    //        }
    //        newResolution.sizeT = maxT[i] + 1;
    //        if (newResolution.sizeZ == 0) {
    //            newResolution.sizeZ = 1;
    //        }
    //        newResolution.imageCount = newResolution.sizeZ * newResolution.sizeT;
    //        if (maxC[i] > 0) {
    //            newResolution.imageCount *= (maxC[i] + 1);
    //        }

    //        newResolution.metadataComplete = true;
    //        newResolution.dimensionOrder = "XYCZT";

    //        core.add(newResolution);

    //        rows.add(maxY[i] >= 1 ? maxY[i] + 1 : 1);
    //        cols.add(maxX[i] >= 1 ? maxX[i] + 1 : 1);

    //        fileMap.put(core.size() - 1, file);
    //        finalResolution = core.size() - initialCoreSize + 1;

    //        tileX.add(tileX.get(tileX.size() - 1));
    //        tileY.add(tileY.get(tileY.size() - 1));
    //        compressionType.add(compressionType.get(compressionType.size() - 1));
    //        tileMap.add(map);
    //        nDimensions.add(nDimensions.get(nDimensions.size() - 1));
    //        tileOffsets.add(tileOffsets.get(tileOffsets.size() - 1));
    //        backgroundColor.put(core.size() - 1, color);
    //    }

    //    ms.resolutionCount = finalResolution;
    //}
}
