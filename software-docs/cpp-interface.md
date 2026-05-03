# SlideIO C++ Interface Reference

SlideIO is a C++ library for reading medical and microscopy slide images. It supports 11 formats (SVS, AFI, SCN, CZI, ZVI, NDPI, VSI, DCM, QPTIFF, OME-TIFF, GDAL) through a pluggable driver architecture and runs on Linux, macOS, and Windows.

This document describes the public C++ API surface.

---

## Table of Contents

1. [Global Functions](#1-global-functions)
2. [Slide](#2-slide)
3. [Scene](#3-scene)
4. [Core Abstract Base Classes](#4-core-abstract-base-classes)
   - [CVSlide](#41-cvslide)
   - [CVScene](#42-cvscene)
   - [ImageDriver](#43-imagedriver)
   - [LevelInfo](#44-levelinfo)
   - [ImageDriverManager](#45-imagedrivermanager)
   - [CVTools](#46-cvtools)
5. [Base Types](#5-base-types)
   - [Enumerations](#51-enumerations)
   - [Geometric Types](#52-geometric-types)
   - [Exceptions](#53-exceptions)
6. [Converter API](#6-converter-api)
7. [Transformer API](#7-transformer-api)
8. [Image Tools API](#8-image-tools-api)

---

## 1. Global Functions

**Header:** `slideio/slideio/slideio.hpp`

```cpp
namespace slideio {
    // Open a slide file. Returns a Slide object.
    // If driver is empty, the library auto-detects the format.
    std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver = "");

    // Return a list of all registered driver IDs
    // (e.g. "SVS", "AFI", "SCN", "CZI", "ZVI", "NDPI", "DCM", "GDAL", ...).
    std::vector<std::string> getDriverIDs();

    // Set the library-wide log level ("FATAL", "ERROR", "WARNING", "INFO", "DEBUG").
    void setLogLevel(const std::string& level);

    // Return the library version string.
    std::string getVersion();
}
```

### Minimal usage example

```cpp
#include "slideio/slideio/slideio.hpp"

auto slide = slideio::openSlide("/path/to/slide.svs");
int numScenes = slide->getNumScenes();
auto scene = slide->getScene(0);

// Read a 512x512 region at (1000, 2000), scaled to 256x256
std::vector<uint8_t> buffer(256 * 256 * scene->getNumChannels());
scene->readResampledBlock({1000, 2000, 512, 512}, {256, 256},
                          buffer.data(), buffer.size());
```

---

## 2. Slide

**Header:** `slideio/slideio/slide.hpp`

The `Slide` class represents an opened slide file. A slide contains one or more scenes (image regions) and optionally auxiliary images such as labels or macro overviews.

Instances are created via `slideio::openSlide()`.

```cpp
class Slide {
public:
    ~Slide();

    // --- Scene access ---

    // Number of scenes (image regions) in the slide.
    int getNumScenes() const;

    // Get a scene by its zero-based index.
    std::shared_ptr<Scene> getScene(int index) const;

    // Get a scene by name. Returns nullptr if not found.
    std::shared_ptr<Scene> getSceneByName(const std::string& name) const;

    // --- File info and metadata ---

    // Filesystem path that was used to open this slide.
    std::string getFilePath() const;

    // Raw metadata string (XML, JSON, or plain text depending on format).
    const std::string& getRawMetadata() const;

    // Format of the raw metadata.
    MetadataFormat getMetadataFormat() const;

    // --- Auxiliary images ---

    // Names of all auxiliary images (e.g. "label", "macro").
    const std::list<std::string>& getAuxImageNames() const;

    // Number of auxiliary images.
    int getNumAuxImages() const;

    // Retrieve an auxiliary image as a Scene.
    std::shared_ptr<Scene> getAuxImage(const std::string& sceneName) const;

    // --- Utilities ---

    // Human-readable summary of the slide.
    std::string toString() const;

    // Driver identification.
    void setDriverId(const std::string& driverId);
    const std::string& getDriverId() const;
};

typedef std::shared_ptr<slideio::Slide> SlidePtr;
```

---

## 3. Scene

**Header:** `slideio/slideio/scene.hpp`

The `Scene` class represents a single raster image within a slide. It provides methods for querying image metadata and reading pixel data at arbitrary regions and resolutions. Scenes can be multidimensional (Z-slices, time frames) and multichannel.

```cpp
class Scene {
public:
    Scene(std::shared_ptr<CVScene> scene);
    virtual ~Scene();

    // --- Basic properties ---

    std::string getFilePath() const;
    std::string getName() const;

    // Bounding rectangle as (x, y, width, height).
    std::tuple<int, int, int, int> getRect() const;

    // --- Channels ---

    int getNumChannels() const;
    slideio::DataType getChannelDataType(int channel) const;
    std::string getChannelName(int channel) const;

    // --- Dimensions ---

    int getNumZSlices() const;   // Z-depth (1 for 2D images)
    int getNumTFrames() const;   // Time frames (1 for single-frame images)

    // --- Resolution and magnification ---

    // Pixel resolution in meters per pixel (x, y).
    std::tuple<double, double> getResolution() const;

    // Distance between Z-slices in meters.
    double getZSliceResolution() const;

    // Time between frames in seconds.
    double getTFrameResolution() const;

    // Objective magnification (e.g. 20.0 for 20x).
    double getMagnification() const;

    // --- Compression ---

    Compression getCompression() const;

    // --- Zoom levels ---

    int getNumZoomLevels() const;
    const LevelInfo* getLevelInfo(int level) const;

    // --- 2D raster reading ---

    // Read a rectangular block at native resolution.
    void readBlock(
        const std::tuple<int,int,int,int>& blockRect,
        void* buffer, size_t bufferSize);

    // Read selected channels at native resolution.
    void readBlockChannels(
        const std::tuple<int,int,int,int>& blockRect,
        const std::vector<int>& channelIndices,
        void* buffer, size_t bufferSize);

    // Read a rectangular block, resampled to blockSize.
    void readResampledBlock(
        const std::tuple<int,int,int,int>& blockRect,
        const std::tuple<int,int>& blockSize,
        void* buffer, size_t bufferSize);

    // Read selected channels, resampled to blockSize.
    void readResampledBlockChannels(
        const std::tuple<int,int,int,int>& blockRect,
        const std::tuple<int,int>& blockSize,
        const std::vector<int>& channelIndices,
        void* buffer, size_t bufferSize);

    // --- 4D raster reading (Z-slices + time frames) ---

    void read4DBlock(
        const std::tuple<int,int,int,int>& blockRect,
        const std::tuple<int,int>& zSliceRange,
        const std::tuple<int,int>& timeFrameRange,
        void* buffer, size_t bufferSize);

    void read4DBlockChannels(
        const std::tuple<int,int,int,int>& blockRect,
        const std::vector<int>& channelIndices,
        const std::tuple<int,int>& zSliceRange,
        const std::tuple<int,int>& timeFrameRange,
        void* buffer, size_t bufferSize);

    void readResampled4DBlock(
        const std::tuple<int,int,int,int>& blockRect,
        const std::tuple<int,int>& blockSize,
        const std::tuple<int,int>& zSliceRange,
        const std::tuple<int,int>& timeFrameRange,
        void* buffer, size_t bufferSize);

    void readResampled4DBlockChannels(
        const std::tuple<int,int,int,int>& blockRect,
        const std::tuple<int,int>& blockSize,
        const std::vector<int>& channelIndices,
        const std::tuple<int,int>& zSliceRange,
        const std::tuple<int,int>& timeFrameRange,
        void* buffer, size_t bufferSize);

    // --- Auxiliary images ---

    const std::list<std::string>& getAuxImageNames() const;
    int getNumAuxImages() const;
    std::shared_ptr<Scene> getAuxImage(const std::string& imageName) const;

    // --- Metadata ---

    std::string getRawMetadata() const;
    MetadataFormat getMetadataFormat() const;

    // --- Channel attributes ---

    int getNumChannelAttributes() const;
    int getChannelAttributeIndex(const std::string& attributeName) const;
    std::string getChannelAttributeName(int attributeIndex) const;
    std::string getChannelAttributeValue(int channelIndex,
                                         const std::string& attributeName) const;

    // --- Buffer size helper ---

    // Compute the required buffer size (in bytes) for a given read operation.
    int getBlockSize(
        const std::tuple<int,int>& blockSize,
        int refChannel, int numChannels,
        int numSlices, int numFrames) const;

    // --- Utilities ---

    std::string toString() const;
    std::shared_ptr<CVScene> getCVScene();
};

typedef std::shared_ptr<slideio::Scene> ScenePtr;
```

---

## 4. Core Abstract Base Classes

These classes define the internal extension points. Driver authors implement `ImageDriver`, `CVSlide`, and `CVScene` to add support for new file formats.

### 4.1 CVSlide

**Header:** `slideio/core/cvslide.hpp`

Abstract base class for slide implementations.

```cpp
class CVSlide {
public:
    virtual ~CVSlide() = default;

    // Pure virtual
    virtual int getNumScenes() const = 0;
    virtual std::string getFilePath() const = 0;
    virtual std::shared_ptr<CVScene> getScene(int index) const = 0;

    // Virtual with default implementations
    virtual std::shared_ptr<CVScene> getSceneByName(const std::string& name);
    virtual const std::string& getRawMetadata() const;
    virtual MetadataFormat getMetadataFormat() const;
    virtual const std::list<std::string>& getAuxImageNames() const;
    virtual int getNumAuxImages() const;
    virtual std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const;

    void setDriverId(const std::string& driverId);
    const std::string& getDriverId() const;
    std::string toString() const;

    static MetadataFormat recognizeMetadataFormat(const std::string& metadata);
};

typedef std::shared_ptr<slideio::CVSlide> CVSlidePtr;
```

### 4.2 CVScene

**Header:** `slideio/core/cvscene.hpp`

Abstract base class for raster image access. Uses OpenCV types internally (`cv::Rect`, `cv::Size`, `cv::Range`, `cv::Mat`).

```cpp
class CVScene : public RefCounter {
public:
    virtual ~CVScene() = default;

    // --- Pure virtual (must be implemented) ---

    virtual std::string getFilePath() const = 0;
    virtual int getSceneIndex() const = 0;
    virtual const std::string& getDriverId() const = 0;
    virtual std::string getName() const = 0;
    virtual cv::Rect getRect() const = 0;
    virtual int getNumChannels() const = 0;
    virtual slideio::DataType getChannelDataType(int channel) const = 0;
    virtual Resolution getResolution() const = 0;
    virtual double getMagnification() const = 0;
    virtual Compression getCompression() const = 0;

    // The primary raster reading method that drivers must implement.
    virtual void readResampledBlockChannelsEx(
        const cv::Rect& blockRect, const cv::Size& blockSize,
        const std::vector<int>& componentIndices,
        int zSliceIndex, int tFrameIndex,
        cv::OutputArray output) = 0;

    // --- Virtual with defaults ---

    virtual int getNumZSlices() const;            // default: 1
    virtual int getNumTFrames() const;            // default: 1
    virtual std::string getChannelName(int channel) const;
    virtual double getZSliceResolution() const;   // default: 0
    virtual double getTFrameResolution() const;   // default: 0

    // 2D reading (built on readResampledBlockChannelsEx)
    virtual void readBlock(const cv::Rect& blockRect, cv::OutputArray output);
    virtual void readBlockChannels(const cv::Rect& blockRect,
                                   const std::vector<int>& channelIndices,
                                   cv::OutputArray output);
    virtual void readResampledBlock(const cv::Rect& blockRect,
                                    const cv::Size& blockSize,
                                    cv::OutputArray output);
    virtual void readResampledBlockChannels(const cv::Rect& blockRect,
                                           const cv::Size& blockSize,
                                           const std::vector<int>& channelIndices,
                                           cv::OutputArray output);

    // 4D reading
    virtual void read4DBlock(const cv::Rect& blockRect,
                            const cv::Range& zSliceRange,
                            const cv::Range& timeFrameRange,
                            cv::OutputArray output);
    virtual void read4DBlockChannels(const cv::Rect& blockRect,
                                     const std::vector<int>& channelIndices,
                                     const cv::Range& zSliceRange,
                                     const cv::Range& timeFrameRange,
                                     cv::OutputArray output);
    virtual void readResampled4DBlock(const cv::Rect& blockRect,
                                     const cv::Size& blockSize,
                                     const cv::Range& zSliceRange,
                                     const cv::Range& timeFrameRange,
                                     cv::OutputArray output);
    virtual void readResampled4DBlockChannels(
        const cv::Rect& blockRect, const cv::Size& blockSize,
        const std::vector<int>& channelIndices,
        const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
        cv::OutputArray output);

    // Zoom levels
    virtual int getNumZoomLevels() const;
    virtual const LevelInfo* getZoomLevelInfo(int level) const;

    // Auxiliary images
    virtual const std::list<std::string>& getAuxImageNames() const;
    virtual int getNumAuxImages() const;
    virtual std::shared_ptr<CVScene> getAuxImage(const std::string& imageName) const;

    // Metadata
    virtual std::string getRawMetadata() const;
    virtual MetadataFormat getMetadataFormat() const;

    // Channel attributes
    virtual int defineChannelAttribute(const std::string& attributeName);
    virtual int getChannelAttributeIndex(const std::string& attributeName) const;
    virtual void setChannelAttribute(int channelIndex,
                                    const std::string& attributeName,
                                    const std::string& attributeValues);
    virtual std::string getChannelAttributeValue(int channelIndex,
                                                 const std::string& attributeName) const;
    virtual const std::string& getChannelAttributeValue(int channelIndex,
                                                        int attributeIndex) const;
    virtual const std::string& getChannelAttributeName(int index) const;
    virtual int getNumChannelAttributes() const;

    std::string toString() const;
};

typedef std::shared_ptr<slideio::CVScene> CVScenePtr;
```

### 4.3 ImageDriver

**Header:** `slideio/core/imagedriver.hpp`

Base class that every format driver must implement.

```cpp
class ImageDriver {
public:
    virtual ~ImageDriver();

    // Unique identifier for this driver (e.g. "SVS", "CZI").
    virtual std::string getID() const = 0;

    // Open a file and return a CVSlide.
    virtual std::shared_ptr<CVSlide> openFile(const std::string& filePath) = 0;

    // File extension pattern (e.g. "*.svs;*.tif").
    virtual std::string getFileSpecs() const = 0;

    // Return true if this driver can open the given file.
    virtual bool canOpenFile(const std::string& filePath) const;
};
```

### 4.4 LevelInfo

**Header:** `slideio/core/levelinfo.hpp`

Describes one level in a multi-resolution zoom pyramid.

```cpp
class LevelInfo {
public:
    LevelInfo();
    LevelInfo(int level, const Size& size, double scale,
              double magnification, const Size& tileSize);
    LevelInfo(const LevelInfo& other);

    int getLevel() const;
    void setLevel(int level);

    // Pixel dimensions of this level.
    Size getSize() const;
    void setSize(const Size& size);

    // Scale factor relative to the base level (1.0 = full resolution).
    double getScale() const;
    void setScale(double scale);

    // Effective magnification at this level.
    double getMagnification() const;
    void setMagnification(double magnification);

    // Tile dimensions used for internal storage.
    Size getTileSize() const;
    void setTileSize(const Size& tileSize);

    // Total number of tiles at this level.
    int getTileCount() const;
    void updateTileCount() const;

    // Bounding rectangle of a tile by index.
    Rect getTileRect(int tileIndex) const;

    bool operator==(const LevelInfo& other) const;
    LevelInfo& operator=(const LevelInfo& other);
    std::string toString() const;
};
```

### 4.5 ImageDriverManager

**Header:** `slideio/slideio/imagedrivermanager.hpp`

Singleton that manages all registered image drivers.

```cpp
class ImageDriverManager {
public:
    static std::vector<std::string> getDriverIDs();
    static std::shared_ptr<slideio::ImageDriver> findDriver(const std::string& filePath);
    static std::shared_ptr<CVSlide> openSlide(const std::string& filePath,
                                              const std::string& driver);
    static void setLogLevel(const std::string& level);
    static std::string getVersion();
};
```

### 4.6 CVTools

**Header:** `slideio/core/tools/cvtools.hpp`

Utility class for OpenCV type conversions and matrix operations.

```cpp
class CVTools {
public:
    static std::shared_ptr<CVSlide> cvOpenSlide(const std::string& path,
                                                const std::string& driver);
    static std::vector<std::string> cvGetDriverIDs();

    // DataType <-> OpenCV type conversions
    static DataType fromOpencvType(int type);
    static int toOpencvType(DataType dt);
    static bool isValidDataType(slideio::DataType dt);
    static int cvGetDataTypeSize(DataType dt);
    static int cvTypeFromDataType(DataType dt);
    static std::string dataTypeToString(DataType dataType);
    static std::string compressionToString(Compression dataType);

    // Extract/insert slices from multidimensional matrices
    static void extractSliceFrom3D(cv::Mat mat3D, int sliceIndex,
                                   cv::OutputArray output);
    static void extractSliceFromMultidimMatrix(cv::Mat multidimMat,
                                              const std::vector<int>& indices,
                                              cv::OutputArray output);
    static void insertSliceInMultidimMatrix(cv::Mat& multidimMat,
                                           const cv::Mat& sliceMat,
                                           const std::vector<int>& indices);

    static DataType getMatDataType(const cv::Mat& mat);
};
```

---

## 5. Base Types

### 5.1 Enumerations

**Header:** `slideio/base/slideio_enums.hpp`

#### DataType

Pixel channel data types.

| Value | Description |
|---|---|
| `DT_Byte` | Unsigned 8-bit integer |
| `DT_Int8` | Signed 8-bit integer |
| `DT_UInt16` | Unsigned 16-bit integer |
| `DT_Int16` | Signed 16-bit integer |
| `DT_UInt32` | Unsigned 32-bit integer |
| `DT_Int32` | Signed 32-bit integer |
| `DT_Int64` | Signed 64-bit integer |
| `DT_UInt64` | Unsigned 64-bit integer |
| `DT_Float16` | 16-bit floating point |
| `DT_Float32` | 32-bit floating point |
| `DT_Float64` | 64-bit floating point |
| `DT_Unknown` | Unknown data type |
| `DT_None` | No data type |

#### Compression

Image compression schemes.

| Value | Description |
|---|---|
| `Unknown` | Unknown compression |
| `Uncompressed` | No compression |
| `Jpeg` | JPEG |
| `JpegXR` | JPEG XR |
| `Png` | PNG |
| `Jpeg2000` | JPEG 2000 |
| `LZW` | LZW |
| `HuffmanRL` | Huffman run-length |
| `CCITT_T4` | CCITT Group 3 fax |
| `CCITT_T6` | CCITT Group 4 fax |
| `JpegOld` | Old-style JPEG |
| `Zlib` | Zlib/Deflate |
| `PackBits` | PackBits |
| `RLE_LW` | RLE (LW variant) |
| `RLE_HC` | RLE (HC variant) |
| `RLE_BL` | RLE (BL variant) |
| `PKZIP` | PKZIP |
| `GIF` | GIF |
| `BIGGIF` | Big GIF |
| `RLE` | Generic RLE |
| `BMP` | BMP |
| `JpegLossless` | Lossless JPEG |
| `VP8` | VP8 (WebP) |
| *(and others)* | |

#### MetadataFormat

```cpp
enum class MetadataFormat {
    None,       // No metadata available
    Unknown,    // Metadata exists but format is unrecognized
    Text,       // Plain text
    JSON,       // JSON
    XML         // XML
};
```

### 5.2 Geometric Types

#### Size

**Header:** `slideio/base/size.hpp`

```cpp
class Size {
public:
    int32_t width;
    int32_t height;

    Size();
    Size(int32_t _width, int32_t _height);
    Size(const cv::Size& cvSize);

    operator cv::Size() const;

    bool operator==(const Size& other) const;
    bool operator!=(const Size& size) const;

    int64_t area() const;
    bool empty() const;    // true if width <= 0 or height <= 0
};
```

#### Rect

**Header:** `slideio/base/rect.hpp`

```cpp
class Rect {
public:
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;

    Rect();
    Rect(int32_t _x, int32_t _y, int32_t _width, int32_t _height);
    Rect(const cv::Rect& cvRect);

    operator cv::Rect() const;

    friend bool operator==(const Rect& lhs, const Rect& rhs);

    Size size() const;
    int64_t area() const;
    bool empty() const;    // true if width <= 0 or height <= 0
    bool valid() const;    // true if width > 0 and height > 0
};
```

#### Range

**Header:** `slideio/base/range.hpp`

A half-open interval `[start, end)`.

```cpp
class Range {
public:
    int32_t start;
    int32_t end;

    Range();
    Range(int32_t _start, int32_t _end);
    Range(const cv::Range& r);

    operator cv::Range() const;

    int32_t size() const;       // end - start
    bool empty() const;         // true if end <= start
    static Range all();         // Range(INT_MIN, INT_MAX)

    bool operator==(const Range& other) const;
    bool operator!=(const Range& other) const;

    // Intersection
    friend Range operator&(const Range& r1, const Range& r2);
    Range& operator&=(const Range& r);

    // Shift
    friend Range operator+(const Range& r, int32_t delta);
    friend Range operator-(const Range& r, int32_t delta);
};
```

#### Resolution

**Header:** `slideio/base/resolution.hpp`

Pixel spacing in meters per pixel.

```cpp
class Resolution {
public:
    double x;   // Horizontal resolution (meters/pixel)
    double y;   // Vertical resolution (meters/pixel)

    Resolution();
    Resolution(double _x, double _y);

    bool operator==(const Resolution& pt) const;
};
```

### 5.3 Exceptions

**Header:** `slideio/base/exceptions.hpp`

```cpp
struct RuntimeError : public std::exception {
public:
    RuntimeError();

    // Stream-style error message construction.
    template <typename T>
    RuntimeError& operator<<(T rhs);

    const char* what() const noexcept override;
};

// Convenience macro: throws with source file and line number.
#define RAISE_RUNTIME_ERROR  throw slideio::RuntimeError() << __FILE__ << ":" << __LINE__ << ":"
```

---

## 6. Converter API

**Headers:** `slideio/converter/`

The converter module converts scenes into tiled multi-resolution image files (SVS or OME-TIFF format).

### Convert function

```cpp
namespace slideio::converter {
    // Convert a scene to a file.
    // - inputScene:    source scene to read from
    // - parameters:    conversion settings (format, encoding, tiling, etc.)
    // - outputPath:    destination file path
    // - tileBatchSize: number of tiles to process in one batch
    // - cb:            optional progress callback, receives percentage (0-100)
    void convertScene(
        std::shared_ptr<Scene> inputScene,
        ConverterParameters& parameters,
        const std::string& outputPath,
        int tileBatchSize,
        ConverterCallback cb = nullptr);

    // Progress callback type.
    typedef const std::function<void(int)>& ConverterCallback;
}
```

### Converter parameters

#### Image format and encoding enums

```cpp
namespace slideio::converter {
    enum ImageFormat  { Unknown, SVS, OME_TIFF };
    enum Encoding     { UNKNOWN_ENCODING, JPEG, JPEG2000 };
    enum Container    { UNKNOWN_CONTAINER, TIFF_CONTAINER };
}
```

#### ConverterParameters (base class)

```cpp
class ConverterParameters {
public:
    ConverterParameters(ImageFormat format, Container containerType,
                        Compression compression);
    ConverterParameters();

    ImageFormat getFormat() const;

    // Region of interest (default: entire scene).
    const Rect& getRect() const;
    void setRect(const Rect& rect);

    // Slice, channel, and time-frame sub-ranges.
    void setSliceRange(const Range& range);
    const Range& getSliceRange() const;
    void setChannelRange(const Range& range);
    const Range& getChannelRange() const;
    void setTFrameRange(const Range& range);
    const Range& getTFrameRange() const;

    Compression getEncoding() const;
    Container getContainerType() const;

    std::shared_ptr<EncodeParameters> getEncodeParameters();
    std::shared_ptr<ContainerParameters> getContainerParameters();

    bool isValid() const;

    int getTileBatchSize() const;
    void setTileBatchSize(int batchSize);

    // Fill in defaults from the source scene for any unset parameters.
    void updateNotDefinedParameters(const std::shared_ptr<CVScene>& scene);
};
```

#### SVS converter parameters

```cpp
// SVS with JPEG encoding
class SVSJpegConverterParameters : public SVSConverterParameters {
public:
    SVSJpegConverterParameters();

    void setQuality(int q);      // JPEG quality (0-100, default 95)
    int getQuality() const;

    // Inherited from SVSConverterParameters:
    int getTileWidth() const;    void setTileWidth(int tileWidth);
    int getTileHeight() const;   void setTileHeight(int tileHeight);
    int getNumZoomLevels() const;      void setNumZoomLevels(int n);
    int getNumReadingThreads() const;  void setNumReadingThreads(int n);
    int getNumEncodingThreads() const; void setNumEncodingThreads(int n);
};

// SVS with JPEG 2000 encoding
class SVSJp2KConverterParameters : public SVSConverterParameters {
public:
    SVSJp2KConverterParameters();

    void setCompressionRate(float rate);   // Compression ratio (default 4.5)
    float getCompressionRate() const;
};
```

#### OME-TIFF converter parameters

```cpp
// OME-TIFF with JPEG encoding
class OMETIFFJpegConverterParameters : public OMETIFFConverterParameters {
    // Same interface as SVSJpegConverterParameters
};

// OME-TIFF with JPEG 2000 encoding
class OMETIFFJp2KConverterParameters : public OMETIFFConverterParameters {
    // Same interface as SVSJp2KConverterParameters
};
```

### TIFF container parameters

```cpp
class TIFFContainerParameters : public ContainerParameters {
public:
    TIFFContainerParameters();

    int getTileWidth() const;
    void setTileWidth(int tileWidth);

    int getTileHeight() const;
    void setTileHeight(int tileHeight);

    int getNumZoomLevels() const;
    void setNumZoomLevels(int numZoomLevels);

    int getNumReadingThreads() const;
    void setNumReadingThreads(int numReadingThreads);

    int getNumEncodingThreads() const;
    void setNumEncodingThreads(int numEncodingThreads);
};
```

### Conversion example

```cpp
#include "slideio/slideio/slideio.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"

auto slide = slideio::openSlide("/path/to/input.czi");
auto scene = slide->getScene(0);

slideio::converter::SVSJpegConverterParameters params;
params.setQuality(90);
params.setTileWidth(256);
params.setTileHeight(256);
params.setNumZoomLevels(4);
params.setNumEncodingThreads(4);

slideio::converter::convertScene(
    scene, params, "/path/to/output.svs", 16,
    [](int progress) { std::cout << progress << "%" << std::endl; });
```

---

## 7. Transformer API

**Headers:** `slideio/transformer/`

The transformer module provides image filters and color transformations that can be applied to scenes. A `TransformerScene` wraps an existing scene and applies a chain of transformations on-the-fly during reads.

### TransformationType enum

```cpp
enum class TransformationType {
    Unknown,
    ColorTransformation,
    GaussianBlurFilter,
    MedianBlurFilter,
    SobelFilter,
    ScharrFilter,
    LaplacianFilter,
    BilateralFilter,
    CannyFilter
};
```

### Transformation base classes

```cpp
// Abstract interface
class Transformation {
public:
    virtual TransformationType getType() const = 0;
};

// Extended base with apply logic
class TransformationEx : public Transformation {
public:
    TransformationEx();
    virtual ~TransformationEx() = default;

    TransformationType getType() const override;

    // Compute output channel types given input channel types.
    virtual std::vector<DataType> computeChannelDataTypes(
        const std::vector<DataType>& channels) const;

    // Border inflation needed by the filter kernel.
    virtual int getInflationValue() const;

    // Apply the transformation to a block.
    virtual void applyTransformation(const cv::Mat& block,
                                     cv::OutputArray transformedBlock) const = 0;
};
```

### Color transformation

```cpp
enum class ColorSpace {
    RGB, GRAY, HSV, HLS, YUV, YCbCr, XYZ, LAB, LUV
};

class ColorTransformation : public TransformationEx {
public:
    ColorTransformation();
    ColorTransformation(ColorSpace colorSpace);

    ColorSpace getColorSpace() const;
    void setColorSpace(ColorSpace colorSpace);

    void applyTransformation(const cv::Mat& block,
                             cv::OutputArray transformedBlock) const override;
    std::vector<DataType> computeChannelDataTypes(
        const std::vector<DataType>& channels) const override;
};
```

### Filters

#### GaussianBlurFilter

```cpp
class GaussianBlurFilter : public TransformationEx {
public:
    int getKernelSizeX() const;   void setKernelSizeX(int v);
    int getKernelSizeY() const;   void setKernelSizeY(int v);
    double getSigmaX() const;     void setSigmaX(double v);
    double getSigmaY() const;     void setSigmaY(double v);
};
```

#### MedianBlurFilter

```cpp
class MedianBlurFilter : public TransformationEx {
public:
    int getKernelSize() const;    void setKernelSize(int v);
};
```

#### SobelFilter

Edge detection using Sobel operator.

```cpp
class SobelFilter : public TransformationEx {
public:
    DataType getDepth() const;    void setDepth(const DataType& depth);
    int getDx() const;            void setDx(int dx);        // X derivative order
    int getDy() const;            void setDy(int dy);        // Y derivative order
    int getKernelSize() const;    void setKernelSize(int k);
    double getScale() const;      void setScale(double s);
    double getDelta() const;      void setDelta(double d);
};
```

#### ScharrFilter

Edge detection using Scharr operator (3x3 only, more accurate than Sobel).

```cpp
class ScharrFilter : public TransformationEx {
public:
    DataType getDepth() const;    void setDepth(const DataType& depth);
    int getDx() const;            void setDx(int dx);
    int getDy() const;            void setDy(int dy);
    double getScale() const;      void setScale(double s);
    double getDelta() const;      void setDelta(double d);
};
```

#### LaplacianFilter

```cpp
class LaplacianFilter : public TransformationEx {
public:
    DataType getDepth() const;    void setDepth(const DataType& depth);
    int getKernelSize() const;    void setKernelSize(int k);
    double getScale() const;      void setScale(double s);
    double getDelta() const;      void setDelta(double d);
};
```

#### BilateralFilter

Edge-preserving smoothing.

```cpp
class BilateralFilter : public TransformationEx {
public:
    int getDiameter() const;      void setDiameter(int d);
    double getSigmaColor() const; void setSigmaColor(double v);
    double getSigmaSpace() const; void setSigmaSpace(double v);
};
```

#### CannyFilter

Edge detection using Canny algorithm.

```cpp
class CannyFilter : public TransformationEx {
public:
    double getThreshold1() const;  void setThreshold1(double v);
    double getThreshold2() const;  void setThreshold2(double v);
    int getApertureSize() const;   void setApertureSize(int v);
    bool getL2Gradient() const;    void setL2Gradient(bool v);
};
```

### TransformerScene

Wraps an existing `CVScene` and applies a chain of transformations during reads.

```cpp
class TransformerScene : public CVScene {
public:
    TransformerScene(std::shared_ptr<CVScene> originScene,
                     const std::list<std::shared_ptr<Transformation>>& transformations);

    // All CVScene methods are delegated to the origin scene,
    // with raster data passing through the transformation chain.

    std::shared_ptr<CVScene> getOriginScene() const;
};
```

### Transformer example

```cpp
#include "slideio/slideio/slideio.hpp"
#include "slideio/transformer/transformerscene.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"
#include "slideio/transformer/colortransformation.hpp"

auto slide = slideio::openSlide("/path/to/slide.svs");
auto cvScene = slide->getScene(0)->getCVScene();

// Build a transformation chain: blur then convert to grayscale
auto blur = std::make_shared<slideio::GaussianBlurFilter>();
blur->setKernelSizeX(5);
blur->setKernelSizeY(5);
blur->setSigmaX(1.5);

auto gray = std::make_shared<slideio::ColorTransformation>(slideio::ColorSpace::GRAY);

std::list<std::shared_ptr<slideio::Transformation>> chain = {blur, gray};

auto transformed = std::make_shared<slideio::TransformerScene>(cvScene, chain);
// Use 'transformed' as a CVScene — reads will return blurred grayscale data.
```

---

## 8. Image Tools API

**Headers:** `slideio/imagetools/`

Low-level image encoding/decoding utilities. Most users will not need these directly.

### Encode parameters

```cpp
// Base class
class EncodeParameters {
public:
    Compression getCompression() const;
};

// JPEG encoding
class JpegEncodeParameters : public EncodeParameters {
public:
    JpegEncodeParameters(int quality = 95);
    int getQuality() const;
    void setQuality(int quality);
};

// JPEG 2000 encoding
class JP2KEncodeParameters : public EncodeParameters {
public:
    enum Codec { J2KStream, J2KFile };

    JP2KEncodeParameters(float rate = 4.5, Codec codec = Codec::J2KStream);

    float getCompressionRate() const;
    void setCompressionRate(float rate);

    Codec getCodecFormat() const;
    void setCodecFormat(Codec codec);

    int getSubSamplingDx() const;  void setSubSamplingDx(int v);
    int getSubSamplingDy() const;  void setSubSamplingDy(int v);
};
```

### ImageTools utility class

```cpp
class ImageTools {
public:
    struct ImageHeader {
        int channels = 0;
        std::vector<int> chanelTypes;   // OpenCV channel types
        cv::Size size = {};
    };

    // --- Standard image I/O ---
    static void readSmallImageRaster(const std::string& path, cv::OutputArray output);
    static void writeSmallImageRaster(const std::string& path,
                                      Compression compression, cv::Mat raster);

    // --- JPEG ---
    static void decodeJpegStream(const uint8_t* data, size_t size,
                                 cv::OutputArray output);
    static void encodeJpeg(const cv::Mat& raster,
                           std::vector<uint8_t>& encodedStream,
                           const JpegEncodeParameters& params);

    // --- JPEG 2000 ---
    static void readJp2KFile(const std::string& path, cv::OutputArray output);
    static void readJp2KStremHeader(const uint8_t* data, size_t dataSize,
                                    ImageHeader& header);
    static void decodeJp2KStream(const uint8_t* data, size_t dataSize,
                                 cv::OutputArray output,
                                 const std::vector<int>& channelIndices = {},
                                 bool forceYUV = false);
    static int encodeJp2KStream(const cv::Mat& mat, uint8_t* buffer,
                                int bufferSize,
                                const JP2KEncodeParameters& parameters);

    // --- JPEG XR ---
    static void readJxrImage(const std::string& path, cv::OutputArray output);
    static void decodeJxrBlock(const uint8_t* data, size_t size,
                               cv::OutputArray output);

    // --- Bitmap ---
    static void readBitmap(const std::string& path, cv::OutputArray output);

    // --- Image comparison ---
    static double computeSimilarity(const cv::Mat& left, const cv::Mat& right,
                                    bool ignoreTypes = false);
    static double computeSimilarity2(const cv::Mat& left, const cv::Mat& right);
    static double compareHistograms(const cv::Mat& left, const cv::Mat& right,
                                    int bins);
};
```

---

## Supported Image Formats

| Driver ID | Format | Extensions |
|---|---|---|
| `SVS` | Aperio SVS | `.svs` |
| `AFI` | Aperio AFI | `.afi` |
| `SCN` | Leica SCN | `.scn` |
| `CZI` | Zeiss CZI | `.czi` |
| `ZVI` | Zeiss ZVI | `.zvi` |
| `NDPI` | Hamamatsu NDPI | `.ndpi` |
| `VSI` | Olympus VSI | `.vsi` |
| `DCM` | DICOM | `.dcm` |
| `QPTIFF` | PerkinElmer QPTIFF | `.qptiff` |
| `OMETIFF` | OME-TIFF | `.ome.tif`, `.ome.tiff` |
| `GDAL` | GDAL-supported formats | Various |

---

## Key Design Patterns

- **Smart pointers everywhere.** All public classes are returned and passed as `std::shared_ptr`.
- **Two API layers.** The public API (`Slide`, `Scene`) uses plain buffers and tuples. The internal API (`CVSlide`, `CVScene`) uses OpenCV types. Driver authors work with the internal API.
- **Single required override.** Drivers only need to implement `readResampledBlockChannelsEx()`; all other read methods are built on top of it.
- **Zoom pyramid support.** Multi-resolution levels are exposed through `LevelInfo` objects.
- **Multidimensional images.** Z-slices and time frames are first-class dimensions, with dedicated 4D read methods.
- **Transformation chaining.** `TransformerScene` applies a list of transformations lazily during reads.
- **Pluggable drivers.** Each format driver is an independent shared library loaded at runtime.
