#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/slideio.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <csignal>

using namespace slideio;
using namespace slideio::converter;

// RAII class to manage cursor visibility
class CursorGuard {
public:
	CursorGuard() {
		std::cout << "\033[?25l" << std::flush;  // Hide cursor
	}
	~CursorGuard() {
		std::cout << "\033[?25h" << std::flush;  // Show cursor
	}
	CursorGuard(const CursorGuard&) = delete;
	CursorGuard& operator=(const CursorGuard&) = delete;
};

static void signalHandler(int signal) {
	std::cout << "\033[?25h" << std::flush;  // Restore cursor
	std::exit(signal);
}

static bool parseRect(const std::string& rectStr, Rect& rect) {
	if (rectStr.empty()) {
		return false;
	}
	std::istringstream iss(rectStr);
	char c1, c2, c3;
	if (!(iss >> rect.x >> c1 >> rect.y >> c2 >> rect.width >> c3 >> rect.height) || 
		c1 != ',' || c2 != ',' || c3 != ',') {
		throw std::runtime_error("Invalid rectangle format. Use: x,y,width,height");
	}
	if (rect.width <= 0 || rect.height <= 0) {
		throw std::runtime_error("Rectangle width and height must be positive");
	}
	if (rect.x < 0 || rect.y < 0) {
		throw std::runtime_error("Rectangle x and y must be non-negative");
	}
	return true;
}

static bool parseRange(const std::string& rangeStr, Range& range) {
	if (rangeStr.empty()) {
		return false;
	}
	std::istringstream iss(rangeStr);
	char c;
	if (!(iss >> range.start >> c >> range.end) || c != ',') {
		throw std::runtime_error("Invalid range format. Use: start,end");
	}
	if (range.start < 0 || range.end < 0) {
		throw std::runtime_error("Range start and end must be non-negative");
	}
	if (range.start >= range.end) {
		throw std::runtime_error("Range start must be less than end");
	}
	return true;
}

static void showProgress(int progress) {
	const int barWidth = 50;
	std::cout << "\r[";
	int pos = barWidth * progress / 100;
	for (int i = 0; i < barWidth; ++i) {
		if (i < pos) std::cout << "=";
		else if (i == pos) std::cout << ">";
		else std::cout << " ";
	}
	std::cout << "] " << std::setw(3) << progress << "%" << std::flush;
	if (progress >= 100) {
		std::cout << std::endl;
	}
}

static std::string formatDuration(std::chrono::milliseconds duration) {
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	duration -= hours;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
	duration -= minutes;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
	duration -= seconds;
	auto millis = duration;

	std::ostringstream oss;
	if (hours.count() > 0) {
		oss << hours.count() << "h ";
	}
	if (minutes.count() > 0 || hours.count() > 0) {
		oss << minutes.count() << "m ";
	}
	oss << seconds.count() << "." << std::setfill('0') << std::setw(3) << millis.count() << "s";
	return oss.str();
}

static void process(const std::string& inputPath, const std::string& outputPath, double compressionRate, int tileSize, int numZoomLevels, const std::string& inputDriver, const std::string& targetFormat, const std::string& targetCompression, int compressionQuality, const Rect& rect, const Range& channelRange, const Range& sliceRange, const Range& frameRange, bool showProgressBar) {
	if (!std::filesystem::exists(inputPath)) {
		throw std::runtime_error("Input file does not exist: " + inputPath);
	}
	if (std::filesystem::exists(outputPath)) {
		throw std::runtime_error("Output file already exists: " + outputPath);
	}
	auto slide = openSlide(inputPath, inputDriver);
	auto scene = slide->getScene(0);

	Compression compression = (targetCompression == "Jpeg") ? Compression::Jpeg : Compression::Jpeg2000;
	ImageFormat format = (targetFormat == "SVS") ? SVS : OME_TIFF;
	ConverterParameters params(format, TIFF_CONTAINER, compression);
	if (!rect.empty()) {
		const Rect& sceneRect = scene->getCVScene()->getRect();
		if (rect.x + rect.width > sceneRect.width 
			|| rect.y + rect.height > sceneRect.height) {
			throw std::runtime_error("Specified rectangle exceeds scene dimensions");
		}
		params.setRect(rect);
	}
	if (!channelRange.empty()) {
		const int numChannels = scene->getNumChannels();
		if (channelRange.end > numChannels) {
			throw std::runtime_error("Channel range exceeds number of channels in the scene");
		}
		params.setChannelRange(channelRange);
	}
	if (!sliceRange.empty()) {
		const int numSlices = scene->getNumZSlices();
		if (sliceRange.end > numSlices) {
			throw std::runtime_error("Slice range exceeds number of slices in the scene");
		}
		params.setSliceRange(sliceRange);
	}
	if (!frameRange.empty()) {
		const int numFrames = scene->getNumTFrames();
		if (frameRange.end > numFrames) {
			throw std::runtime_error("Frame range exceeds number of frames in the scene");
		}
		params.setTFrameRange(frameRange);
	}
	if (compression == Compression::Jpeg2000) {
		auto jp2kParams 
	        = std::static_pointer_cast<JP2KEncodeParameters>(params.getEncodeParameters());
		jp2kParams->setCompressionRate(static_cast<float>(compressionRate));
	} else if (compression==Compression::Jpeg) {
		auto jpegParams 
			= std::static_pointer_cast<JpegEncodeParameters>(params.getEncodeParameters());
		jpegParams->setQuality(compressionQuality);
	}
	auto containerParams 
        = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
	containerParams->setTileWidth(tileSize);
	containerParams->setTileHeight(tileSize);
	if (numZoomLevels > 0) {
		containerParams->setNumZoomLevels(numZoomLevels);
	}
	TiffConverter converter;
	converter.createFileLayout(scene->getCVScene(), params);
	
	auto startTime = std::chrono::high_resolution_clock::now();
	
	if (showProgressBar) {
		std::cout << "Converting..." << std::endl;
		CursorGuard cursorGuard;  // RAII: cursor will be restored automatically
		converter.createTiff(outputPath, showProgress);
	} else {
		converter.createTiff(outputPath, [](int) {});
	}
	
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	
	if (showProgressBar) {
		std::cout << "Conversion completed successfully." << std::endl;
	}
	std::cout << "Conversion time: " << formatDuration(duration) << std::endl;
}

