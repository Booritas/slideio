#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/slideio/slideio.hpp"
#include <iostream>
#include <filesystem>

using namespace slideio;
using namespace slideio::converter;

static void process(const std::string& inputPath, const std::string& outputPath, double compressionRate) {
	if (!std::filesystem::exists(inputPath)) {
		throw std::runtime_error("Input file does not exist: " + inputPath);
	}
	if (std::filesystem::exists(outputPath)) {
		throw std::runtime_error("Output file already exists: " + outputPath);
	}
	auto slide = openSlide(inputPath, "AUTO");
	auto scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	OMETIFFJp2KConverterParameters params;
	params.setNumZoomLevels(3);
	params.setTileWidth(512);
	params.setTileHeight(512);
	params.setCompressionRate(static_cast<float>(compressionRate));
	TiffConverter converter;
	converter.createFileLayout(scene->getCVScene(), params);
	converter.createTiff(outputPath, [](int progress) {printf("%d", progress); });
}

int main(int argc, char* argv[]) {
	if (argc < 3 || argc > 4) {
		std::cerr << "Usage: " << (argc > 0 ? argv[0] : "converter") << " <inputPath> <outputPath> [compressionRate]" << std::endl;
		return 1;
	}

	std::string inputPath = argv[1];
	std::string outputPath = argv[2];
	double compressionRate = 5;

	if (argc == 4) {
		try {
			compressionRate = std::stod(argv[3]);
		} catch (const std::exception& ) {
			std::cerr << "Error: Invalid compression rate: " << argv[3] << std::endl;
			return 1;
		}
	}

	if (!std::filesystem::exists(inputPath)) {
		std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
		return 1;
	}

	if (std::filesystem::exists(outputPath)) {
		std::cerr << "Error: Output file already exists: " << outputPath << std::endl;
		return 1;
	}
	try {
  		process(inputPath, outputPath, compressionRate);

	} catch (const std::exception& e) {
		std::cerr << "Error during processing: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
