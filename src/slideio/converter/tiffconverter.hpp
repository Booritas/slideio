// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffstructure.hpp"
#include "slideio/core/tools/boundedqueue.hpp"
#include <mutex>
#include <chrono>
#include <atomic>

#include "slideio/core/cvslide.hpp"

namespace slideio
{
    class Slide;
    class Scene;
    class CVScene;

    namespace converter
    {
        class ConverterParameters;

		class SLIDEIO_CONVERTER_EXPORTS TiffConverter
		{
		public:
			TiffConverter() = default;
			TiffConverter(const TiffConverter& other)
				: m_pages(other.m_pages)
				, m_file(other.m_file)
				, m_scene(other.m_scene)
				, m_parameters(other.m_parameters)
				, m_cropRect(other.m_cropRect)
				, m_filePath(other.m_filePath)
				, m_totalTiles(other.m_totalTiles)
				, m_currentTile(other.m_currentTile)
				, m_lastProgress(other.m_lastProgress)
				, m_readersIdleTimeNs(other.m_readersIdleTimeNs.load())
				, m_encodersIdleTimeNs(other.m_encodersIdleTimeNs.load())
				, m_writerIdleTimeNs(other.m_writerIdleTimeNs.load())
				, m_numReaderThreads(other.m_numReaderThreads)
				, m_numEncoderThreads(other.m_numEncoderThreads) {}
			TiffConverter& operator=(const TiffConverter& other) {
				if (this != &other) {
					m_pages = other.m_pages;
					m_file = other.m_file;
					m_scene = other.m_scene;
					m_parameters = other.m_parameters;
					m_cropRect = other.m_cropRect;
					m_filePath = other.m_filePath;
					m_totalTiles = other.m_totalTiles;
					m_currentTile = other.m_currentTile;
					m_lastProgress = other.m_lastProgress;
					m_readersIdleTimeNs.store(other.m_readersIdleTimeNs.load());
					m_encodersIdleTimeNs.store(other.m_encodersIdleTimeNs.load());
					m_writerIdleTimeNs.store(other.m_writerIdleTimeNs.load());
					m_numReaderThreads = other.m_numReaderThreads;
					m_numEncoderThreads = other.m_numEncoderThreads;
				}
				return *this;
			}
			TiffConverter(TiffConverter&&) = default;
			TiffConverter& operator=(TiffConverter&&) = default;

			struct Block {
				cv::Rect rect;              // Block rectangle in the source image
				size_t firstTileSequenceId; // Preserves original read order
			};
			struct Tile {
				size_t sequenceId;      // Preserves original read order
				cv::Mat raster;         // Tile pixel data 
				cv::Point2i location;   // Location of the tile in the target image
			};
			struct EncodedTile {
				size_t sequenceId;                  // Preserves original read order
				std::vector<uint8_t> encodedData;   // Encoded tile data
				cv::Point2i location;               // Location of the tile in the target image
			};
		public:
            void createFileLayout(const std::shared_ptr<CVScene>& scene, const ConverterParameters& parameters);
            void createTiff(const std::string& filePath, const std::function<void(int)>& cb, int tileBatchSize);
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
            std::chrono::nanoseconds getReadersIdleTime() const {
                return std::chrono::nanoseconds(m_readersIdleTimeNs.load());
            }
            std::chrono::nanoseconds getEncodersIdleTime() const {
                return std::chrono::nanoseconds(m_encodersIdleTimeNs.load());
            }
			std::chrono::nanoseconds getWriterIdleTime() const {
				return std::chrono::nanoseconds(m_writerIdleTimeNs.load());
			}
			int getNumReaderThreads() const {
				return m_numReaderThreads;
			}
			int getNumEncoderThreads() const {
				return m_numEncoderThreads;
			}
			int getNumWriterThreads() const {
				return 1;
			}
            void createTileQueue(const TiffDirectory& dir, const TiffDirectoryStructure& page, int tileBatchSize, std::queue<Block>& queue);
			virtual std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> cloneScene() const;
		protected:
            std::shared_ptr<CVScene> getScene() const {
                return m_scene;
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
                const std::function<void(int)>& cb, int param);
            void writeDirectoryDataST(TiffDirectory& dir, const TiffDirectoryStructure& page,
                const std::function<void(int)>& cb, int tileBatchSize);
            void writeDirectoryDataMT(TiffDirectory& dir, const TiffDirectoryStructure& page,
                                      const std::function<void(int)>& cb, int tileBatchSize);
            void computeCropRect();
            void makeSureValid() const;
            static std::string SVSDateString();
            static std::string SVSTimeString();
            void checkSVSRequirements() const;
            void checkJpegRequirements() const;
            void checkEncodingRequirements() const;
            void checkContainerRequirements() const;
            void updateNotDefinedParameters();
			// --- Multithreaded conversion helpers ---
			void readTiles(const TiffDirectory& dir, const TiffDirectoryStructure& page, BoundedQueue<Tile>& inputQueue,
				std::queue<Block>& blockQueue, std::mutex& blockQueueMutex, std::atomic<size_t>& activeReaders,
				std::exception_ptr& readerException, std::mutex& exceptionMutex);
			void encodeTiles(BoundedQueue<Tile>& inputQueue, BoundedQueue<EncodedTile>& outputQueue,
				std::atomic<size_t>& activeEncoders, std::exception_ptr& encoderException, std::mutex& exceptionMutex);
			void writeTile(const EncodedTile& tile);
			void writeTiles(BoundedQueue<Tile>& inputQueue, BoundedQueue<EncodedTile>& outputQueue, const std::function<void(int)>& cb,
				std::exception_ptr& writerException, std::mutex& exceptionMutex);
			std::vector<uint8_t> encodeTile(const cv::Mat& tile);
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
			std::atomic<int64_t> m_readersIdleTimeNs{0};
			std::atomic<int64_t> m_encodersIdleTimeNs{0};
			std::atomic<int64_t> m_writerIdleTimeNs{0};
			int m_numReaderThreads = 0;
			int m_numEncoderThreads = 0;
		};
    }
}
