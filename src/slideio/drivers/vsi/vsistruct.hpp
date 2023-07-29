// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio
{
    namespace vsi
    {
        #pragma pack(push,1)
        struct ImageFileHeader
        {
            uint8_t magic[2];
            uint16_t i42;
            uint32_t offsetFirstIFD;
        };
        struct ImageFileDirectory
        {
            uint16_t entryCount;
        };
        struct FieldEntry
        {
            uint16_t tag;
            uint16_t type;
            uint32_t numberOfValues;
            uint32_t offset;
        };
        struct VolumeHeader
        {
            uint16_t headerSize;
            uint16_t magicNumber;
            uint32_t volumeVersion;
            int64_t offsetFirstDataField;
            uint32_t flags;
            uint32_t unused;
        };
        struct DataField
        {
            uint32_t type;
            uint32_t tag;
            uint32_t offsetNextField;
            uint32_t dataSize;
        };
        struct TagHeader
        {
            int32_t fieldType;
            int32_t tag;
            int32_t nextField;
            uint32_t dataSize;
        };

        struct EtsVolumeHeader {
            char magic[4]; // Magic ID 'S', 'I', 'S', '\0'
            uint32_t headerSize; // Size of the header : 64
            uint32_t versionNumber; // Version number of the header
            uint32_t numDimensions; // Number of dimensions of the multidimensional indices
            uint64_t additionalHeaderPos; // 64bit index to the file position of the additional header
            uint32_t additionalHeaderSize; // Size of the additional header
            uint32_t unused1;
            uint64_t usedChunksPos; // 64 bit index to the file position of used chunks
            uint32_t numUsedChunks; // Number of used chunks
            uint32_t unused2;
            uint64_t freeChunksPos; // 64 bit index to the file position of free chunks
            uint32_t numFreeChunks; // Number of free chunks
            uint32_t unused3;
        };
        struct ETSAdditionalHeader
        {
            char magic[4]; // Header identification ( 0x00535445 )
            uint32_t version; // Header version info
            uint32_t componentType; // Component type
            uint32_t componentCount; // Component count
            uint32_t colorSpace; // Component color space
            uint32_t format; // Compression format
            uint32_t quality; // Compression quality
            uint32_t sizeX; // Tile x size
            uint32_t sizeY; // Tile y size
            uint32_t sizeZ; // Tile z size
            uint32_t pixInfoHints[17]; // Pixel info hints
            uint32_t background[10]; // Background color
            uint32_t componentOrder; // Component order
            uint32_t usePyramid; // Use pyramid
            uint32_t unused[18]; // For future use

        };
        struct ETSBlock
        {
            int64_t filePos;
            uint32_t size;
            uint32_t unused;
        };

#pragma pack(pop)
        constexpr uint32_t EXTENDED_FIELD_TYPE_MASK = 0x1000000;
        constexpr uint32_t VOLUME_DATA_BLOCK_TYPE_MASK = 0x10000000;
        constexpr uint32_t VOLUME_TAG_COUNT_MASK = 0xFFFFFFF;
        // Pixel types
        enum class PixelType
        {
            CHAR = 1,
            UCHAR = 2,
            SHORT = 3,
            USHORT = 4,
            INT = 5,
            UINT = 6,
            INT64 = 7,
            UINT64 = 8,
            FLOAT = 9,
            DOUBLE = 10,
            DOUBLE2 = 11,
            BOOL = 12,
            TCHAR = 13,
            DWORD = 14,
            TIMESTAMP = 17,
            DATE = 18,
            VECTOR_INT_2 = 256,
            VECTOR_INT_3 = 257,
            VECTOR_INT_4 = 258,
            RECT_INT = 259,
            VECTOR_DOUBLE_2 = 260,
            VECTOR_DOUBLE_3 = 261,
            VECTOR_DOUBLE_4 = 262,
            RECT_DOUBLE = 263,
            MATRIX_DOUBLE_2_2 = 264,
            MATRIX_DOUBLE_3_3 = 265,
            MATRIX_DOUBLE_4_4 = 266,
            TUPLE_INT = 267,
            TUPLE_DOUBLE = 268,
            RGB = 269,
            BGR = 270,
            FIELD_TYPE = 271,
            MEM_MODEL = 272,
            COLOR_SPACE = 273,
            UNICODE_TCHAR = 8192,
            ARRAY_INT_2 = 274,
            ARRAY_INT_3 = 275,
            ARRAY_INT_4 = 276,
            ARRAY_INT_5 = 277,
            ARRAY_DOUBLE_2 = 279,
            ARRAY_DOUBLE_3 = 280,
            DIM_INDEX_1 = 8195,
            DIM_INDEX_2 = 8199,
            VOLUME_INDEX = 8200,
            PIXEL_INFO_TYPE = 8470,
        };
        enum class ColorSpace
        {
            Unknown = 0,
            Gray = 1,
            Palette = 2,
            RGB = 3,
            BGR = 4,
            HSV = 5
        };
        enum class Compression
        {
            RAW = 0,
            JPEG = 2,
            JPEG_2000 = 3,
            JPEG_LOSSLESS = 5,
            PNG = 8,
            BMP = 9,
        };
        constexpr int DEFAULT_IMAGE = 0;
        constexpr int OVERVIEW_IMAGE = 1;
        constexpr int SAMPLE_MASK = 2;
        constexpr int FOCUS_IMAGE = 4;
        constexpr int EFI_SHARPNESS_MAP = 8;
        constexpr int EFI_HEIGHT_MAP = 16;
        constexpr int EFI_TEXTURE_MAP = 32;
        constexpr int EFI_STACK = 64;
        constexpr int MACRO_IMAGE = 256;
        constexpr int Z = 1;
        constexpr int T = 2;
        constexpr int LAMBDA = 3;
        constexpr int C = 4;
        constexpr int UNKNOWN = 5;
        constexpr int PHASE = 9;
    }
}
