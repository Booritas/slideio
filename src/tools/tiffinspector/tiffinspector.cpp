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
		// argv[0] rather than a literal: the distributions install this as
		// slideio-tiffinspector, so a hard-coded name would tell the user to
		// run something that is not on their machine.
		const std::string name =
			(argc > 0 && argv[0]) ? std::filesystem::path(argv[0]).filename().string()
			                      : std::string("tiffinspector");
		std::cerr << "Usage: " << name << " inputPath" << std::endl;
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
