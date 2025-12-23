#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/slideio/slideio.hpp"
#include <iostream>
#include <filesystem>

using namespace slideio;
using namespace slideio::converter;

static void process(const std::string& inputPath) {
	if (!std::filesystem::exists(inputPath)) {
		throw std::runtime_error("Input file does not exist: " + inputPath);
	}
	std::vector<TiffDirectory> dirs;
	TiffTools::scanFile(inputPath, dirs);
	for (const auto& dir: dirs) {
		std::cout << dir;
	}
}

int main(int argc, char* argv[]) {
	if (argc !=2 ) {
		std::cerr << "Usage: tiffinspector inputPath" << std::endl;
		return 1;
	}

	std::string inputPath = argv[1];

	if (!std::filesystem::exists(inputPath)) {
		std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
		return 1;
	}

	try {
  		process(inputPath);

	} catch (const std::exception& e) {
		std::cerr << "Error during processing: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
