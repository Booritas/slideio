// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/slideio.hpp"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

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

// Initialize console for UTF-8 output on Windows
static void initializeConsole() {
#ifdef _WIN32
	// Set console output code page to UTF-8
	SetConsoleOutputCP(CP_UTF8);
	// Enable ANSI escape sequences on Windows 10+
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
#endif
}

static void showProgress(int progress) {
	static auto startTime = std::chrono::steady_clock::now();
	static bool initialized = false;
	
	if (!initialized) {
		initializeConsole();
		startTime = std::chrono::steady_clock::now();
		initialized = true;
	}
	
	const int barWidth = 50;
	std::cout << "\r[";
	
	int pos = barWidth * progress / 100;
	
	// Use original progress bar style
	for (int i = 0; i < barWidth; ++i) {
		if (i < pos) {
			std::cout << "=";
		}
		else if (i == pos) {
			std::cout << ">";
		}
		else {
			std::cout << " ";
		}
	}
	
	std::cout << "] " << std::setw(3) << progress << "%";
	
	// Calculate elapsed and remaining time
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
	
	if (progress > 0 && progress < 100) {
		// Calculate ETA based on average time per percent
		auto totalEstimated = elapsed.count() * 100 / progress;
		auto remaining = totalEstimated - elapsed.count();
		
		// Format elapsed time
		int elapsedHours = static_cast<int>(elapsed.count() / 3600);
		int elapsedMinutes = static_cast<int>((elapsed.count() % 3600) / 60);
		int elapsedSeconds = static_cast<int>(elapsed.count() % 60);
		
		// Format remaining time - round up to next minute
		int remainingMinutesTotal = static_cast<int>((remaining + 59) / 60); // Round up
		int remainingHours = remainingMinutesTotal / 60;
		int remainingMinutes = remainingMinutesTotal % 60;
		
		std::cout << " | ";
		
		// Print elapsed time
		if (elapsedHours > 0) {
			std::cout << elapsedHours << "h " << elapsedMinutes << "m " << elapsedSeconds << "s";
		} else if (elapsedMinutes > 0) {
			std::cout << elapsedMinutes << "m " << elapsedSeconds << "s";
		} else {
			std::cout << elapsedSeconds << "s";
		}
		
		std::cout << " / ETA: ";
		
		// Print remaining time (rounded to minutes)
		if (remainingHours > 0) {
			std::cout << remainingHours << "h " << remainingMinutes << "m";
		} else if (remainingMinutes > 0) {
			std::cout << remainingMinutes << "m  ";
		} else {
			std::cout << "< 1m";
		}
	}
	
	std::cout << std::flush;
	
	if (progress >= 100) {
		std::cout << std::endl;
		initialized = false;  // Reset for next conversion
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

void printInfo(const TiffConverter& converter) {
	const ConverterParameters& params = converter.getParameters();
	std::shared_ptr<const TIFFContainerParameters> tiffParams =
		std::static_pointer_cast<const TIFFContainerParameters>(params.getContainerParameters());
	std::cout << "Target format: "
		<< ((converter.getParameters().getFormat() == SVS) ? "SVS" : "OMETIFF")
		<< std::endl;
	const Compression compression = params.getEncoding();
	std::cout << "Target compression: " << compression;
	if (compression == Compression::Jpeg) {
		const int quality =
			std::static_pointer_cast<const JpegEncodeParameters>(params.getEncodeParameters())->getQuality();
		std::cout << " (Compression quality: " << quality << ")";
	}
	else if (compression == Compression::Jpeg2000) {
		const float rate =
			std::static_pointer_cast<const JP2KEncodeParameters>(params.getEncodeParameters())->getCompressionRate();
		std::cout << " (Compression rate: " << rate << ")";
	}
	std::cout << std::endl;
	const int numZoomLevels = tiffParams->getNumZoomLevels();
	std::cout << "Number of zoom levels: " << numZoomLevels << std::endl;
	std::cout << "Tile size: " << tiffParams->getTileWidth() << " x " << tiffParams->getTileHeight() << std::endl;
	std::cout << "Number of TIFF pages: " << converter.getNumTiffPages() << std::endl;
	std::cout << "Channel range: " << params.getChannelRange() << std::endl;
	std::cout << "Slice range: " << params.getSliceRange() << std::endl;
	std::cout << "Time frame range: " << params.getTFrameRange() << std::endl;
	std::cout << "Target image rectangle: " << params.getRect() << std::endl;

}


void convertFile(
	const std::string& inputPath,
	const std::string& outputPath,
	int sceneIndex,
	double compressionRate, 
	int tileSize, 
	int numZoomLevels, 
	const std::string& inputDriver, 
	const std::string& targetFormat, 
	const std::string& targetCompression, 
	int compressionQuality, 
	const Rect& rect, 
	const Range& channelRange, 
	const Range& sliceRange, 
	const Range& frameRange, 
	bool silent,
	bool infoOnly,
	bool deleteIfExists) {
	if (!std::filesystem::exists(inputPath)) {
		throw std::runtime_error("Input file does not exist: " + inputPath);
	}
	if (!infoOnly && std::filesystem::exists(outputPath)) {
		if (deleteIfExists) {
			std::filesystem::remove(outputPath);
		} else {
			throw std::runtime_error("Output file already exists: " + outputPath);
		}
	}
	auto slide = openSlide(inputPath, inputDriver);
	auto scene = slide->getScene(sceneIndex);

	Compression compression = (targetCompression == "Jpeg") ? Compression::Jpeg : Compression::Jpeg2000;
	ImageFormat format = (targetFormat == "SVS") ? SVS : OME_TIFF;
	ConverterParameters params(format, TIFF_CONTAINER, compression);
	if (!rect.empty()) {
		const Rect& sceneRect = scene->getCVScene()->getRect();
		if (rect.x + rect.width > sceneRect.width 
			|| rect.y + rect.height > sceneRect.height) {
			RAISE_RUNTIME_ERROR << "Specified rectangle exceeds scene dimensions: (0,0,"
		    << sceneRect.width << "," << sceneRect.height << ").";
		}
		params.setRect(rect);
	}
	if (!channelRange.empty()) {
		const int numChannels = scene->getNumChannels();
		if (channelRange.end > numChannels) {
			RAISE_RUNTIME_ERROR << "Channel range exceeds number of channels in the scene: "
		    << numChannels;
		}
		params.setChannelRange(channelRange);
	}
	if (!sliceRange.empty()) {
		const int numSlices = scene->getNumZSlices();
		if (sliceRange.end > numSlices) {
			RAISE_RUNTIME_ERROR << "Slice range exceeds number of slices in the scene: "
				<< numSlices;
		}
		params.setSliceRange(sliceRange);
	}
	if (!frameRange.empty()) {
		const int numFrames = scene->getNumTFrames();
		if (frameRange.end > numFrames) {
			RAISE_RUNTIME_ERROR << "Frame range exceeds number of frames in the scene: "
		    << numFrames;
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

	if (infoOnly || !silent) {
		printInfo(converter);
	}

	if (infoOnly) {
		std::cout << "Info-only mode: Conversion skipped." << std::endl;
		return;
	}

	auto startTime = std::chrono::high_resolution_clock::now();
	
	if (!silent) {
		std::cout << "Converting..." << std::endl;
		CursorGuard cursorGuard;  // RAII: cursor will be restored automatically
		converter.createTiff(outputPath, showProgress);
	} else {
		converter.createTiff(outputPath,nullptr);
	}
	
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	
	if (!silent) {
		std::cout << "Conversion completed successfully." << std::endl;
		std::cout << "Conversion time: " << formatDuration(duration) << std::endl;
	}
}

