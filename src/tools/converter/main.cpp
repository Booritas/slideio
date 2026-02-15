// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"
#include "sceneconverter.hpp"

#include <csignal>
#include <CLI/CLI.hpp>

#include "slideio/slideio/slideio.hpp"

using namespace slideio;


static void signalHandler(int signal) {
    std::cout << "\033[?25h" << std::flush; // Restore cursor
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
    bool silent = false;
    bool infoOnly = false;
    bool deleteIfExists = false;
    int sceneIndex = 0;
    int logLevelValue = 1;

    app.add_option("input", inputPath, "Input file path")
       ->required()
       ->check(CLI::ExistingFile);

    app.add_option("output", outputPath, "Output file path")
       ->required();

    app.add_option("-n,--scene-index", sceneIndex, "Index of the scene to convert")
       ->default_val(0)
       ->check(CLI::NonNegativeNumber);

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

    app.add_flag("-s,--silent", silent, "Silent execution (no information, no progress bar)")
       ->default_val(false);

    app.add_flag("-i,--info-only", infoOnly, "Print conversion information without performing the conversion")
       ->default_val(false);

    app.add_flag("-x,--delete-if-exists", deleteIfExists, "Delete output file if it already exists")
       ->default_val(false);

    app.add_option("-l,--log-level", logLevelValue, "Log level: 0=FATAL, 1=ERROR, 2=WARNING, 3=INFO")
       ->default_val(1)
       ->check(CLI::Range(0, 3));

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

        std::string logLevel;
        switch (logLevelValue) {
        case 0:
            logLevel = "FATAL";
            break;
        case 1:
            logLevel = "ERROR";
            break;
        case 2:
            logLevel = "WARNING";
            break;
        case 3:
            logLevel = "INFO";
            break;
        default:
            logLevel = "INFO";
        }
        setLogLevel(logLevel);

        convertFile(inputPath,
                    outputPath,
                    sceneIndex,
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
                    silent,
                    infoOnly,
                    deleteIfExists);
    }
    catch (const std::exception& e) {
        std::cerr << "Error during processing: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
