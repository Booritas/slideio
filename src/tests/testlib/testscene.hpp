#pragma once
#include "slideio/core/cvscene.hpp"
#include "slideio/core/levelinfo.hpp"
#include <vector>

class TestScene : public slideio::CVScene
{
public:
    using slideio::CVScene::setChannelAttribute;
    // Records the arguments of every readResampledBlockChannelsEx call, so a test can
    // assert on the request the base-class level path made and not only on its output.
    struct Request
    {
        cv::Rect rect;
        cv::Size size;
        int zSlice;
        int tFrame;
    };

    void addLevel(const slideio::LevelInfo& level) { m_levels.push_back(level); }
    void clearLevels() { m_levels.clear(); }
    const std::vector<Request>& requests() const { return m_requests; }
    void clearRequests() { m_requests.clear(); }
    // Off by default: the tests that predate the level api rely on an untouched output.
    void setRenderCoordinates(bool render) { m_render = render; }
    TestScene() :
        m_filePath("/path/folder/file.svs"),
        m_rect(0, 0, 100, 100),
        m_numChannels(3),
        m_dataType(slideio::DataType::DT_Unknown),
        m_name("TestScene"),
        m_resolution(1., 1.),
        m_magnification(20.0),
        m_compression(slideio::Compression::Jpeg),
        m_numSlices(1),
        m_numTFrames(1)
    {}
    std::string getFilePath() const override { return m_filePath; }
    void setFilePath(const std::string& filePath) { m_filePath = filePath; }
	int getSceneIndex() const override { return 0; }
    std::string getName() const override { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    cv::Rect getRect() const override { return m_rect; }
    void setRect(const cv::Rect& rect) { m_rect = rect; }
    int getNumChannels() const override { return m_numChannels; }
    void setNumChannels(int numChannels) { m_numChannels = numChannels; }
    slideio::DataType getChannelDataType(int channel) const override { return m_dataType; }
    void setChannelDataType(slideio::DataType dataType) { m_dataType = dataType; }
    slideio::Resolution getResolution() const override { return m_resolution; }
    void setResolution(const slideio::Resolution& resolution) { m_resolution = resolution; }
    double getMagnification() const override { return m_magnification; }
    void setMagnification(double magnification) { m_magnification = magnification; }
    slideio::Compression getCompression() const override { return m_compression; }
    void setCompression(slideio::Compression compression) { m_compression = compression; }
    int getNumZSlices() const override { return m_numSlices; }
    int getNumTFrames() const override { return m_numTFrames; }
	void setNumZSlices(int numSlices) { m_numSlices = numSlices; }
	void setNumTFrames(int numFrames) { m_numTFrames = numFrames; }
	void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
		const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override {
		m_requests.push_back({blockRect, blockSize, zSliceIndex, tFrameIndex});
		if (!output.needed()) return;
		const int channels = componentIndices.empty() ? m_numChannels : (int)componentIndices.size();
		output.create(blockSize, CV_8UC(channels));
		if (!m_render) return;
		// Each pixel encodes the scene coordinate it came from, so a test can tell which
		// part of the scene a block was actually served from.
		cv::Mat raster = output.getMat();
		for (int y = 0; y < blockSize.height; ++y) {
			uint8_t* row = raster.ptr<uint8_t>(y);
			for (int x = 0; x < blockSize.width; ++x) {
				const int sceneX = blockRect.x + (blockRect.width * x) / blockSize.width;
				const int sceneY = blockRect.y + (blockRect.height * y) / blockSize.height;
				for (int c = 0; c < channels; ++c) {
					row[x * channels + c] = static_cast<uint8_t>((sceneX + sceneY * 7 + c * 31) & 0xFF);
				}
			}
		}
	}
    const std::string& getDriverId() const override {
        return m_driverId;
    }
private:
    std::string m_filePath;
    cv::Rect m_rect;
    int m_numChannels;
    slideio::DataType m_dataType;
    std::string m_name;
    slideio::Resolution m_resolution;
    double m_magnification;
    slideio::Compression m_compression;
    int m_numSlices;
    int m_numTFrames;
	std::string m_driverId = "TestDriver";
    std::vector<Request> m_requests;
    bool m_render = false;
};