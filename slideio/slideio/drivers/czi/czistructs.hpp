// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_czistructs_HPP
#define OPENCV_slideio_czistructs_HPP
#include <cstdint>
#include <string>
#include <vector>

namespace slideio
{
    #pragma pack(push,1)
    struct SegmentHeader
    {
        char SID[16];
        uint64_t allocatedSize;
        uint64_t usedSize;
    };
    struct FileHeader
    {
        uint32_t majorVersion;
        uint32_t minorVerion;
        uint64_t reserved;
        char primaryFileGuid[16];
        char fileGuid[16];
        uint32_t filePart;
        uint64_t directoryPosition;
        uint64_t metadataPosition;
        uint32_t updatePending;
        uint64_t attachmentDirectoryPosition;
    };
    struct MetadataHeader
    {
        uint32_t xmlSize;
        uint32_t attachmentSize;
        uint8_t reserved[248];
    };
    struct DirectoryHeader
    {
        uint32_t entryCount;
        uint8_t reserved[124];
    };
    struct DirectoryEntryDV
    {
        char schemaType[2];
        int32_t pixelType;
        int64_t filePosition;
        int32_t filePart;
        int32_t compression;
        uint8_t pyramidType;
        uint8_t reserved[5];
        int32_t dimensionCount;
    };
    struct DimensionEntryDV
    {
        char dimension[4];
        int32_t start;
        int32_t size;
        float startCoordinate;
        int32_t storedSize;
    };
    struct SubBlockHeader
    {
        int32_t metadataSize;
        int32_t attachmentSize;
        int64_t dataSize;
        DirectoryEntryDV direEntry;
    };
    #pragma pack(pop)
    struct CZIChannelInfo
    {
        std::string id;
        std::string name;
    };
    typedef std::vector<CZIChannelInfo> CZIChannelInfos;
    struct Dimension
    {
        char type;
        int start;
        int size;
    };
    enum CZIDataType
    {
        Gray8 = 0,
        Gray16 = 1,
        Gray32Float = 2,
        Bgr24 = 3,
        Bgr48 = 4,
        Bgr96Float = 8,
        Bgra32 = 9,
        Gray64ComplexFloat = 10,
        Bgr192ComplexFloat = 11,
        Gray32 = 12,
        Gray64 = 13,
    };
}
#endif