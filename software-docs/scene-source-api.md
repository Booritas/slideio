# Computed Scenes: `ISceneSource`, `SceneMetadata` and `makeScene`

Introduced on branch `feature/scene-source`. This page describes the public API
for presenting **pixels an application computes itself** as an ordinary
`slideio::Scene`, so code written against SlideIO's scene interface, the
converter included, can consume them without knowing where they came from.

This page is the user-facing summary and doubles as release-note text. The
authoritative contract is the Doxygen in the two headers.

## The API

Until now every `Scene` came from a file opened by a driver. Three public
declarations make it possible to build one from anything that can produce a
raster: a processing pipeline, a synthetic test pattern, a remote tile service.

- **`ISceneSource`** (`slideio/slideio/scenesource.hpp`) is the interface you
  implement. Its one required method, `readBlock`, fills a caller-supplied
  buffer with a rectangle of the scene, resampled to a requested size, for a
  list of channels at a given Z slice and time frame. Duplicated channel
  indices are permitted; the channel is delivered once per occurrence, in
  request order. `readLevelBlock` lets a source that keeps its own pyramid
  serve a zoom level directly; a source without one returns `false` and
  SlideIO falls back to reading the base level. `supportsConcurrentReads` says
  whether two reads may overlap; it defaults to `false`, and SlideIO then
  serialises reads for you.
- **`SceneMetadata`** (`slideio/slideio/scenemetadata.hpp`) describes the
  scene: its name, rectangle, per-channel data types and names, resolution,
  magnification, Z and T dimensions, zoom levels and colour profile.
- **`makeScene(source, metadata)`** returns a `std::shared_ptr<slideio::Scene>`
  backed by the source.

```cpp
#include "slideio/slideio/scenesource.hpp"
#include "slideio/slideio/scenemetadata.hpp"

class Gradient : public slideio::ISceneSource {
public:
    void readBlock(const slideio::Rect& rect, const slideio::Size& blockSize,
                   const std::vector<int>& channels, int zSlice, int tFrame,
                   void* buffer, size_t bufferSize) override {
        // fill buffer with blockSize.width * blockSize.height * channels.size() bytes
    }
};

slideio::SceneMetadata m;
m.name = "gradient";
m.rect = slideio::Rect(0, 0, 4096, 4096);
m.channelDataTypes.assign(3, slideio::DataType::DT_Byte);
m.levels.emplace_back(0, slideio::Size(4096, 4096), 1.0, 20.0, slideio::Size(256, 256));

std::shared_ptr<slideio::Scene> scene = slideio::makeScene(std::make_shared<Gradient>(), m);
```

The new headers, like every installed SlideIO header, do not include OpenCV.

## Validation and errors

Metadata is checked once, when the scene is made: a null source, a rectangle
without positive size, no channels or an unusable channel type, channel names
or significant bits that do not match the channel count, a zoom pyramid whose
level 0 does not match the scene or whose scales do not decrease, Z or T
counts below 1, and similar mistakes raise **`slideio::RuntimeError`** from
`makeScene` rather than surfacing later as a failed or out-of-bounds read.

Reads are checked before the source is called: a non-positive block or
rectangle, an out-of-range channel, level, Z slice or time frame, and a
request mixing channels of different data types all raise
`slideio::RuntimeError`, and the source never sees them.

The change is **additive**. No existing class changes behaviour, and there is
no source or binary incompatibility.

## Converting computed scenes

A computed scene converts through `slideio::converter::convertScene` like any
other. The converter used to give each of its reader threads its own copy of
the scene by re-opening the source file by path and driver, which a scene with
no file cannot do. It now **shares a computed scene across its reader threads**
instead of re-opening it (a computed scene reports the driver id `SOURCE`);
file-backed scenes are re-opened exactly as before. A failure to prepare a
reader thread's scene is now reported as an exception from `convertScene`
rather than terminating the process.

## Limitations

- **Auxiliary images.** The names a source reports through
  `ISceneSource::getAuxImageNames` are forwarded by the scene, but the images
  themselves are not yet retrievable: `Scene::getAuxImage` on a computed scene
  raises `slideio::RuntimeError`.
- **Compression for OME-TIFF export.** `convertScene` writes JPEG or JPEG 2000;
  there is no uncompressed option. A JPEG 2000 compression rate of 1 encodes
  losslessly.
