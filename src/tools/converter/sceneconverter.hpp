#pragma once
#include <map>
#include <string>
#include <vector>

namespace slideio {
	class Rect;
	class Range;
}

/// Parses -o arguments, each "<driver-id>.<name>=<value>", into open options. The key
/// ends at the first '='. An argument without '=', with an empty key, or repeating a key
/// throws; the keys themselves are validated by openSlide.
std::map<std::string, std::string> parseOpenOptions(const std::vector<std::string>& arguments);

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
	const slideio::Rect& rect, 
	const slideio::Range& channelRange, 
	const slideio::Range& sliceRange, 
	const slideio::Range& frameRange, 
	bool silent,
	bool infoOnly,
	bool deleteIfExists,
	int tileBatchSize,
	int numReadingThreads,
	int numEncodingThreads,
	const std::map<std::string, std::string>& openOptions);
