// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffstructure.hpp"

namespace slideio
{
    class CVScene;

    namespace converter
    {
        class ConverterParameters;

        class SLIDEIO_CONVERTER_EXPORTS TiffConverter
        {
        public:
            void createFileLayout(const std::shared_ptr<CVScene>& scene, const ConverterParameters& parameters);
            void createTiff(const std::string& filePath, const std::function<void(int)>& cb);
            int getNumTiffPages() const {
                return static_cast<int>(m_pages.size());
            }

            const TiffPageStructure& getTiffPage(int index) const;
            Rect getSceneRect() const {
                return m_cropRect;
            }

            int getTotalTiles() const {
                return m_totalTiles;
			}

            const ConverterParameters& getParameters() const {
                return m_parameters;
            }
        private:
            TiffPageStructure& appendPage() {
                return m_pages.emplace_back();
            }
            DataType getChannelRangeDataType(const Range& channelRange) const;
            int computeChannelChunk(int firstChannel, const std::shared_ptr<CVScene>& scene) const;
            std::string createSVSImageDescription() const;
            std::string createImageDescriptionTag() const;
            std::string createOMETiffDescription() const;
            TiffDirectory setUpDirectory(const TiffDirectoryStructure& page);
            void writeDirectoryData(TiffDirectory& dir, const TiffDirectoryStructure& page,
                const std::function<void(int)>& cb);
            void computeCropRect();
            void makeSureValid() const;
            static std::string SVSDateString();
            static std::string SVSTimeString();
            void checkSVSRequirements() const;
            void checkJpegRequirements() const;
            void checkEncodingRequirements() const;
            void checkContainerRequirements() const;
            void updateNotDefinedParameters();
        private:
            std::vector<TiffPageStructure> m_pages;
            TIFFKeeperPtr m_file;
            std::shared_ptr<CVScene> m_scene;
            ConverterParameters m_parameters;
            Rect m_cropRect;
            std::string m_filePath;
            int m_totalTiles = 0;
            int m_currentTile = 0;
			int m_lastProgress = 0;
        };
    }
}
