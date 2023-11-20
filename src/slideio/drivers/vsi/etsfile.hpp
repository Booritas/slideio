// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once


#include <string>
#include <vector>

#include "vsistruct.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS EtsFile
        {
        public:
            struct TileInfo
            {
                std::vector<int> coordinates;
                int64_t offset = 0;
                uint32_t size = 0;
            };
        public:
            EtsFile(const std::string& filePath);
            std::string getFilePath() const {
                return m_filePath;
            }
            DataType getDataType() const {
                return m_dataType;
            }
            int getNumChannels() const {
                return m_numChannels;
            }
            slideio::Compression getCompression() const {
                return m_compression;
            }
            void read();
        private:
            std::string m_filePath;
            DataType m_dataType = DataType::DT_Unknown;
            int m_numChannels = 0;
            ColorSpace m_colorSpace = ColorSpace::Unknown;
            slideio::Compression m_compression = slideio::Compression::Unknown;
            int m_compressionQuality = 0;
            int m_tileWidth = 0;
            int m_tileHeight = 0;
            int m_numZSlices = 0;
            uint32_t m_pixelInfoHints[17] = { 0 };
            uint32_t m_backgroundColor[10] = { 0 };
            bool m_usePyramid = true;
            std::vector<int> m_dimensions;
            std::vector<TileInfo> m_tiles;
        };
    }
}