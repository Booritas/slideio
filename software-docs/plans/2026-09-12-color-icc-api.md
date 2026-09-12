# Colour / ICC Public API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give machine-learning callers colorimetrically comparable pixels across scanners, by reading each slide's embedded ICC profile and converting blocks into a chosen device-independent space.

**Architecture:** Parse-free colour types in `slideio-core` so all eleven driver libraries can return profile bytes without seeing an ICC engine; the engine (`IccTransform`, wrapping lcms2) private to `slideio-imagetools`; a `ColorManagement` transformation in `slideio-transformer` reusing the existing `transformScene` decorator path; pass-through getters on the public `Scene`. Two new virtuals on `TransformationEx` — `bindToSource` and `amendColorProfile` — let a transformation learn its source scene, which the current contract does not allow.

**Tech Stack:** C++17, CMake, Conan 2, lcms2 (`lcms/2.16`), OpenCV, libtiff, DCMTK, GDAL, Google Test, pybind11.

**Spec:** `software-docs/specs/2026-09-12-color-icc-api-design.md`

## Global Constraints

- **Dependencies** come from conan center, from `extern/` as a git submodule, or nowhere. `lcms/2.16` is on conan center; do not vendor it.
- **lcms2 is visible only inside `slideio-imagetools`.** `lcms2.h` must never be included by `slideio-core`, `slideio-transformer`, `slideio` or any driver. Link it `PRIVATE`.
- **No `thread_local` holding a file handle or any file state**, per the CLAUDE.md concurrency contract.
- **Channel order is RGB, not OpenCV BGR.** slideio block buffers are interleaved in scene channel order. Never call `cv::cvtColor` to "fix" channel order on these paths.
- **Internal docs** go in `software-docs/`. Public API docs go in `docs-src/`. Never add anything to `docs/`.
- **Tests that read image files** must guard with `SLIDEIO_SKIP_IF_IMAGE_MISSING(path)` after computing the path with `TestTools::getTestImagePath(...)`.
- **Errors** are raised with `RAISE_RUNTIME_ERROR << "..."` from `slideio/core/exceptions.hpp`.
- **Commit style:** lowercase imperative subject line, blank line, then a body explaining *why*. No `feat:`/`fix:` prefixes, no attribution footers. Match `git log`.
- **Do not cite line numbers** in code comments or docs; anchor to greppable symbol names, because line numbers shift as this plan is executed.

## Build and test commands

`install.py -a conan` **does not work on this machine** — it aborts on `src/single_tests/jp2k`, so every module after it is left without its generated CMake files, and configure then fails with a missing-package error that reads like a broken checkout. Use this instead:

```bash
export PATH="/c/Users/Stanislav/anaconda3/envs/conan2/Scripts:$PATH"

# Regenerate per-module conan output for BOTH profiles (VS is multi-config).
for f in $(find src -name "conanfile.*" | grep -v single_tests); do d=$(dirname "$f")
  for pr in x86_64_release x86_64_debug; do
    conan install -nr -pr:b conan/Windows/$pr -pr:h conan/Windows/$pr \
      -of "$d/cmake" -g CMakeDeps -g CMakeToolchain -b missing "$f"
  done
done

cmake -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=./cmake/conan_toolchain.cmake -S . -B build
cmake --build build --config Release --target slideio_tests -- -m
```

Four facts behind that: conan 2.30.0 lives at `C:\Users\Stanislav\anaconda3\envs\conan2\Scripts\conan.exe` and is not on the default PATH; the private remote `slideio` (http://159.89.28.69) is unreachable so `-nr` is required for cached packages; dependencies are per module, not root; and both profiles must be generated.

**Task 3 changes this once.** `lcms/2.16` is a new package and will not be in the local cache at `d:\conan2\`, so `-nr` cannot fetch it. For the imagetools module only, target conan center explicitly — `-r conancenter` rather than `-nr`, which fetches the new package without touching the dead `slideio` remote:

```bash
conan install -r conancenter -pr:b conan/Windows/x86_64_release -pr:h conan/Windows/x86_64_release \
  -of src/slideio/imagetools/cmake -g CMakeDeps -g CMakeToolchain -b missing src/slideio/imagetools/conanfile.py
```

Running tests:

```bash
./build/Release/slideio_tests.exe --gtest_filter="ColorProfile.*"
./build/Release/slideio_transformer_tests.exe --gtest_filter="ColorManagement.*"
```

---

## File Structure

**Created:**

| File | Responsibility |
|---|---|
| `src/slideio/core/colorprofile.hpp` / `.cpp` | Parse-free colour types: `ColorProfile`, `ColorProfileInfo`, four enums |
| `src/slideio/imagetools/icctransform.hpp` / `.cpp` | The only lcms2 consumer: header parsing, sRGB synthesis, block conversion |
| `src/slideio/transformer/colormanagement.hpp` / `.cpp` | `ColorManagement` transformation: policy, applicability, binding |
| `src/slideio/transformer/colormanagementwrap.hpp` / `.cpp` | Python-facing value wrapper, mirroring `ColorTransformationWrap` |
| `src/tests/main/test_colorprofile.cpp` | Core type tests |
| `src/tests/main/test_icctransform.cpp` | Engine tests, including the channel-order regression |
| `src/tests/transformer/test_colormanagement.cpp` | Transformation, policy matrix, error and concurrency tests |

**Modified:** `core/cvscene.hpp`, `core/CMakeLists.txt`, `slideio/scene.hpp`/`.cpp`, `imagetools/conanfile.py`, `imagetools/CMakeLists.txt`, `imagetools/tifftools.hpp`/`.cpp`, `transformer/transformationex.hpp`/`.cpp`, `transformer/transformationtype.hpp`/`.cpp`, `transformer/transformerscene.hpp`/`.cpp`, `transformer/CMakeLists.txt`, the svs/ndpi/dcm/gdal drivers, both test `CMakeLists.txt`, `software-docs/BREAKING_CHANGES.md`, and in the **separate** `slideio-python` repo: `src/pyscene.*`, `src/pybind.cpp`.

---

### Task 1: Core colour types

**Files:**
- Create: `src/slideio/core/colorprofile.hpp`, `src/slideio/core/colorprofile.cpp`
- Create: `src/tests/main/test_colorprofile.cpp`
- Modify: `src/slideio/core/CMakeLists.txt`, `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `slideio::ColorProfile` (ctors `ColorProfile()`, `explicit ColorProfile(std::vector<uint8_t>)`; methods `isEmpty() const -> bool`, `getSource() const -> ColorProfileSource`, `setSource(ColorProfileSource)`, `getData() const -> const std::vector<uint8_t>&`, `getSize() const -> size_t`); `slideio::ColorProfileInfo` (aggregate struct, `toString() const -> std::string`); enums `ColorProfileSource{None,Embedded,Assumed}`, `IccColorSpace{Unknown,Gray,RGB,CMYK,Lab,XYZ,YCbCr}`, `RenderingIntent{Perceptual,RelativeColorimetric,Saturation,AbsoluteColorimetric}`, `ColorTarget{sRGB,LinearRGB,Lab,XYZ}`.

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_colorprofile.cpp`:

```cpp
#include <gtest/gtest.h>
#include "slideio/core/colorprofile.hpp"

using namespace slideio;

TEST(ColorProfile, defaultIsAbsent)
{
    ColorProfile profile;
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(0u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
    ASSERT_TRUE(profile.getData().empty());
}

TEST(ColorProfile, constructedFromBytesIsEmbedded)
{
    std::vector<uint8_t> bytes{1, 2, 3, 4};
    ColorProfile profile(bytes);
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(4u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::Embedded, profile.getSource());
    ASSERT_EQ(bytes, profile.getData());
}

TEST(ColorProfile, emptyByteVectorStaysAbsent)
{
    // A driver that finds a zero-length tag must not claim a profile exists.
    ColorProfile profile(std::vector<uint8_t>{});
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}

TEST(ColorProfile, sourceIsSettable)
{
    ColorProfile profile(std::vector<uint8_t>{1, 2, 3});
    profile.setSource(ColorProfileSource::Assumed);
    ASSERT_EQ(ColorProfileSource::Assumed, profile.getSource());
}

TEST(ColorProfileInfo, defaultIsNotPresent)
{
    ColorProfileInfo info;
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
    ASSERT_EQ(IccColorSpace::Unknown, info.dataSpace);
    ASSERT_EQ(0u, info.dataSize);
}

TEST(ColorProfileInfo, toStringNamesPresenceAndDescription)
{
    ColorProfileInfo info;
    info.present = true;
    info.source = ColorProfileSource::Embedded;
    info.description = "Aperio RGB";
    info.dataSpace = IccColorSpace::RGB;
    info.dataSize = 3144;
    const std::string text = info.toString();
    ASSERT_NE(std::string::npos, text.find("Aperio RGB"));
    ASSERT_NE(std::string::npos, text.find("Embedded"));
    ASSERT_NE(std::string::npos, text.find("3144"));
}
```

- [ ] **Step 2: Add the test to the build and run it to verify it fails**

Add `test_colorprofile.cpp` to `set(TEST_SOURCES ...)` in `src/tests/main/CMakeLists.txt`, next to `test_color_tools.cpp`.

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL — `Cannot open include file: 'slideio/core/colorprofile.hpp'`.

- [ ] **Step 3: Write the header**

Create `src/slideio/core/colorprofile.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "slideio/core/slideio_core_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief where a scene's colour profile came from */
    enum class ColorProfileSource
    {
        /**@brief the file carries no profile*/
        None,
        /**@brief a real ICC profile read out of the file*/
        Embedded,
        /**@brief none embedded; sRGB assumed under MissingProfilePolicy*/
        Assumed,
    };

    /**@brief colour space of ICC profile data.
     *
     * Distinct from slideio::ColorSpace in the transformer, which describes
     * OpenCV conversions and is unrelated to ICC.*/
    enum class IccColorSpace { Unknown, Gray, RGB, CMYK, Lab, XYZ, YCbCr };

    /**@brief ICC rendering intent*/
    enum class RenderingIntent
    {
        Perceptual, RelativeColorimetric, Saturation, AbsoluteColorimetric
    };

    /**@brief device-independent space a scene's pixels may be converted into*/
    enum class ColorTarget { sRGB, LinearRGB, Lab, XYZ };

    /**@brief raw ICC profile bytes as found in a slide.
     *
     * A byte container only: it does not parse or validate its contents. Use
     * Scene::getColorProfileInfo() for the parsed header, or hand getData() to
     * an external colour management system. Parsing lives in slideio-imagetools
     * because it needs lcms2, which neither this module nor any driver may see.*/
    class SLIDEIO_CORE_EXPORTS ColorProfile
    {
    public:
        ColorProfile() = default;
        explicit ColorProfile(std::vector<uint8_t> iccBytes);
        bool isEmpty() const { return m_data.empty(); }
        ColorProfileSource getSource() const { return m_source; }
        void setSource(ColorProfileSource source) { m_source = source; }
        const std::vector<uint8_t>& getData() const { return m_data; }
        size_t getSize() const { return m_data.size(); }
    private:
        std::vector<uint8_t> m_data;
        ColorProfileSource m_source = ColorProfileSource::None;
    };

    /**@brief parsed ICC header facts. Populated by slideio-imagetools.*/
    struct SLIDEIO_CORE_EXPORTS ColorProfileInfo
    {
        bool present = false;
        ColorProfileSource source = ColorProfileSource::None;
        std::string description;
        std::string manufacturer;
        std::string model;
        std::string version;
        IccColorSpace dataSpace = IccColorSpace::Unknown;
        IccColorSpace connectionSpace = IccColorSpace::Unknown;
        RenderingIntent intent = RenderingIntent::RelativeColorimetric;
        std::array<double, 3> whitePoint{0.0, 0.0, 0.0};
        size_t dataSize = 0;
        std::string toString() const;
    };

    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, ColorProfileSource source);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, IccColorSpace space);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, RenderingIntent intent);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, ColorTarget target);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
```

- [ ] **Step 4: Write the implementation**

Create `src/slideio/core/colorprofile.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/colorprofile.hpp"
#include <ostream>
#include <sstream>