int main(int argc, char* argv[]) {
	// Register signal handlers to restore cursor on Ctrl+C
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
#ifdef SIGBREAK
	std::signal(SIGBREAK, signalHandler);
#endif

	CLI::App app{"Slide format converter"};

	std::string inputPath;
	std::string outputPath;
	double compressionRate = 5.0;
	int tileSize = 512;
	int numZoomLevels = -1;
	std::string inputDriver = "AUTO";
	std::string targetFormat = "OMETIFF";
	std::string targetCompression = "Jpeg2000";
	int compressionQuality = 95;
	std::string rectStr;
	std::string channelRangeStr;
	std::string sliceRangeStr;
	std::string frameRangeStr;
	bool showProgressBar = false;

	app.add_option("input", inputPath, "Input file path")
		->required()
		->check(CLI::ExistingFile);

	app.add_option("output", outputPath, "Output file path")
		->required()
		->check(CLI::NonexistentPath);

	app.add_option("-c,--compression-rate", compressionRate, "Compression rate for Jpeg2000 compression")
		->default_val(5.0);

	app.add_option("-t,--tile-size", tileSize, "Tile size (width and height)")
		->default_val(512)
		->check(CLI::PositiveNumber);

	app.add_option("-z,--zoom-levels", numZoomLevels, "Number of zoom levels (-1 for auto)")
		->default_val(-1)
		->check(CLI::Range(-1, 100));

	app.add_option("-d,--driver", inputDriver, "Input driver name (AUTO for auto-detection)")
		->default_val("AUTO");

	app.add_option("-f,--format", targetFormat, "Target format (SVS or OMETIFF)")
		->default_val("OMETIFF")
		->check(CLI::IsMember({"SVS", "OMETIFF"}));

	app.add_option("-m,--compression-method", targetCompression, "Compression method (Jpeg or Jpeg2000)")
		->default_val("Jpeg2000")
		->check(CLI::IsMember({"Jpeg", "Jpeg2000"}));

	app.add_option("-q,--quality", compressionQuality, "Compression quality for Jpeg compression (0-100)")
		->default_val(95)
		->check(CLI::Range(0, 100));

	app.add_option("-r,--rect", rectStr, "Region of interest: x,y,width,height (empty for full image)")
		->default_str("");

	app.add_option("--channel-range", channelRangeStr, "Channel range: start,end (empty for all channels)")
		->default_str("");

	app.add_option("--slice-range", sliceRangeStr, "Z-slice range: start,end (empty for all slices)")
		->default_str("");

	app.add_option("--frame-range", frameRangeStr, "Time frame range: start,end (empty for all frames)")
		->default_str("");

	app.add_flag("-p,--progress", showProgressBar, "Show progress bar during conversion")
		->default_val(false);

	CLI11_PARSE(app, argc, argv);

	try {
		Rect rect;
		Range channelRange;
		Range sliceRange;
		Range frameRange;
		
		parseRect(rectStr, rect);
		parseRange(channelRangeStr, channelRange);
		parseRange(sliceRangeStr, sliceRange);
		parseRange(frameRangeStr, frameRange);
		
  		process(inputPath,
			outputPath,
			compressionRate,
			tileSize,
			numZoomLevels,
			inputDriver,
			targetFormat,
			targetCompression,
			compressionQuality,
			rect,
			channelRange,
			sliceRange,
			frameRange,
			showProgressBar);
	} catch (const std::exception& e) {
		std::cerr << "Error during processing: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
