// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <string>
#include <vector>

#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/drivers/vsi/etsfilescene.hpp"
#include "slideio/drivers/vsi/volume.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/drivers/vsi/pyramid.hpp"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS EtsFile
        {
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

            void read(std::list<std::shared_ptr<Volume>>& volumes);

            void assignVolume(const std::shared_ptr<Volume>& volume) {
                m_volume = volume;
            }

            std::shared_ptr<Volume> getVolume() const {
                return m_volume;
            }

            const cv::Size& getSize() const {
                return m_size;
            }

            const cv::Size& getTileSize() const {
                return m_tileSize;
            }

            int getNumZSlices() const {
                return m_numZSlices;
            }

            int getNumTFrames() const {
                return m_numTFrames;
            }

            int getNumLambdas() const {
                return m_numLambdas;
            }

            int getNumPyramidLevels() const {
                return m_pyramid.getNumLevels();
            }

            const PyramidLevel& getPyramidLevel(int index) const {
                return m_pyramid.getLevel(index);
            }
            void readTile(int levelIndex, int zSlice, int tFrame, int tileIndex, cv::OutputArray tileRaster);
        private:
            std::string m_filePath;
            DataType m_dataType = DataType::DT_Unknown;
            int m_numChannels = 1;
            ColorSpace m_colorSpace = ColorSpace::Unknown;
            slideio::Compression m_compression = slideio::Compression::Unknown;
            int m_compressionQuality = 0;
            cv::Size m_size;
            cv::Size m_sizeWithCompleteTiles;
            cv::Size m_tileSize;
            int m_numZSlices = 1;
            int m_numTFrames = 1;
            int m_numLambdas = 1;
            uint32_t m_pixelInfoHints[17] = {0};
            uint32_t m_backgroundColor[10] = {0};
            bool m_usePyramid = false;
            int m_numDimensions;
            std::shared_ptr<Volume> m_volume;
            Pyramid m_pyramid;
            std::unique_ptr<VSIStream> m_etsStream;
            std::vector<uint8_t> m_buffer;
        };
    }
}
