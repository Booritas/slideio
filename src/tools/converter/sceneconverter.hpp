#include <string>

namespace slideio {
	class Rect;
	class Range;
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
	const slideio::Rect& rect, 
	const slideio::Range& channelRange, 
	const slideio::Range& sliceRange, 
	const slideio::Range& frameRange, 
	bool silent,
	bool infoOnly,
	bool deleteIfExists);
