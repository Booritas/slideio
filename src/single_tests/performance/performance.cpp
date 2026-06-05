#include <chrono>
#include <iostream>

#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/slideio/slideio.hpp"
using namespace slideio;

void readSlide(const std::string& path, const std::string& driver)
{
	ImageDriverManager::setLogLevel("INFO");
	auto openStart = std::chrono::steady_clock::now();
	auto slide = openSlide(path, driver);
	auto openEnd = std::chrono::steady_clock::now();
	auto openMs = std::chrono::duration_cast<std::chrono::milliseconds>(openEnd - openStart).count();
	std::cout << "openSlide: " << openMs << " ms" << std::endl;

	auto scene = slide->getScene(0);
    auto rect = scene->getRect();
	const int sceneWidth = std::get<2>(rect);
	const int sceneHeight = std::get<3>(rect);
	const int blockWidth = 1000;
	double scaleX = static_cast<double>(blockWidth) / sceneWidth;
	const int blockHeight = static_cast<int>(sceneHeight * scaleX);
	std::tuple<int, int> blockSize(blockWidth, blockHeight);
	auto bufferSize = scene->getBlockSize(blockSize, 0, scene->getNumChannels(), scene->getNumZSlices() , scene->getNumTFrames());
	std::vector<uint8_t> buffer(bufferSize);

	auto readStart = std::chrono::steady_clock::now();
	scene->readResampledBlock(rect, blockSize, buffer.data(), bufferSize);
	auto readEnd = std::chrono::steady_clock::now();
	auto readMs = std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - readStart).count();
	std::cout << "readResampledBlock: " << readMs << " ms" << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <path> <driver>" << std::endl;
		return 1;
	}
	std::string path = argv[1];
	std::string driver = argv[2];
	readSlide(path, driver);
    return 0;
}