using namespace slideio;

ColorProfile::ColorProfile(std::vector<uint8_t> iccBytes) : m_data(std::move(iccBytes))
{
    // A zero-length tag is not a profile: leave the source at None so a caller
    // cannot be told a profile exists when nothing was found.
    m_source = m_data.empty() ? ColorProfileSource::None : ColorProfileSource::Embedded;
}

std::string ColorProfileInfo::toString() const
{
    std::ostringstream os;
    os << "ColorProfileInfo(present=" << (present ? "true" : "false")
       << ", source=" << source;
    if (present) {
        os << ", description='" << description << "'"
           << ", space=" << dataSpace
           << ", pcs=" << connectionSpace
           << ", intent=" << intent
           << ", size=" << dataSize;
    }
    os << ")";
    return os.str();
}

std::ostream& slideio::operator << (std::ostream& os, ColorProfileSource source)
{
    switch (source) {
    case ColorProfileSource::None: os << "None"; break;
    case ColorProfileSource::Embedded: os << "Embedded"; break;
    case ColorProfileSource::Assumed: os << "Assumed"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, IccColorSpace space)
{
    switch (space) {
    case IccColorSpace::Gray: os << "Gray"; break;
    case IccColorSpace::RGB: os << "RGB"; break;
    case IccColorSpace::CMYK: os << "CMYK"; break;
    case IccColorSpace::Lab: os << "Lab"; break;
    case IccColorSpace::XYZ: os << "XYZ"; break;
    case IccColorSpace::YCbCr: os << "YCbCr"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, RenderingIntent intent)
{
    switch (intent) {
    case RenderingIntent::Perceptual: os << "Perceptual"; break;
    case RenderingIntent::RelativeColorimetric: os << "RelativeColorimetric"; break;
    case RenderingIntent::Saturation: os << "Saturation"; break;
    case RenderingIntent::AbsoluteColorimetric: os << "AbsoluteColorimetric"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, ColorTarget target)
{
    switch (target) {
    case ColorTarget::sRGB: os << "sRGB"; break;
    case ColorTarget::LinearRGB: os << "LinearRGB"; break;
    case ColorTarget::Lab: os << "Lab"; break;
    case ColorTarget::XYZ: os << "XYZ"; break;
    default: os << "Unknown"; break;
    }
    return os;
}
```

Add both files to `set(SOURCE_FILES ...)` in `src/slideio/core/CMakeLists.txt`, next to the `slideio_enums` entries:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/colorprofile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colorprofile.cpp
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --config Release --target slideio_tests -- -m` then
`./build/Release/slideio_tests.exe --gtest_filter="ColorProfile*"`
Expected: 6 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/colorprofile.hpp src/slideio/core/colorprofile.cpp \
        src/slideio/core/CMakeLists.txt \
        src/tests/main/test_colorprofile.cpp src/tests/main/CMakeLists.txt
git commit -m "add parse-free colour profile types to slideio-core

ColorProfile carries raw ICC bytes and their provenance; ColorProfileInfo
carries the parsed header. They are split because eleven driver libraries need
to return profile bytes and none of them may see lcms2, which lives in
slideio-imagetools. A zero-length tag leaves the source at None so a driver
cannot report a profile that is not there."
```

---

### Task 2: Profile access on CVScene and Scene

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`, `src/slideio/slideio/scene.hpp`, `src/slideio/slideio/scene.cpp`
- Test: `src/tests/main/test_colorprofile.cpp`

**Interfaces:**
- Consumes: `ColorProfile` from Task 1.
- Produces: `virtual ColorProfile CVScene::getColorProfile() const` (defaults to an empty profile); `ColorProfile Scene::getColorProfile() const`.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_colorprofile.cpp`:

```cpp
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "tests/testlib/testtools.hpp"

TEST(ColorProfile, sceneWithoutProfileReportsAbsent)
{
    // PNG through the gdal driver carries no ICC profile, and no driver
    // overrides the new virtual yet, so this exercises the default.
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const ColorProfile profile = scene->getColorProfile();
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL — `class "slideio::Scene" has no member "getColorProfile"`.

- [ ] **Step 3: Add the virtual to CVScene**

In `src/slideio/core/cvscene.hpp`, add `#include "slideio/core/colorprofile.hpp"` and place this next to `getRawMetadata()`:

```cpp
        /**@brief returns the ICC colour profile embedded in the scene.
         *
         * The default returns an empty profile, which is the correct answer for
         * a format that carries no colorimetry. A driver overrides it when it
         * has real profile bytes. Per scene rather than per slide: a label and a
         * macro image are captured through different optics than the tissue
         * scan.*/
        virtual ColorProfile getColorProfile() const { return ColorProfile(); }
```

- [ ] **Step 4: Add the pass-through to Scene**

In `src/slideio/slideio/scene.hpp`, add `#include "slideio/core/colorprofile.hpp"` and declare, next to `getRawMetadata()`:

```cpp
        /**@brief returns the raw ICC colour profile embedded in the scene.
         *
         * Empty when the slide carries none. The bytes are the profile exactly
         * as stored, suitable for handing to an external colour management
         * system. Use #getColorProfileInfo for the parsed header.*/
        ColorProfile getColorProfile() const;
```

In `src/slideio/slideio/scene.cpp`, implement it next to the other forwarding methods:

```cpp
ColorProfile Scene::getColorProfile() const
{
    return m_scene->getColorProfile();
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cmake --build build --config Release --target slideio_tests -- -m` then
`./build/Release/slideio_tests.exe --gtest_filter="ColorProfile.sceneWithoutProfileReportsAbsent"`
Expected: PASS (or SKIPPED if `colors.png` is absent — rerun with the image present before committing).

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/slideio/scene.hpp \
        src/slideio/slideio/scene.cpp src/tests/main/test_colorprofile.cpp
git commit -m "expose the embedded colour profile on CVScene and Scene

One defaulted virtual, so all eleven driver libraries keep compiling untouched
and a format with no colorimetry answers correctly without any code. Per scene
rather than per slide because a label and a macro image are captured through
different optics than the tissue scan."
```

---

### Task 3: lcms2 dependency and ICC header parsing

**Files:**
- Create: `src/slideio/imagetools/icctransform.hpp`, `src/slideio/imagetools/icctransform.cpp`
- Create: `src/tests/main/test_icctransform.cpp`
- Modify: `src/slideio/imagetools/conanfile.py`, `src/slideio/imagetools/CMakeLists.txt`, `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Consumes: `ColorProfile`, `ColorProfileInfo` from Task 1.
- Produces: `static ColorProfileInfo IccTransform::describe(const ColorProfile&)`; `static ColorProfile IccTransform::createSRGBProfile()`.

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_icctransform.cpp`:

```cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/icctransform.hpp"

using namespace slideio;

TEST(IccTransform, describeEmptyProfileReportsAbsent)
{
    const ColorProfileInfo info = IccTransform::describe(ColorProfile());
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
}

TEST(IccTransform, createSRGBProfileIsUsable)
{
    const ColorProfile profile = IccTransform::createSRGBProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::Assumed, profile.getSource());
}

TEST(IccTransform, describeSRGBProfileReportsRGBAndXYZ)
{
    const ColorProfile profile = IccTransform::createSRGBProfile();
    const ColorProfileInfo info = IccTransform::describe(profile);
    ASSERT_TRUE(info.present);
    ASSERT_EQ(IccColorSpace::RGB, info.dataSpace);
    ASSERT_EQ(IccColorSpace::XYZ, info.connectionSpace);
    ASSERT_EQ(profile.getSize(), info.dataSize);
    ASSERT_FALSE(info.description.empty());
    // D65 white point, roughly (0.9505, 1.0, 1.0890).
    ASSERT_NEAR(0.9505, info.whitePoint[0], 0.01);
    ASSERT_NEAR(1.0000, info.whitePoint[1], 0.01);
}

TEST(IccTransform, describeCarriesSourceFromTheProfile)
{
    // Provenance is known to whoever produced the bytes, not discoverable
    // from the bytes, so describe() must copy it rather than invent it.
    ColorProfile profile = IccTransform::createSRGBProfile();
    profile.setSource(ColorProfileSource::Embedded);
    ASSERT_EQ(ColorProfileSource::Embedded, IccTransform::describe(profile).source);
}

TEST(IccTransform, describeTruncatedProfileReportsAbsentRatherThanThrowing)
{
    // A corrupt profile must not kill a batch read; the caller's policy decides.
    const ColorProfile good = IccTransform::createSRGBProfile();
    std::vector<uint8_t> truncated(good.getData().begin(), good.getData().begin() + 40);
    ColorProfileInfo info;
    ASSERT_NO_THROW(info = IccTransform::describe(ColorProfile(truncated)));
    ASSERT_FALSE(info.present);
}
```

- [ ] **Step 2: Add lcms2 to the build, add the test, and run it to verify it fails**

In `src/slideio/imagetools/conanfile.py`, add to `requirements()`:

```python
        self.requires("lcms/2.16")
```

In `src/slideio/imagetools/CMakeLists.txt`, add `icctransform.hpp`/`icctransform.cpp` to the source list, then next to the other `find_package` calls:

```cmake
find_package(lcms REQUIRED)
```

and to the link line, **PRIVATE** so no consumer inherits the lcms2 headers:

```cmake
target_link_libraries(${LIBRARY_NAME} PRIVATE lcms::lcms)
```

Add `test_icctransform.cpp` to `set(TEST_SOURCES ...)` in `src/tests/main/CMakeLists.txt`.

Fetch the new package from conan center (the `-nr` flag used elsewhere cannot, since lcms is not yet in the local cache), for both profiles:

```bash
export PATH="/c/Users/Stanislav/anaconda3/envs/conan2/Scripts:$PATH"
for pr in x86_64_release x86_64_debug; do
  conan install -r conancenter -pr:b conan/Windows/$pr -pr:h conan/Windows/$pr \
    -of src/slideio/imagetools/cmake -g CMakeDeps -g CMakeToolchain -b missing \
    src/slideio/imagetools/conanfile.py
done
cmake -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=./cmake/conan_toolchain.cmake -S . -B build
cmake --build build --config Release --target slideio_tests -- -m
```

Expected: FAIL — `Cannot open include file: 'slideio/imagetools/icctransform.hpp'`.

- [ ] **Step 3: Write the header**

Create `src/slideio/imagetools/icctransform.hpp`. Note it exposes no lcms2 type: the handle is held by an opaque pointer so `lcms2.h` stays out of every consumer.

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <opencv2/core.hpp>
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/core/colorprofile.hpp"
#include "slideio/core/slideio_enums.hpp"

namespace slideio
{
    /**@brief ICC colour conversion of raster blocks.
     *
     * The only class in the project built against lcms2. The compiled transform
     * is created once in the constructor and never mutated, so apply() is const
     * and safe to call from several threads on one instance.
     *
     * Channel order is RGB, matching slideio block buffers, NOT OpenCV's BGR.*/
    class SLIDEIO_IMAGETOOLS_EXPORTS IccTransform
    {
    public:
        IccTransform(const ColorProfile& source, ColorTarget target,
                     RenderingIntent intent, bool blackPointCompensation,
                     DataType sourceType);
        ~IccTransform();
        IccTransform(const IccTransform&) = delete;
        IccTransform& operator=(const IccTransform&) = delete;

        void apply(const cv::Mat& src, cv::OutputArray dst) const;
        DataType getOutputDataType() const { return m_outputType; }

        /**@brief parses the ICC header. Returns present=false for empty or
         * unparseable bytes rather than throwing, so a corrupt profile does not
         * stop a batch read. The source field is copied from the profile, not
         * parsed: provenance is not discoverable from the bytes.*/
        static ColorProfileInfo describe(const ColorProfile& profile);

        /**@brief a synthetic sRGB profile, marked ColorProfileSource::Assumed.*/
        static ColorProfile createSRGBProfile();
    private:
        void* m_transform = nullptr;   // cmsHTRANSFORM
        DataType m_outputType = DataType::DT_Unknown;
        int m_targetChannels = 3;
    };
}
```

- [ ] **Step 4: Implement describe() and createSRGBProfile()**

Create `src/slideio/imagetools/icctransform.cpp` with the parsing half only; conversion arrives in Task 5.

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/icctransform.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include <lcms2.h>

using namespace slideio;

namespace
{
    IccColorSpace toIccColorSpace(cmsColorSpaceSignature sig)
    {
        switch (sig) {
        case cmsSigGrayData: return IccColorSpace::Gray;
        case cmsSigRgbData: return IccColorSpace::RGB;
        case cmsSigCmykData: return IccColorSpace::CMYK;
        case cmsSigLabData: return IccColorSpace::Lab;
        case cmsSigXYZData: return IccColorSpace::XYZ;
        case cmsSigYCbCrData: return IccColorSpace::YCbCr;
        default: return IccColorSpace::Unknown;
        }
    }

    RenderingIntent toRenderingIntent(cmsUInt32Number intent)
    {
        switch (intent) {
        case INTENT_PERCEPTUAL: return RenderingIntent::Perceptual;
        case INTENT_SATURATION: return RenderingIntent::Saturation;
        case INTENT_ABSOLUTE_COLORIMETRIC: return RenderingIntent::AbsoluteColorimetric;
        default: return RenderingIntent::RelativeColorimetric;
        }
    }

    std::string readProfileText(cmsHPROFILE handle, cmsInfoType type)
    {
        char buffer[512] = {0};
        const cmsUInt32Number size =
            cmsGetProfileInfoASCII(handle, type, "en", "US", buffer, sizeof(buffer) - 1);
        return size > 0 ? std::string(buffer) : std::string();
    }
}

ColorProfileInfo IccTransform::describe(const ColorProfile& profile)
{
    ColorProfileInfo info;
    info.source = profile.getSource();
    info.dataSize = profile.getSize();
    if (profile.isEmpty()) {
        return info;
    }
    cmsHPROFILE handle = cmsOpenProfileFromMem(profile.getData().data(),
                                               static_cast<cmsUInt32Number>(profile.getSize()));
    if (!handle) {
        SLIDEIO_LOG(WARNING) << "IccTransform: cannot parse an ICC profile of "
                             << profile.getSize() << " bytes; treating it as absent";
        return info;
    }
    info.present = true;
    info.description = readProfileText(handle, cmsInfoDescription);
    info.manufacturer = readProfileText(handle, cmsInfoManufacturer);
    info.model = readProfileText(handle, cmsInfoModel);
    info.dataSpace = toIccColorSpace(cmsGetColorSpace(handle));
    info.connectionSpace = toIccColorSpace(cmsGetPCS(handle));
    info.intent = toRenderingIntent(cmsGetHeaderRenderingIntent(handle));

    const cmsUInt32Number version = static_cast<cmsUInt32Number>(cmsGetProfileVersion(handle));
    info.version = std::to_string(version);

    if (const cmsCIEXYZ* wp = static_cast<const cmsCIEXYZ*>(
            cmsReadTag(handle, cmsSigMediaWhitePointTag))) {
        info.whitePoint = {wp->X, wp->Y, wp->Z};
    }
    cmsCloseProfile(handle);
    return info;
}

ColorProfile IccTransform::createSRGBProfile()
{
    cmsHPROFILE handle = cmsCreate_sRGBProfile();
    if (!handle) {
        RAISE_RUNTIME_ERROR << "IccTransform: lcms2 failed to create an sRGB profile";
    }
    cmsUInt32Number size = 0;
    if (!cmsSaveProfileToMem(handle, nullptr, &size) || size == 0) {
        cmsCloseProfile(handle);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot measure the synthetic sRGB profile";
    }
    std::vector<uint8_t> bytes(size);
    if (!cmsSaveProfileToMem(handle, bytes.data(), &size)) {
        cmsCloseProfile(handle);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot serialise the synthetic sRGB profile";
    }
    cmsCloseProfile(handle);
    ColorProfile profile(std::move(bytes));
    profile.setSource(ColorProfileSource::Assumed);
    return profile;
}

IccTransform::~IccTransform()
{
    if (m_transform) {
        cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    }
}
```

The constructor and `apply` are added in Task 5. To keep this task compiling, add a temporary constructor body that raises:

```cpp
IccTransform::IccTransform(const ColorProfile&, ColorTarget, RenderingIntent, bool, DataType)
{
    RAISE_RUNTIME_ERROR << "IccTransform: conversion is not implemented yet";
}

void IccTransform::apply(const cv::Mat&, cv::OutputArray) const
{
    RAISE_RUNTIME_ERROR << "IccTransform: conversion is not implemented yet";
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `./build/Release/slideio_tests.exe --gtest_filter="IccTransform.*"`
Expected: 5 tests PASS.

- [ ] **Step 6: Verify lcms2 has not leaked**

Run: `grep -rn "lcms2.h" src/ | grep -v "src/slideio/imagetools/icctransform.cpp"`
Expected: no output. If anything else includes it, the dependency boundary is broken — fix before committing.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/imagetools/icctransform.hpp src/slideio/imagetools/icctransform.cpp \
        src/slideio/imagetools/conanfile.py src/slideio/imagetools/CMakeLists.txt \
        src/tests/main/test_icctransform.cpp src/tests/main/CMakeLists.txt
git commit -m "add the lcms2-backed ICC header parser to imagetools

lcms/2.16 comes from conan center and is linked PRIVATE, so it is visible to
this one translation unit and to nothing else -- core, the transformer and all
eleven drivers stay engine-free. describe() reports present=false for corrupt
bytes instead of throwing, because a bad profile must not stop a batch read;
the caller's missing-profile policy decides what happens next."
```

---

### Task 4: Parsed profile info on Scene

**Files:**
- Modify: `src/slideio/slideio/scene.hpp`, `src/slideio/slideio/scene.cpp`
- Test: `src/tests/main/test_colorprofile.cpp`

**Interfaces:**
- Consumes: `Scene::getColorProfile()` (Task 2), `IccTransform::describe` (Task 3).
- Produces: `ColorProfileInfo Scene::getColorProfileInfo() const`.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_colorprofile.cpp`:

```cpp
TEST(ColorProfile, sceneWithoutProfileReportsInfoAbsent)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
    ASSERT_NE(std::string::npos, info.toString().find("present=false"));
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL — no member `getColorProfileInfo`.

- [ ] **Step 3: Implement it**

Declare in `src/slideio/slideio/scene.hpp` under `getColorProfile()`:

```cpp
        /**@brief returns the parsed header of the scene's ICC colour profile.
         *
         * present is false when the slide carries no profile, and also when the
         * profile it carries cannot be parsed. Use the source field to tell a
         * real correction from an assumed one.*/
        ColorProfileInfo getColorProfileInfo() const;
```

Implement in `src/slideio/slideio/scene.cpp`, adding `#include "slideio/imagetools/icctransform.hpp"`:

```cpp
ColorProfileInfo Scene::getColorProfileInfo() const
{
    return IccTransform::describe(m_scene->getColorProfile());
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `./build/Release/slideio_tests.exe --gtest_filter="ColorProfile.sceneWithoutProfileReportsInfoAbsent"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/slideio/scene.hpp src/slideio/slideio/scene.cpp \
        src/tests/main/test_colorprofile.cpp
git commit -m "report the parsed ICC header from Scene

slideio already links imagetools, so the public Scene can call describe()
without the engine reaching core or the drivers. Introspection is now complete
on its own: a pipeline can audit a corpus for profile coverage before any
colour transform exists."
```

---

### Task 5: ICC conversion for the four target spaces

**Files:**
- Modify: `src/slideio/imagetools/icctransform.cpp`
- Test: `src/tests/main/test_icctransform.cpp`

**Interfaces:**
- Consumes: Task 3's class shell.
- Produces: a working `IccTransform(source, target, intent, bpc, sourceType)` and `apply(src, dst)`; `getOutputDataType()` returning `DT_Byte`/`DT_UInt16` for `sRGB` and `DT_Float32` for `LinearRGB`/`Lab`/`XYZ`.

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_icctransform.cpp`:

```cpp
namespace {
    cv::Mat makeRgbPatch(const cv::Vec3b& colour)
    {
        cv::Mat patch(4, 4, CV_8UC3);
        patch.setTo(cv::Scalar(colour[0], colour[1], colour[2]));
        return patch;
    }
}

TEST(IccTransform, srgbToLabWhiteIsLightness100)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 255, 255}), out);
    ASSERT_EQ(CV_32FC3, out.type());
    const cv::Vec3f lab = out.at<cv::Vec3f>(0, 0);
    ASSERT_NEAR(100.0f, lab[0], 0.5f);
    ASSERT_NEAR(0.0f, lab[1], 1.0f);
    ASSERT_NEAR(0.0f, lab[2], 1.0f);
}

TEST(IccTransform, srgbToLabMidGreyIsLightness53)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat out;
    transform.apply(makeRgbPatch({128, 128, 128}), out);
    ASSERT_NEAR(53.6f, out.at<cv::Vec3f>(0, 0)[0], 1.0f);
}

TEST(IccTransform, channelOrderIsRgbNotBgr)
{
    // The regression that matters: a BGR mix-up produces output of the right
    // shape and dtype that looks entirely plausible. Pure red must stay red,
    // which in Lab means a strongly positive a* and a near-zero-to-positive b*.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 0, 0}), out);
    const cv::Vec3f lab = out.at<cv::Vec3f>(0, 0);
    ASSERT_NEAR(53.2f, lab[0], 1.5f);   // sRGB red
    ASSERT_GT(lab[1], 60.0f);           // a* strongly positive
    ASSERT_GT(lab[2], 40.0f);           // b* positive; blue would give a large negative
}

TEST(IccTransform, srgbToSrgbPreservesDataType)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::sRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Byte, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({10, 200, 90}), out);
    ASSERT_EQ(CV_8UC3, out.type());
    const cv::Vec3b rgb = out.at<cv::Vec3b>(0, 0);
    ASSERT_NEAR(10, rgb[0], 2);
    ASSERT_NEAR(200, rgb[1], 2);
    ASSERT_NEAR(90, rgb[2], 2);
}

TEST(IccTransform, linearRgbRemovesGamma)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::LinearRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({128, 128, 128}), out);
    // sRGB 128/255 is about 0.216 once linearised, not 0.502.
    ASSERT_NEAR(0.216f, out.at<cv::Vec3f>(0, 0)[0], 0.02f);
}

TEST(IccTransform, xyzTargetProducesFloat)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::XYZ,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 255, 255}), out);
    ASSERT_NEAR(1.0f, out.at<cv::Vec3f>(0, 0)[1], 0.02f);   // Y of white
}

TEST(IccTransform, rejectsNonThreeChannelInput)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat grey(4, 4, CV_8UC1, cv::Scalar(128));
    cv::Mat out;
    ASSERT_THROW(transform.apply(grey, out), slideio::RuntimeError);
}

TEST(IccTransform, applyIsSafeFromSeveralThreads)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&transform, &failures]() {
            for (int i = 0; i < 200; ++i) {
                cv::Mat out;
                transform.apply(makeRgbPatch({255, 255, 255}), out);
                if (std::abs(out.at<cv::Vec3f>(0, 0)[0] - 100.0f) > 0.5f) {
                    ++failures;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    ASSERT_EQ(0, failures.load());
}
```

Add `#include <thread>`, `#include <atomic>` and `#include "slideio/core/exceptions.hpp"` at the top of the file.

- [ ] **Step 2: Run them to verify they fail**

Run: `./build/Release/slideio_tests.exe --gtest_filter="IccTransform.*"`
Expected: the new tests FAIL with "conversion is not implemented yet"; the five from Task 3 still pass.

- [ ] **Step 3: Implement the constructor and apply()**

Replace the two temporary bodies in `src/slideio/imagetools/icctransform.cpp`:

```cpp
namespace
{
    cmsHPROFILE createTargetProfile(ColorTarget target)
    {
        switch (target) {
        case ColorTarget::sRGB:
            return cmsCreate_sRGBProfile();
        case ColorTarget::Lab:
            return cmsCreateLab4Profile(nullptr);
        case ColorTarget::XYZ:
            return cmsCreateXYZProfile();
        case ColorTarget::LinearRGB: {
            // sRGB primaries and white point with a gamma-1.0 tone curve.
            cmsCIExyY whitePoint{0.3127, 0.3290, 1.0};
            cmsCIExyYTRIPLE primaries{{0.6400, 0.3300, 1.0},
                                      {0.3000, 0.6000, 1.0},
                                      {0.1500, 0.0600, 1.0}};
            cmsToneCurve* linear = cmsBuildGamma(nullptr, 1.0);
            cmsToneCurve* curves[3] = {linear, linear, linear};
            cmsHPROFILE profile = cmsCreateRGBProfile(&whitePoint, &primaries, curves);
            cmsFreeToneCurve(linear);
            return profile;
        }
        default:
            return nullptr;
        }
    }

    cmsUInt32Number sourceFormat(DataType type)
    {
        // TYPE_RGB_*, never TYPE_BGR_*: slideio buffers are in scene channel
        // order, which is RGB. OpenCV's BGR convention does not apply here.
        switch (type) {
        case DataType::DT_Byte: return TYPE_RGB_8;
        case DataType::DT_UInt16: return TYPE_RGB_16;
        default: return 0;
        }
    }

    cmsUInt32Number targetFormat(ColorTarget target, DataType sourceType)
    {
        switch (target) {
        case ColorTarget::sRGB: return sourceFormat(sourceType);
        case ColorTarget::Lab: return TYPE_Lab_FLT;
        case ColorTarget::XYZ: return TYPE_XYZ_FLT;
        case ColorTarget::LinearRGB: return TYPE_RGB_FLT;
        default: return 0;
        }
    }

    cmsUInt32Number toLcmsIntent(RenderingIntent intent)
    {
        switch (intent) {
        case RenderingIntent::Perceptual: return INTENT_PERCEPTUAL;
        case RenderingIntent::Saturation: return INTENT_SATURATION;
        case RenderingIntent::AbsoluteColorimetric: return INTENT_ABSOLUTE_COLORIMETRIC;
        default: return INTENT_RELATIVE_COLORIMETRIC;
        }
    }
}

IccTransform::IccTransform(const ColorProfile& source, ColorTarget target,
                           RenderingIntent intent, bool blackPointCompensation,
                           DataType sourceType)
{
    const cmsUInt32Number srcFormat = sourceFormat(sourceType);
    if (srcFormat == 0) {
        RAISE_RUNTIME_ERROR << "IccTransform: unsupported source data type " << sourceType
                            << "; only DT_Byte and DT_UInt16 are colorimetric";
    }
    if (source.isEmpty()) {
        RAISE_RUNTIME_ERROR << "IccTransform: an empty source profile cannot be converted";
    }

    cmsHPROFILE srcProfile = cmsOpenProfileFromMem(
        source.getData().data(), static_cast<cmsUInt32Number>(source.getSize()));
    if (!srcProfile) {
        RAISE_RUNTIME_ERROR << "IccTransform: cannot parse the source ICC profile ("
                            << source.getSize() << " bytes)";
    }
    cmsHPROFILE dstProfile = createTargetProfile(target);
    if (!dstProfile) {
        cmsCloseProfile(srcProfile);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot create a profile for target " << target;
    }

    const cmsUInt32Number flags =
        blackPointCompensation ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0;
    m_transform = cmsCreateTransform(srcProfile, srcFormat, dstProfile,
                                     targetFormat(target, sourceType),
                                     toLcmsIntent(intent), flags);
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    if (!m_transform) {
        RAISE_RUNTIME_ERROR << "IccTransform: lcms2 could not build a transform to " << target;
    }
    m_outputType = (target == ColorTarget::sRGB) ? sourceType : DataType::DT_Float32;
}

void IccTransform::apply(const cv::Mat& src, cv::OutputArray dst) const
{
    if (src.channels() != 3) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected 3 channels, received " << src.channels();
    }
    if (!src.isContinuous()) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected a continuous block";
    }
    const int depth = (m_outputType == DataType::DT_Float32)
                          ? CV_32F
                          : ((m_outputType == DataType::DT_UInt16) ? CV_16U : CV_8U);
    dst.create(src.rows, src.cols, CV_MAKETYPE(depth, m_targetChannels));
    cv::Mat output = dst.getMat();
    cmsDoTransform(static_cast<cmsHTRANSFORM>(m_transform), src.data, output.data,
                   static_cast<cmsUInt32Number>(src.rows) * static_cast<cmsUInt32Number>(src.cols));
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/Release/slideio_tests.exe --gtest_filter="IccTransform.*"`
Expected: 13 tests PASS, including `channelOrderIsRgbNotBgr` and `applyIsSafeFromSeveralThreads`.

If `applyIsSafeFromSeveralThreads` fails or is flaky, lcms2's documented re-entrancy does not hold in this build. **Stop and report it** — the concurrency decision in Task 13 depends on it, and the spec names `ContextPool` holding one `cmsHTRANSFORM` per borrower as the fallback.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/imagetools/icctransform.cpp src/tests/main/test_icctransform.cpp
git commit -m "convert raster blocks through lcms2 into the four target spaces

sRGB preserves the source data type so a corrected read is a drop-in for an
uncorrected one; Lab, linear RGB and XYZ produce float32. Source formats are
TYPE_RGB_*, never TYPE_BGR_*, because slideio buffers are in scene channel
order -- a mix-up there yields output of the right shape and dtype that looks
plausible, so channelOrderIsRgbNotBgr pins it with an asymmetric patch.

applyIsSafeFromSeveralThreads checks lcms2's documented re-entrancy on a shared
handle rather than trusting it, because the transformer's concurrent-read
forwarding depends on that property holding."
```

---

### Task 6: Read the ICC tag from TIFF directories

**Files:**
- Modify: `src/slideio/imagetools/tifftools.hpp`, `src/slideio/imagetools/tifftools.cpp`
- Test: `src/tests/main/test_tifftools.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `std::vector<uint8_t> TiffDirectory::iccProfile`, populated by `TiffTools::scanTiffDirTags`.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_tifftools.cpp`:

```cpp
TEST(TiffTools, iccProfileIsEmptyWhenTheTagIsAbsent)
{
    // Most TIFF-family slides carry no ICC tag; the field must stay empty
    // rather than holding stale bytes from a previous directory.
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::vector<slideio::TiffDirectory> directories;
    slideio::TiffTools::scanFile(path, directories);
    ASSERT_FALSE(directories.empty());
    for (const auto& directory : directories) {
        ASSERT_EQ(directory.iccProfile.size() == 0, directory.iccProfile.empty());
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL — `TiffDirectory` has no member `iccProfile`.

- [ ] **Step 3: Add the field and read the tag**

In `src/slideio/imagetools/tifftools.hpp`, add to `struct TiffDirectory`, after `description`/`software`:

```cpp
        /**@brief raw ICC profile bytes from TIFFTAG_ICCPROFILE (34675). Empty
         * when the directory carries no profile.*/
        std::vector<uint8_t> iccProfile;
```

In `src/slideio/imagetools/tifftools.cpp`, inside `TiffTools::scanTiffDirTags` next to the other `TIFFGetField` calls:

```cpp
    uint32_t iccSize = 0;
    void* iccData = nullptr;
    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccSize, &iccData) && iccData && iccSize > 0) {
        const uint8_t* bytes = static_cast<const uint8_t*>(iccData);
        dir.iccProfile.assign(bytes, bytes + iccSize);
    }
    else {
        dir.iccProfile.clear();
    }
```

The `else` branch matters: `scanTiffDirTags` is called repeatedly against one `TiffDirectory` in some paths, and leaving a stale profile behind would attribute one directory's colorimetry to another.

- [ ] **Step 4: Run the test to verify it passes**

Run: `./build/Release/slideio_tests.exe --gtest_filter="TiffTools.iccProfile*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/imagetools/tifftools.hpp src/slideio/imagetools/tifftools.cpp \
        src/tests/main/test_tifftools.cpp
git commit -m "read TIFFTAG_ICCPROFILE into TiffDirectory

One site serves five driver libraries: svs (and with it phtiff), scn, pke,
ome-tiff and vsi all scan directories through TiffTools. The field is cleared
when the tag is absent, because scanTiffDirTags is called repeatedly against
one directory on some paths and a stale profile would attribute one
directory's colorimetry to another."
```

---

### Task 7: Expose the profile from the TIFF-family drivers

**Files:**
- Modify: `src/slideio/drivers/svs/svsscene.hpp`, `src/slideio/drivers/svs/svsslide.cpp`, and the equivalent scene/slide pairs in `scn/`, `pke/`, `ome-tiff/`, `vsi/`
- Test: `src/tests/main/test_svs_driver.cpp`, `src/tests/main/test_scn_driver.cpp`, `src/tests/ometiff/test_ometiff_driver.cpp`

**Interfaces:**
- Consumes: `TiffDirectory::iccProfile` (Task 6), `CVScene::getColorProfile` (Task 2).
- Produces: `SVSScene::setColorProfile(const ColorProfile&)` and a `getColorProfile()` override; the same pair on the scn, pke, ome-tiff and vsi scene classes.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_svs_driver.cpp`:

```cpp
TEST(SVSImageDriver, colorProfileMatchesTheTiffTag)
{
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "SVS");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);

    std::vector<slideio::TiffDirectory> directories;
    slideio::TiffTools::scanFile(path, directories);

    // Whatever the file holds, the scene must report exactly that: bytes when
    // the tag is present, absence when it is not. No invention either way.
    const slideio::ColorProfile profile = scene->getColorProfile();
    ASSERT_EQ(directories[0].iccProfile.size(), profile.getSize());
    if (!directories[0].iccProfile.empty()) {
        ASSERT_EQ(directories[0].iccProfile, profile.getData());
        ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
        ASSERT_TRUE(scene->getColorProfileInfo().present);
    }
    else {
        ASSERT_TRUE(profile.isEmpty());
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m` then
`./build/Release/slideio_tests.exe --gtest_filter="SVSImageDriver.colorProfileMatchesTheTiffTag"`
Expected: FAIL — the scene reports an empty profile even when the directory holds bytes. (If `CMU-1-Small-Region.svs` has no ICC tag the test passes trivially; still implement the steps below, and rely on the ome-tiff and scn cases plus Task 9's DICOM test for positive coverage.)

- [ ] **Step 3: Add storage and the override to SVSScene**

In `src/slideio/drivers/svs/svsscene.hpp`, add `#include "slideio/core/colorprofile.hpp"`, then in the `protected` member block add `ColorProfile m_colorProfile;` and in the public section:

```cpp
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
        }
```

Placing it on `SVSScene` rather than on the concrete scenes means `SVSTiledScene`, `SVSSmallScene` and the PHTIFF scenes derived from them all inherit it.

- [ ] **Step 4: Populate it where scenes are constructed**

In `src/slideio/drivers/svs/svsslide.cpp`, find the scene construction sites (`grep -n "make_shared<SVSTiledScene>\|make_shared<SVSSmallScene>" src/slideio/drivers/svs/svsslide.cpp`) and after each construction, set the profile from the directory backing that scene:

```cpp
    scene->setColorProfile(ColorProfile(dir.iccProfile));
```

Repeat the same two steps for the scn, pke, ome-tiff and vsi scene/slide pairs, using each driver's own scene base class and its own directory variable.

- [ ] **Step 5: Run the driver tests to verify they pass**

Run:
```bash
./build/Release/slideio_tests.exe --gtest_filter="SVSImageDriver.colorProfile*:SCNImageDriver.colorProfile*"
./build/Release/slideio_ometiff_tests.exe --gtest_filter="*colorProfile*"
```
Expected: PASS or SKIPPED.

- [ ] **Step 6: Verify AFI inherits it without code**

Run: `./build/Release/slideio_tests.exe --gtest_filter="AFIImageDriver.*"`
Expected: PASS. AFI delegates to SVS scenes, so it gains the profile with no change of its own; confirm nothing regressed.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/svs src/slideio/drivers/scn src/slideio/drivers/pke \
        src/slideio/drivers/ome-tiff src/slideio/drivers/vsi \
        src/tests/main/test_svs_driver.cpp src/tests/main/test_scn_driver.cpp \
        src/tests/ometiff/test_ometiff_driver.cpp
git commit -m "carry the TIFF ICC profile through to the scenes

Storage sits on each driver's scene base class, so SVSTiledScene, SVSSmallScene
and the PHTIFF scenes derived from them all inherit it, and AFI gains the
profile with no code of its own because it delegates to SVS scenes. The tests
assert the scene reports exactly what the directory holds -- bytes when the tag
is present, absence when it is not -- rather than asserting a particular file
carries a profile, which would make them hostage to the test corpus."
```

---

### Task 8: NDPI profile extraction

**Files:**
- Modify: `src/slideio/drivers/ndpi/ndpitifftools.hpp`, `src/slideio/drivers/ndpi/ndpitifftools.cpp`, `src/slideio/drivers/ndpi/ndpiscene.hpp`, `src/slideio/drivers/ndpi/ndpislide.cpp`
- Test: `src/tests/ndpi/test_ndpi_driver.cpp`

**Interfaces:**
- Consumes: `CVScene::getColorProfile` (Task 2).
- Produces: `std::vector<uint8_t> NDPITiffDirectory::iccProfile`; `NDPIScene::getColorProfile()` override.

The ndpi driver keeps its own `NDPITiffDirectory` and `NDPITiffTools::scanTiffDirTags` against the `extern/ndpi-tiff` fork, so Task 6's change does not reach it.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/ndpi/test_ndpi_driver.cpp`:

```cpp
TEST(NDPIImageDriver, colorProfileMatchesTheTiffTag)
{
    std::string path = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "NDPI");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    if (!profile.isEmpty()) {
        ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
        ASSERT_TRUE(scene->getColorProfileInfo().present);
    }
    else {
        ASSERT_EQ(slideio::ColorProfileSource::None, profile.getSource());
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_ndpi_tests -- -m` then
`./build/Release/slideio_ndpi_tests.exe --gtest_filter="NDPIImageDriver.colorProfile*"`
Expected: FAIL to compile until the scene override exists.

- [ ] **Step 3: Add the field and read the tag**

In `src/slideio/drivers/ndpi/ndpitifftools.hpp`, add to `struct NDPITiffDirectory`:

```cpp
        /**@brief raw ICC profile bytes from TIFFTAG_ICCPROFILE (34675).*/
        std::vector<uint8_t> iccProfile;
```

In `NDPITiffTools::scanTiffDirTags` in `ndpitifftools.cpp`, add the same block as Task 6, including the `else` that clears the field.

- [ ] **Step 4: Add storage and the override to NDPIScene**

In `ndpiscene.hpp`, add `#include "slideio/core/colorprofile.hpp"`, a `ColorProfile m_colorProfile;` member, and:

```cpp
        ColorProfile getColorProfile() const override {
            return m_colorProfile;
        }
        void setColorProfile(const ColorProfile& profile) {
            m_colorProfile = profile;
        }
```

Populate it in `ndpislide.cpp` where scenes are constructed (`grep -n "make_shared<NDPIScene>" src/slideio/drivers/ndpi/ndpislide.cpp`), from the directory backing the scene.

- [ ] **Step 5: Run the test to verify it passes**

Run: `./build/Release/slideio_ndpi_tests.exe --gtest_filter="NDPIImageDriver.colorProfile*"`
Expected: PASS or SKIPPED.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/ndpi src/tests/ndpi/test_ndpi_driver.cpp
git commit -m "read the ICC tag in the ndpi driver

The ndpi driver scans directories with its own NDPITiffTools against the
extern/ndpi-tiff fork rather than through the shared TiffTools, so the change
that served the other five TIFF-family drivers does not reach it and has to be
made a second time here."
```

---

### Task 9: DICOM profile extraction

**Files:**
- Modify: `src/slideio/drivers/dcm/dcmfile.hpp`, `src/slideio/drivers/dcm/dcmfile.cpp`, `src/slideio/drivers/dcm/dcmscene.hpp`, `src/slideio/drivers/dcm/dcmslide.cpp`
- Test: `src/tests/main/test_dcm_driver.cpp`

**Interfaces:**
- Consumes: `CVScene::getColorProfile` (Task 2).
- Produces: `ColorProfile DCMFile::readColorProfile() const`; `DCMScene::getColorProfile()` override.

DICOM WSI is the only supported format that specifies ICC placement normatively — `ICC Profile (0028,2000)` inside the `Optical Path Sequence (0048,0105)` — so it is the reference case for positive coverage.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_dcm_driver.cpp`:

```cpp
TEST(DCMImageDriver, colorProfileFromOpticalPathSequence)
{
    std::string path = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "DCM");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    if (!profile.isEmpty()) {
        ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
        const slideio::ColorProfileInfo info = scene->getColorProfileInfo();
        ASSERT_TRUE(info.present);
        ASSERT_EQ(profile.getSize(), info.dataSize);
    }
    else {
        ASSERT_EQ(slideio::ColorProfileSource::None, profile.getSource());
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL to compile or report an empty profile.

- [ ] **Step 3: Read the tag in DCMFile**

Declare in `dcmfile.hpp`:

```cpp
        /**@brief reads ICC Profile (0028,2000).
         *
         * Looked for first inside Optical Path Sequence (0048,0105), where
         * DICOM WSI places it, then at dataset level as a fallback for
         * non-WSI objects that carry it directly.*/
        ColorProfile readColorProfile() const;
```

Implement in `dcmfile.cpp`, following the existing `getValidDataset()` idiom:

```cpp
ColorProfile DCMFile::readColorProfile() const
{
    DcmDataset* dataset = getValidDataset();
    const Uint8* bytes = nullptr;
    unsigned long count = 0;

    DcmItem* opticalPath = nullptr;
    if (dataset->findAndGetSequenceItem(DCM_OpticalPathSequence, opticalPath, 0).good()
        && opticalPath != nullptr) {
        opticalPath->findAndGetUint8Array(DCM_ICCProfile, bytes, &count);
    }
    if (bytes == nullptr || count == 0) {
        dataset->findAndGetUint8Array(DCM_ICCProfile, bytes, &count);
    }
    if (bytes == nullptr || count == 0) {
        return ColorProfile();
    }
    return ColorProfile(std::vector<uint8_t>(bytes, bytes + count));
}
```

- [ ] **Step 4: Surface it on the scene**

Add `ColorProfile m_colorProfile;`, a `getColorProfile()` override and a `setColorProfile()` setter to `dcmscene.hpp` exactly as in Task 8, and populate it in `dcmslide.cpp` from `file->readColorProfile()` where scenes are constructed.

- [ ] **Step 5: Run the test to verify it passes**

Run: `./build/Release/slideio_tests.exe --gtest_filter="DCMImageDriver.colorProfile*"`
Expected: PASS or SKIPPED.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/dcm src/tests/main/test_dcm_driver.cpp
git commit -m "read ICC Profile (0028,2000) in the dcm driver

DICOM WSI puts the profile inside Optical Path Sequence (0048,0105), which is
checked first; dataset level is a fallback for non-WSI objects that carry the
tag directly. DICOM is the only supported format that specifies ICC placement
normatively, so it is the reference case for extraction."
```

---

### Task 10: GDAL profile extraction

**Files:**
- Modify: `src/slideio/drivers/gdal/gdalscene.hpp`, `src/slideio/drivers/gdal/gdalscene.cpp`
- Test: `src/tests/main/test_gdal_driver.cpp`

**Interfaces:**
- Consumes: `CVScene::getColorProfile` (Task 2).
- Produces: `GDALScene::getColorProfile()` override.

GDAL exposes the profile base64-encoded as metadata item `SOURCE_ICC_PROFILE` in the `COLOR_PROFILE` domain.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_gdal_driver.cpp`:

```cpp
TEST(GDALImageDriver, colorProfileAbsentForPlainPng)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene->getColorProfile().isEmpty());
}

TEST(GDALImageDriver, colorProfileDecodedFromBase64WhenPresent)
{
    // A JPEG written with an embedded profile; skipped when the corpus lacks it.
    std::string path = TestTools::getTestImagePath("gdal", "icc/srgb-tagged.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
    const slideio::ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_TRUE(info.present);
    ASSERT_EQ(slideio::IccColorSpace::RGB, info.dataSpace);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --config Release --target slideio_tests -- -m`
Expected: FAIL to compile until the override exists.

- [ ] **Step 3: Implement the override**

In `gdalscene.hpp`, add `#include "slideio/core/colorprofile.hpp"` and declare `ColorProfile getColorProfile() const override;`.

In `gdalscene.cpp`, using GDAL's own base64 decoder so no new dependency is introduced:

```cpp
ColorProfile GDALScene::getColorProfile() const
{
    if (!m_hFile) {
        return ColorProfile();
    }
    const char* encoded = GDALGetMetadataItem(m_hFile, "SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    if (encoded == nullptr || *encoded == '\0') {
        return ColorProfile();
    }
    // GDAL stores the profile base64-encoded in this domain.
    std::vector<uint8_t> decoded(strlen(encoded));   // decoded is always shorter
    const int size = CPLBase64DecodeInPlace(reinterpret_cast<GByte*>(
        memcpy(decoded.data(), encoded, decoded.size())));
    if (size <= 0) {
        SLIDEIO_LOG(WARNING) << "GDALScene: SOURCE_ICC_PROFILE is not valid base64; ignoring it";
        return ColorProfile();
    }
    decoded.resize(size);
    return ColorProfile(std::move(decoded));
}
```

Add `#include "cpl_string.h"` and `#include "slideio/core/log.hpp"` if not already present.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/Release/slideio_tests.exe --gtest_filter="GDALImageDriver.colorProfile*"`
Expected: the first PASSES; the second PASSES or is SKIPPED if the corpus lacks the image.

- [ ] **Step 5: Record the new test image if one was added**

If you added `gdal/icc/srgb-tagged.jpg` to the corpus, regenerate the inventory:

Run: `python3 auxfiles/list-test-images.py`
Expected: `software-docs/TEST_IMAGES.md` now lists the new file.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/gdal src/tests/main/test_gdal_driver.cpp software-docs/TEST_IMAGES.md
git commit -m "decode the ICC profile GDAL exposes as base64

GDAL publishes SOURCE_ICC_PROFILE base64-encoded in the COLOR_PROFILE metadata
domain. Decoded with CPLBase64DecodeInPlace so the driver gains no dependency
it does not already have. Invalid base64 is logged and treated as absent
rather than raised, matching how a corrupt profile is handled elsewhere."
```

---

### Task 11: Source binding hooks on TransformationEx

**Files:**
- Modify: `src/slideio/transformer/transformationex.hpp`, `src/slideio/transformer/transformerscene.hpp`, `src/slideio/transformer/transformerscene.cpp`, `src/slideio/transformer/CMakeLists.txt`
- Create: `src/tests/transformer/test_colormanagement.cpp`
- Modify: `src/tests/transformer/CMakeLists.txt`

**Interfaces:**
- Consumes: `ColorProfile` (Task 1), `CVScene::getColorProfile` (Task 2).
- Produces: `virtual std::shared_ptr<TransformationEx> TransformationEx::bindToSource(const CVScene&) const` (default `nullptr`); `virtual ColorProfile TransformationEx::amendColorProfile(const ColorProfile&) const` (default: returns its input); `TransformerScene::getColorProfile()` override.

- [ ] **Step 1: Write the failing test**

Create `src/tests/transformer/test_colormanagement.cpp`:

```cpp
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"

using namespace slideio;

TEST(TransformationBinding, existingFiltersNeedNoBinding)
{
    // The seven existing filters must be entirely unaffected by the new hooks.
    GaussianBlurFilter filter;
    ASSERT_EQ(nullptr, filter.bindToSource(*(CVScene*)nullptr).get());
}

TEST(TransformationBinding, existingFiltersLeaveTheProfileAlone)
{
    GaussianBlurFilter filter;
    ColorProfile profile(std::vector<uint8_t>{1, 2, 3});
    const ColorProfile amended = filter.amendColorProfile(profile);
    ASSERT_EQ(profile.getData(), amended.getData());
    ASSERT_EQ(profile.getSource(), amended.getSource());
}

TEST(TransformationBinding, transformedSceneForwardsTheOriginProfile)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<Scene> originScene = slide->getScene(0);
    GaussianBlurFilter filter;
    std::shared_ptr<Scene> transformed = transformScene(originScene, filter);
    // A filter that does not touch colour must not change what the scene
    // reports about its colorimetry.
    ASSERT_EQ(originScene->getColorProfile().getSize(),
              transformed->getColorProfile().getSize());
}
```

`bindToSource` is called with a null reference in the first test only to prove the default ignores its argument; that is safe because the default body never dereferences it. Note that in the header the default must therefore not touch `source`.

- [ ] **Step 2: Add the test to the build and run it to verify it fails**

Add `test_colormanagement.cpp` to `set(TEST_SOURCES ...)` in `src/tests/transformer/CMakeLists.txt`.

Run: `cmake --build build --config Release --target slideio_transformer_tests -- -m`
Expected: FAIL — no member `bindToSource`.

- [ ] **Step 3: Add the two virtuals**

In `src/slideio/transformer/transformationex.hpp`, add `#include "slideio/core/colorprofile.hpp"` and a forward declaration `class CVScene;`, then inside the class:

```cpp
        /**@brief returns a copy of this transformation specialised to a source scene.
         *
         * The default returns nullptr, meaning the transformation needs no
         * binding and is used as-is; it does not touch its argument. A
         * transformation whose behaviour depends on the source image -- colour
         * management on the source ICC profile, stain normalisation on source
         * statistics -- overrides it and returns a new, fully prepared object.
         *
         * A bound copy rather than mutation of this, so one configuration object
         * stays reusable across many scenes instead of becoming last-bind-wins.*/
        virtual std::shared_ptr<TransformationEx> bindToSource(const CVScene& source) const {
            return nullptr;
        }

        /**@brief lets a bound transformation amend the colour profile its scene reports.
         *
         * The default returns the input unchanged. Colour management overrides
         * it to record that it substituted an assumed sRGB profile, so a caller
         * can tell a real correction from an assumed one without knowing which
         * transformation performed it.*/
        virtual ColorProfile amendColorProfile(const ColorProfile& input) const {
            return input;
        }
```

- [ ] **Step 4: Bind in TransformerScene and forward the profile**

In `src/slideio/transformer/transformerscene.cpp`, in the constructor, replace the direct member initialisation of `m_transformations` with a bound list built before `initChannels()` runs:

```cpp
TransformerScene::TransformerScene(std::shared_ptr<CVScene> originScene,
                                   const std::list<std::shared_ptr<Transformation>>& list)
    : m_originScene(originScene), m_inflationValue(0)
{
    // Bind before initChannels(): a bound transformation may report different
    // channel data types from its unbound configuration.
    for (const auto& transformation : list) {
        TransformationEx* transformationEx =
            dynamic_cast<TransformationEx*>(transformation.get());
        std::shared_ptr<TransformationEx> bound =
            transformationEx ? transformationEx->bindToSource(*originScene) : nullptr;
        m_transformations.push_back(bound ? bound : transformation);
    }
    initChannels();
    computeInflationValue();
}
```

Keep whatever the existing constructor body did after member initialisation; only the list construction and its ordering relative to `initChannels()` change.

Add to `transformerscene.hpp`:

```cpp
        ColorProfile getColorProfile() const override;
```

and implement:

```cpp
ColorProfile TransformerScene::getColorProfile() const
{
    ColorProfile profile = m_originScene->getColorProfile();
    for (const auto& transformation : m_transformations) {
        if (TransformationEx* transformationEx =
                dynamic_cast<TransformationEx*>(transformation.get())) {
            profile = transformationEx->amendColorProfile(profile);
        }
    }
    return profile;
}
```

Add `${IMAGETOOLS_LIB_NAME}` to the transformer's `target_link_libraries` in `src/slideio/transformer/CMakeLists.txt`, next to `${CORE_LIB_NAME}` and `${SLIDEIO_LIB_NAME}`, rather than relying on transitivity.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `./build/Release/slideio_transformer_tests.exe --gtest_filter="TransformationBinding.*"`
Expected: 3 tests PASS.

- [ ] **Step 6: Run the whole transformer suite to check nothing regressed**

Run: `./build/Release/slideio_transformer_tests.exe`
Expected: all pre-existing tests still PASS. The binding loop now runs for every transform, so a regression here would show up as a filter behaving differently.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/transformer/transformationex.hpp \
        src/slideio/transformer/transformerscene.hpp \
        src/slideio/transformer/transformerscene.cpp \
        src/slideio/transformer/CMakeLists.txt \
        src/tests/transformer/test_colormanagement.cpp src/tests/transformer/CMakeLists.txt
git commit -m "let a transformation bind to its source scene

applyTransformation receives only the pixel block and never the scene, so no
transformation could learn the slide's ICC profile. bindToSource closes that,
returning a bound copy rather than mutating this, so one configuration object
can be passed to transformScene in a loop over many slides and each scene gets
its own bound state instead of the last binding winning. Stain normalisation
will need the same hook to fit reference statistics once.

amendColorProfile is its read-side twin: TransformerScene folds the origin's
profile through the bound transformations, so a colour-managed scene can report
that it substituted an assumed profile without TransformerScene knowing which
transformation did it. Both defaults leave the seven existing filters untouched.

Binding runs before initChannels() because a bound transformation may report
different channel data types from its unbound configuration."
```

---

### Task 12: The ColorManagement transformation

**Files:**
- Create: `src/slideio/transformer/colormanagement.hpp`, `src/slideio/transformer/colormanagement.cpp`, `src/slideio/transformer/colormanagementwrap.hpp`, `src/slideio/transformer/colormanagementwrap.cpp`
- Modify: `src/slideio/transformer/transformationtype.hpp`, `src/slideio/transformer/transformationtype.cpp`, `src/slideio/transformer/wrappers.hpp`, `src/slideio/transformer/transformations.cpp`, `src/slideio/transformer/CMakeLists.txt`
- Test: `src/tests/transformer/test_colormanagement.cpp`

**Interfaces:**
- Consumes: `IccTransform` (Tasks 3, 5), `bindToSource`/`amendColorProfile` (Task 11).
- Produces: `enum class MissingProfilePolicy { AssumeSRGB, PassThrough, Fail }`; class `ColorManagement` with `getTarget/setTarget`, `getIntent/setIntent`, `getBlackPointCompensation/setBlackPointCompensation`, `getMissingProfilePolicy/setMissingProfilePolicy`, `getSourceProfileOverride/setSourceProfileOverride`; class `ColorManagementWrap` mirroring `ColorTransformationWrap`; `TransformationType::ColorManagement`.

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/transformer/test_colormanagement.cpp`:

```cpp
#include "slideio/transformer/colormanagement.hpp"

namespace {
    std::shared_ptr<Scene> openRgbScene()
    {
        std::string path = TestTools::getTestImagePath("gdal", "colors.png");
        return openSlide(path, "AUTO")->getScene(0);
    }
}

TEST(ColorManagement, defaultsAreTheSafeCase)
{
    ColorManagement cm;
    ASSERT_EQ(ColorTarget::sRGB, cm.getTarget());
    ASSERT_EQ(MissingProfilePolicy::AssumeSRGB, cm.getMissingProfilePolicy());
    ASSERT_EQ(RenderingIntent::RelativeColorimetric, cm.getIntent());
    ASSERT_TRUE(cm.getBlackPointCompensation());
    ASSERT_EQ(TransformationType::ColorManagement, cm.getType());
}

TEST(ColorManagement, labTargetDeclaresFloat32Channels)
{
    ColorManagement cm(ColorTarget::Lab);
    const std::vector<DataType> in{DataType::DT_Byte, DataType::DT_Byte, DataType::DT_Byte};
    const std::vector<DataType> out = cm.computeChannelDataTypes(in);
    ASSERT_EQ(3u, out.size());
    for (DataType type : out) {
        ASSERT_EQ(DataType::DT_Float32, type);
    }
}

TEST(ColorManagement, srgbTargetPreservesChannelDataTypes)
{
    ColorManagement cm(ColorTarget::sRGB);
    const std::vector<DataType> in{DataType::DT_Byte, DataType::DT_Byte, DataType::DT_Byte};
    ASSERT_EQ(in, cm.computeChannelDataTypes(in));
}

TEST(ColorManagement, unprofiledSceneIsReportedAsAssumed)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openRgbScene();
    ASSERT_TRUE(scene->getColorProfile().isEmpty());

    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    const ColorProfileInfo info = managed->getColorProfileInfo();
    ASSERT_EQ(ColorProfileSource::Assumed, info.source);
}

TEST(ColorManagement, failPolicyThrowsAtBindTimeWithoutAProfile)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::Fail);
    // Before any pixel moves, so a batch job dies on the file it cannot handle
    // rather than several thousand tiles later.
    ASSERT_THROW(transformScene(scene, cm), RuntimeError);
}

TEST(ColorManagement, passThroughIsRejectedForNonSrgbTargets)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::PassThrough);
    // Otherwise output dtype would depend on whether a file happened to carry a
    // profile, and a batch would assemble tensors of two different dtypes.
    ASSERT_THROW(transformScene(scene, cm), RuntimeError);
}

TEST(ColorManagement, sourceProfileOverrideIsUsed)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::Fail);
    ColorProfile measured = IccTransform::createSRGBProfile();
    measured.setSource(ColorProfileSource::Embedded);
    cm.setSourceProfileOverride(measured);
    // Fail policy no longer applies: an explicit profile was supplied.
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    ASSERT_EQ(ColorProfileSource::Embedded, managed->getColorProfileInfo().source);
}

TEST(ColorManagement, oneConfigObjectBindsIndependentlyToTwoScenes)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> first = transformScene(openRgbScene(), cm);
    std::shared_ptr<Scene> second = transformScene(openRgbScene(), cm);
    // The second binding must not have disturbed the first.
    ASSERT_EQ(ColorProfileSource::Assumed, first->getColorProfileInfo().source);
    ASSERT_EQ(ColorProfileSource::Assumed, second->getColorProfileInfo().source);
    ASSERT_EQ(DataType::DT_Float32, first->getChannelDataType(0));
}

TEST(ColorManagement, labBlockIsFloatAndPlausible)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> managed = transformScene(openRgbScene(), *std::make_shared<ColorManagement>(ColorTarget::Lab));
    auto rect = managed->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    std::vector<float> buffer(static_cast<size_t>(width) * height * 3);
    managed->readBlock(rect, buffer.data(), buffer.size() * sizeof(float));
    for (size_t i = 0; i < buffer.size(); i += 3) {
        ASSERT_GE(buffer[i], -0.5f);      // L in [0,100]
        ASSERT_LE(buffer[i], 100.5f);
    }
}
```

- [ ] **Step 2: Run them to verify they fail**

Run: `cmake --build build --config Release --target slideio_transformer_tests -- -m`
Expected: FAIL — `colormanagement.hpp` does not exist.

- [ ] **Step 3: Write the header**

Create `src/slideio/transformer/colormanagement.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/core/colorprofile.hpp"

namespace slideio
{
    class IccTransform;

    /**@brief what to do for a slide that embeds no ICC profile*/
    enum class MissingProfilePolicy
    {
        /**@brief treat the source as sRGB. Reads always succeed; the scene
         * reports ColorProfileSource::Assumed so absence stays visible.*/
        AssumeSRGB,
        /**@brief return decoded pixels untouched. Valid only for target sRGB.*/
        PassThrough,
        /**@brief throw at bind time. For pipelines that require real colorimetry.*/
        Fail,
    };

    /**@brief converts scene blocks into a device-independent colour space.
     *
     * Binds only to colorimetric RGB scenes: three channels of DT_Byte or
     * DT_UInt16, and an embedded profile whose data space is RGB. Anything else
     * throws at bind time rather than producing numbers that cannot mean
     * anything -- fluorescence channel intensities are not colorimetric.*/
    class SLIDEIO_TRANSFORMER_EXPORTS ColorManagement : public TransformationEx
    {
    public:
        ColorManagement();
        explicit ColorManagement(ColorTarget target);
        ColorManagement(const ColorManagement& other) = default;
        ColorManagement& operator=(const ColorManagement& other) = default;

        ColorTarget getTarget() const { return m_target; }
        void setTarget(ColorTarget target) { m_target = target; }
        RenderingIntent getIntent() const { return m_intent; }
        void setIntent(RenderingIntent intent) { m_intent = intent; }
        bool getBlackPointCompensation() const { return m_blackPointCompensation; }
        void setBlackPointCompensation(bool value) { m_blackPointCompensation = value; }
        MissingProfilePolicy getMissingProfilePolicy() const { return m_policy; }
        void setMissingProfilePolicy(MissingProfilePolicy policy) { m_policy = policy; }
        const ColorProfile& getSourceProfileOverride() const { return m_sourceOverride; }
        void setSourceProfileOverride(const ColorProfile& profile) { m_sourceOverride = profile; }

        std::shared_ptr<TransformationEx> bindToSource(const CVScene& source) const override;
        ColorProfile amendColorProfile(const ColorProfile& input) const override;
        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;
    private:
        ColorTarget m_target = ColorTarget::sRGB;
        RenderingIntent m_intent = RenderingIntent::RelativeColorimetric;
        bool m_blackPointCompensation = true;
        MissingProfilePolicy m_policy = MissingProfilePolicy::AssumeSRGB;
        ColorProfile m_sourceOverride;
        // Set only on a bound copy. Shared rather than unique so the class stays
        // copyable, and immutable after binding so apply() is const and re-entrant.
        std::shared_ptr<const IccTransform> m_transform;
        ColorProfile m_boundSource;
        bool m_passThrough = false;
    };
}
```

- [ ] **Step 4: Write the implementation**

Create `src/slideio/transformer/colormanagement.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/colormanagement.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/imagetools/icctransform.hpp"

using namespace slideio;

ColorManagement::ColorManagement()
{
    m_type = TransformationType::ColorManagement;
}

ColorManagement::ColorManagement(ColorTarget target) : ColorManagement()
{
    m_target = target;
}

std::shared_ptr<TransformationEx> ColorManagement::bindToSource(const CVScene& source) const
{
    auto bound = std::make_shared<ColorManagement>(*this);

    if (source.getNumChannels() != 3) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected a 3 channel RGB scene, found "
                            << source.getNumChannels()
                            << " channels. ICC conversion of non-colorimetric channels is"
                               " not meaningful.";
    }
    const DataType dataType = source.getChannelDataType(0);
    if (dataType != DataType::DT_Byte && dataType != DataType::DT_UInt16) {
        RAISE_RUNTIME_ERROR << "ColorManagement: expected DT_Byte or DT_UInt16 channels, found "
                            << dataType;
    }

    ColorProfile sourceProfile = m_sourceOverride.isEmpty() ? source.getColorProfile()
                                                            : m_sourceOverride;
    if (!sourceProfile.isEmpty()) {
        const ColorProfileInfo info = IccTransform::describe(sourceProfile);
        if (!info.present) {
            // Corrupt bytes are treated as absence; the policy below decides.
            sourceProfile = ColorProfile();
        }
        else if (info.dataSpace != IccColorSpace::RGB) {
            RAISE_RUNTIME_ERROR << "ColorManagement: the embedded profile describes "
                                << info.dataSpace << " data, not RGB";
        }
    }

    if (sourceProfile.isEmpty()) {
        switch (m_policy) {
        case MissingProfilePolicy::Fail:
            RAISE_RUNTIME_ERROR << "ColorManagement: the scene embeds no ICC profile and the"
                                   " missing profile policy is Fail";
        case MissingProfilePolicy::PassThrough:
            if (m_target != ColorTarget::sRGB) {
                RAISE_RUNTIME_ERROR << "ColorManagement: PassThrough is only coherent with the"
                                       " sRGB target; with " << m_target
                                    << " the output data type would depend on whether a file"
                                       " happens to carry a profile";
            }
            bound->m_passThrough = true;
            bound->m_boundSource = ColorProfile();
            return bound;
        case MissingProfilePolicy::AssumeSRGB:
        default:
            sourceProfile = IccTransform::createSRGBProfile();
            break;
        }
    }

    bound->m_boundSource = sourceProfile;
    bound->m_transform = std::make_shared<const IccTransform>(
        sourceProfile, m_target, m_intent, m_blackPointCompensation, dataType);
    return bound;
}

ColorProfile ColorManagement::amendColorProfile(const ColorProfile& input) const
{
    if (m_passThrough || m_boundSource.isEmpty()) {
        return input;
    }
    return m_boundSource;
}

void ColorManagement::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    if (m_passThrough || !m_transform) {
        block.copyTo(transformedBlock);
        return;
    }
    m_transform->apply(block, transformedBlock);
}

std::vector<DataType> ColorManagement::computeChannelDataTypes(
    const std::vector<DataType>& channels) const
{
    if (m_target == ColorTarget::sRGB) {
        return channels;
    }
    return std::vector<DataType>(channels.size(), DataType::DT_Float32);
}
```

- [ ] **Step 5: Add the wrapper and the type**

Add `ColorManagement` to the `TransformationType` enum in `transformationtype.hpp` and to its `operator<<` in `transformationtype.cpp`.

Create `colormanagementwrap.hpp`/`.cpp` mirroring `colortransformationwrap.hpp`/`.cpp` exactly: a `ColorManagementWrap : public TransformationWrapper` holding `std::shared_ptr<ColorManagement> m_filter`, with a default constructor, a converting constructor from `const ColorManagement&`, `getType()`, `getFilter()`, and forwarding property accessors for target, intent, black point compensation, missing profile policy and source profile override. Add the header to `wrappers.hpp` and the unwrapping case to `transformations.cpp` alongside `ColorTransformationWrap`.

Add all four new files to `set(SOURCE_FILES ...)` in `src/slideio/transformer/CMakeLists.txt`.

- [ ] **Step 6: Run the tests to verify they pass**

Run: `./build/Release/slideio_transformer_tests.exe --gtest_filter="ColorManagement.*"`
Expected: 9 tests PASS.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/transformer src/tests/transformer/test_colormanagement.cpp
git commit -m "add the ColorManagement scene transformation

Converts blocks into sRGB, CIELAB, linear RGB or CIEXYZ using the slide's
embedded profile, through the existing transformScene decorator path, so it
composes with the other filters and with stain normalisation later.

Every rejection happens at bind time, inside transformScene and before any
pixel moves, so a batch job fails on the file it cannot handle rather than
several thousand tiles later: a non-RGB scene, the Fail policy with nothing
embedded, and PassThrough with a non-sRGB target -- that last one because the
output data type would otherwise depend on whether a given file happened to
carry a profile, silently assembling tensors of two different dtypes.

setSourceProfileOverride is how a lab that has characterised its scanner
supplies real colorimetry. slideio ships no vendor profile table, because that
would assert colorimetry never measured on the device that produced the slide."
```

---

### Task 13: Keep concurrent reads through a transform

**Files:**
- Modify: `src/slideio/transformer/transformerscene.hpp`, `src/slideio/transformer/transformerscene.cpp`, `software-docs/BREAKING_CHANGES.md`, `CLAUDE.md`
- Test: `src/tests/transformer/test_colormanagement.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: `bool TransformerScene::supportsConcurrentReads() const override`.

- [ ] **Step 1: Write the failing test**

Append to `src/tests/transformer/test_colormanagement.cpp`:

```cpp
TEST(ColorManagement, transformedSceneKeepsTheOriginConcurrency)
{
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Slide> slide = openSlide(path, "SVS");
    std::shared_ptr<Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene->getCVScene()->supportsConcurrentReads());

    ColorManagement cm(ColorTarget::sRGB);
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    // Wrapping a scene in a transform must not silently serialise its reads.
    ASSERT_TRUE(managed->getCVScene()->supportsConcurrentReads());
}

TEST(ColorManagement, concurrentReadsOfAManagedSceneAgree)
{
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Slide> slide = openSlide(path, "SVS");
    ColorManagement cm(ColorTarget::sRGB);
    std::shared_ptr<Scene> managed = transformScene(slide->getScene(0), cm);

    const std::tuple<int, int, int, int> rect{0, 0, 256, 256};
    std::vector<uint8_t> expected(256 * 256 * 3);
    managed->readBlock(rect, expected.data(), expected.size());

    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 20; ++i) {
                std::vector<uint8_t> actual(expected.size());
                managed->readBlock(rect, actual.data(), actual.size());
                if (actual != expected) {
                    ++mismatches;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    ASSERT_EQ(0, mismatches.load());
}
```

Add `#include <thread>` and `#include <atomic>` to the file.

- [ ] **Step 2: Run them to verify the first fails**

Run: `./build/Release/slideio_transformer_tests.exe --gtest_filter="ColorManagement.*concurren*"`
Expected: `transformedSceneKeepsTheOriginConcurrency` FAILS — `TransformerScene` inherits `CVScene`'s `false`.

- [ ] **Step 3: Forward the origin's concurrency**

Add to `src/slideio/transformer/transformerscene.hpp`:

```cpp
        /**@brief a transformed scene reads as concurrently as its origin does.
         *
         * Every transformation applies as a const, stateless operation over a
         * caller-supplied block, and ColorManagement's bound state -- the
         * compiled lcms2 transform -- is immutable after binding. Without this
         * override a transform silently downgraded a concurrent scene to
         * serialised reads.*/
        bool supportsConcurrentReads() const override {
            return m_originScene->supportsConcurrentReads();
        }
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/Release/slideio_transformer_tests.exe --gtest_filter="ColorManagement.*concurren*"`
Expected: both PASS.

- [ ] **Step 5: Run the full transformer and main suites**

Run:
```bash
./build/Release/slideio_transformer_tests.exe
./build/Release/slideio_tests.exe
```
Expected: all PASS. The seven existing filters are now reachable concurrently for the first time, so any latent shared state in them would surface here.

- [ ] **Step 6: Record the behaviour change**

Add to `software-docs/BREAKING_CHANGES.md` under a `v2.10.0` heading:

```markdown
### TransformerScene now reports its origin scene's concurrency

`TransformerScene::supportsConcurrentReads()` previously inherited `CVScene`'s
`false`, so wrapping any scene in a transform silently serialised its reads --
an SVS scene that read concurrently stopped doing so. It now forwards the origin
scene's value.

Callers that relied on reads of a transformed scene being mutually exclusive in
order to protect their own state must take their own lock. This is the same
contract `Scene` already documents for driver scenes.
```

Update the concurrency bullet in `CLAUDE.md` to note that a transformed scene now inherits its origin's concurrency.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/transformer/transformerscene.hpp \
        src/tests/transformer/test_colormanagement.cpp \
        software-docs/BREAKING_CHANGES.md CLAUDE.md
git commit -m "keep a transformed scene as concurrent as its origin

TransformerScene inherited CVScene's false, so wrapping any scene in a
transform silently downgraded it to serialised reads. For an ML tile loader
with several workers that is the difference between a transform being usable
and not, and it would have made colour management cost exactly the throughput
it was added to serve.

Sound because every transformation applies as a const, stateless operation and
ColorManagement's compiled lcms2 transform is immutable after binding --
verified by applyIsSafeFromSeveralThreads in slideio_tests and by
concurrentReadsOfAManagedSceneAgree here. The seven existing filters become
concurrent too, which BREAKING_CHANGES.md records."
```

---

### Task 14: Python binding and public documentation

**Files (in the separate `slideio-python` repository at `D:\Projects\slideio\slideio-python`):**
- Modify: `src/pyscene.hpp`, `src/pyscene.cpp`, `src/pybind.cpp`
- Test: `tests/test_color.py`

**Files (in this repository):**
- Modify: `docs-src/Sphinx/source/*.rst`

**Interfaces:**
- Consumes: the full C++ surface from Tasks 1-13.
- Produces: `Scene.get_color_profile() -> bytes | None`, `Scene.get_color_profile_info() -> ColorProfileInfo`, the four enums, and `slideio.ColorManagement`.

This task needs the C++ libraries built and installed first, since the Python extension links against them.

- [ ] **Step 1: Write the failing test**

Create `tests/test_color.py` in the `slideio-python` repo:

```python
import os
import unittest
import slideio
from testlib import get_test_image_path


class TestColor(unittest.TestCase):
    def test_absent_profile_is_none(self):
        path = get_test_image_path("gdal", "colors.png")
        with slideio.open_slide(path, "AUTO") as slide:
            scene = slide.get_scene(0)
            self.assertIsNone(scene.get_color_profile())
            info = scene.get_color_profile_info()
            self.assertFalse(info.present)
            self.assertEqual(info.source, slideio.ColorProfileSource.NONE)

    def test_profile_bytes_round_trip_to_pillow(self):
        path = get_test_image_path("svs", "CMU-1-Small-Region.svs")
        with slideio.open_slide(path, "SVS") as slide:
            icc = slide.get_scene(0).get_color_profile()
            if icc is None:
                self.skipTest("the test slide carries no ICC profile")
            self.assertIsInstance(icc, bytes)
            import io
            from PIL import ImageCms
            profile = ImageCms.ImageCmsProfile(io.BytesIO(icc))
            self.assertTrue(ImageCms.getProfileDescription(profile))

    def test_lab_transform_returns_float32(self):
        path = get_test_image_path("gdal", "colors.png")
        with slideio.open_slide(path, "AUTO") as slide:
            scene = slide.get_scene(0)
            cm = slideio.ColorManagement(slideio.ColorTarget.LAB)
            managed = slideio.transform_scene(scene, cm)
            tile = managed.read_block((0, 0, 16, 16), size=(16, 16))
            self.assertEqual(tile.dtype.name, "float32")
            self.assertEqual(tile.shape[2], 3)
            self.assertTrue((tile[:, :, 0] >= -0.5).all())
            self.assertTrue((tile[:, :, 0] <= 100.5).all())

    def test_fail_policy_raises(self):
        path = get_test_image_path("gdal", "colors.png")
        with slideio.open_slide(path, "AUTO") as slide:
            scene = slide.get_scene(0)
            cm = slideio.ColorManagement(slideio.ColorTarget.LAB)
            cm.missing_profile_policy = slideio.MissingProfilePolicy.FAIL
            with self.assertRaises(RuntimeError):
                slideio.transform_scene(scene, cm)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python -m pytest tests/test_color.py -v`
Expected: FAIL — `Scene` has no attribute `get_color_profile`.

- [ ] **Step 3: Add the PyScene accessors**

In `src/pyscene.hpp`, declare:

```cpp
    pybind11::object getColorProfile() const;
    slideio::ColorProfileInfo getColorProfileInfo() const;
```

In `src/pyscene.cpp`:

```cpp
pybind11::object PyScene::getColorProfile() const
{
    const slideio::ColorProfile profile = m_scene->getColorProfile();
    if (profile.isEmpty()) {
        // None rather than an empty bytes object, so `if icc is None` reads
        // naturally and absence cannot be mistaken for a zero-length profile.
        return pybind11::none();
    }
    return pybind11::bytes(reinterpret_cast<const char*>(profile.getData().data()),
                           profile.getSize());
}

slideio::ColorProfileInfo PyScene::getColorProfileInfo() const
{
    return m_scene->getColorProfileInfo();
}
```

- [ ] **Step 4: Bind the types**

In `src/pybind.cpp`, add to the `Scene` class binding, next to `get_raw_metadata`:

```cpp
        .def("get_color_profile", &PyScene::getColorProfile,
             "Raw ICC profile bytes embedded in the scene, or None if it carries none")
        .def("get_color_profile_info", &PyScene::getColorProfileInfo,
             "Parsed ICC header of the scene colour profile")
```

Add the enums next to the existing `ColorSpace` enum, following its `.value(...).export_values()` style:

```cpp
    py::enum_<slideio::ColorTarget>(m, "ColorTarget")
        .value("SRGB", slideio::ColorTarget::sRGB)
        .value("LINEAR_RGB", slideio::ColorTarget::LinearRGB)
        .value("LAB", slideio::ColorTarget::Lab)
        .value("XYZ", slideio::ColorTarget::XYZ)
        .export_values();
    py::enum_<slideio::ColorProfileSource>(m, "ColorProfileSource")
        .value("NONE", slideio::ColorProfileSource::None)
        .value("EMBEDDED", slideio::ColorProfileSource::Embedded)
        .value("ASSUMED", slideio::ColorProfileSource::Assumed)
        .export_values();
    py::enum_<slideio::RenderingIntent>(m, "RenderingIntent")
        .value("PERCEPTUAL", slideio::RenderingIntent::Perceptual)
        .value("RELATIVE_COLORIMETRIC", slideio::RenderingIntent::RelativeColorimetric)
        .value("SATURATION", slideio::RenderingIntent::Saturation)
        .value("ABSOLUTE_COLORIMETRIC", slideio::RenderingIntent::AbsoluteColorimetric)
        .export_values();
    py::enum_<slideio::IccColorSpace>(m, "IccColorSpace")
        .value("UNKNOWN", slideio::IccColorSpace::Unknown)
        .value("GRAY", slideio::IccColorSpace::Gray)
        .value("RGB", slideio::IccColorSpace::RGB)
        .value("CMYK", slideio::IccColorSpace::CMYK)
        .value("LAB", slideio::IccColorSpace::Lab)
        .value("XYZ", slideio::IccColorSpace::XYZ)
        .value("YCBCR", slideio::IccColorSpace::YCbCr)
        .export_values();
    py::enum_<slideio::MissingProfilePolicy>(m, "MissingProfilePolicy")
        .value("ASSUME_SRGB", slideio::MissingProfilePolicy::AssumeSRGB)
        .value("PASS_THROUGH", slideio::MissingProfilePolicy::PassThrough)
        .value("FAIL", slideio::MissingProfilePolicy::Fail)
        .export_values();

    py::class_<slideio::ColorProfileInfo>(m, "ColorProfileInfo")
        .def_readonly("present", &slideio::ColorProfileInfo::present)
        .def_readonly("source", &slideio::ColorProfileInfo::source)
        .def_readonly("description", &slideio::ColorProfileInfo::description)
        .def_readonly("manufacturer", &slideio::ColorProfileInfo::manufacturer)
        .def_readonly("model", &slideio::ColorProfileInfo::model)
        .def_readonly("version", &slideio::ColorProfileInfo::version)
        .def_readonly("data_space", &slideio::ColorProfileInfo::dataSpace)
        .def_readonly("connection_space", &slideio::ColorProfileInfo::connectionSpace)
        .def_readonly("intent", &slideio::ColorProfileInfo::intent)
        .def_readonly("white_point", &slideio::ColorProfileInfo::whitePoint)
        .def_readonly("size", &slideio::ColorProfileInfo::dataSize)
        .def("__repr__", &slideio::ColorProfileInfo::toString);

    py::class_<slideio::ColorManagementWrap, slideio::TransformationWrapper>(m, "ColorManagement")
        .def(py::init<>())
        .def(py::init<slideio::ColorTarget>(), py::arg("target"))
        .def_property_readonly("type", &slideio::ColorManagementWrap::getType, "Type of transformation")
        .def_property("target", &slideio::ColorManagementWrap::getTarget,
                      &slideio::ColorManagementWrap::setTarget, "Target colour space")
        .def_property("intent", &slideio::ColorManagementWrap::getIntent,
                      &slideio::ColorManagementWrap::setIntent, "ICC rendering intent")
        .def_property("black_point_compensation",
                      &slideio::ColorManagementWrap::getBlackPointCompensation,
                      &slideio::ColorManagementWrap::setBlackPointCompensation,
                      "Apply black point compensation")
        .def_property("missing_profile_policy",
                      &slideio::ColorManagementWrap::getMissingProfilePolicy,
                      &slideio::ColorManagementWrap::setMissingProfilePolicy,
                      "What to do when the slide embeds no ICC profile");
```

- [ ] **Step 5: Build and run the tests to verify they pass**

Run: `pip install -e . && python -m pytest tests/test_color.py -v`
Expected: 4 tests PASS or SKIP.

- [ ] **Step 6: Document the public Python surface**

Add the new `Scene` methods, the enums and `ColorManagement` to the appropriate `docs-src/Sphinx/source/*.rst` pages, matching how `ColorTransformation` is documented there. These pages are published publicly; keep the wording free of internal detail.

- [ ] **Step 7: Commit, in both repositories**

```bash
# in slideio-python
git add src/pyscene.hpp src/pyscene.cpp src/pybind.cpp tests/test_color.py
git commit -m "expose colour profiles and colour management to Python

get_color_profile returns plain bytes, and None rather than an empty bytes
object when the slide carries no profile, so `if icc is None` reads naturally
and absence cannot be mistaken for a zero-length profile. Plain bytes because
PIL.ImageCms and colour-science consume it directly: a pipeline that prefers
its own colour maths gets full interoperability without slideio mediating."

# in slideio
git add docs-src/Sphinx/source
git commit -m "document the colour and ICC Python API"
```

---

## Self-Review

**Spec coverage.** Every section of `software-docs/specs/2026-09-12-color-icc-api-design.md` maps to a task: core types → 1; Scene surface → 2, 4; engine → 3, 5; binding and `amendColorProfile` → 11; `ColorManagement`, applicability, policy mechanics, what a managed scene reports → 12; concurrency → 13; driver extraction → 6, 7, 8, 9, 10; errors → 12; Python API → 14; testing → distributed across all tasks; documentation → 13 (`BREAKING_CHANGES.md`) and 14 (Sphinx). The spec's phasing maps to task order: phase 1 → Tasks 1-2, phase 2 → Tasks 3-5, phase 3 → Tasks 6-10, phase 4 → Tasks 11-12, phase 5 → Task 13.

**Known gaps, deliberate:**

- **PHTIFF, QPTIFF, SCN, VSI and OME-TIFF extraction tests** are folded into Task 7 rather than given tests of their own per format. The extraction code is shared, so per-format tests would assert the same code path five times; the SVS test plus the ome-tiff and scn ones give adequate coverage of the shared site.
- **CZI and ZVI need no task.** The defaulted virtual from Task 2 already reports absence correctly, which is the honest answer for formats that carry no ICC profile.
- **The ThreadSanitizer gate** named in the spec is not a task step, because CLAUDE.md and this branch's history record that the TSan merge gate has not been met for any driver on this Windows-only setup. `applyIsSafeFromSeveralThreads` (Task 5) and `concurrentReadsOfAManagedSceneAgree` (Task 13) are what actually run here. Do not claim TSan coverage that was not obtained.
- **Task 10's second GDAL test** names `gdal/icc/srgb-tagged.jpg`, which may not exist in the corpus. It skips cleanly if absent; adding it is optional and Step 5 covers regenerating `TEST_IMAGES.md` if you do.

**Type consistency check.** `ColorProfile`, `ColorProfileInfo`, `ColorProfileSource`, `IccColorSpace`, `RenderingIntent`, `ColorTarget` (Task 1) are used with identical spelling in Tasks 2-14. `IccTransform::describe` and `createSRGBProfile` (Task 3) are called with those exact names in Tasks 4 and 12. `bindToSource` and `amendColorProfile` (Task 11) are overridden with matching signatures in Task 12. `setColorProfile`/`getColorProfile` on driver scenes (Tasks 7-10) match the `CVScene` virtual from Task 2. `MissingProfilePolicy` values `AssumeSRGB`/`PassThrough`/`Fail` (Task 12) match the Python `ASSUME_SRGB`/`PASS_THROUGH`/`FAIL` (Task 14).
