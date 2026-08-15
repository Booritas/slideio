# PHTIFF Driver Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separate the Philips TIFF format from the Aperio SVS driver that hosts it — subclasses instead of driver-id branches, one metadata parse per open, and `PHTDescription` demoted to an internal parser — without changing what either driver returns.

**Architecture:** `PHTIFFImageDriver`, `PHTIFFSlide` and `PHTIFFTiledScene` become subclasses inside the existing svs driver library. The five driver-id branches become virtual dispatch. Because `SVSTiledScene` calls `initialize()` from its constructor and `initialize()` calls `processImageDescription()`, scene construction moves behind static factories first — a virtual call from a base constructor does not reach a derived override.

**Tech Stack:** C++17, tinyxml2, OpenCV, libtiff, GoogleTest, CMake, Conan.

**Spec:** `software-docs/specs/2026-08-15-phtiff-driver-refactor-design.md`

## Global Constraints

- **No observable behaviour change.** The acceptance criterion for every task is the existing suites passing *unchanged*: `slideio_phtiff_tests` (92), `slideio_tests` (489), plus `slideio_pke_tests` (15) and `slideio_ometiff_tests` (98) for Task 1. A diff in a test *expectation* is a signal to stop and re-examine the change, not to update the test. Test code may change where a signature moved; test *assertions* may not.
- Build with `python install.py -a build-only -c release`. Run a suite with `./build/bin/Release/<name>.exe` on Windows, `./build/release/bin/<name>` on Linux/macOS.
- The private dataset must be enabled (`SLIDEIO_TEST_DATA_PRIV_PATH` set), or the real-file tests skip and the net has holes.
- C++17. Prefer `constexpr`/`inline` variables over namespace-scope `const` objects.
- Every file keeps the three-line licence header it already has.
- Comments explain *why*, matching the density of the surrounding code.

## External dependencies that constrain this work

These are outside the svs driver and must keep compiling and passing:

- `src/slideio/drivers/afi/afislide.cpp:136` calls `SVSSlide::openFile(svsPath, "AFI")`. The afi driver library links the svs driver library. **`SVSSlide` keeps `SLIDEIO_SVS_EXPORTS` and `openFile(path, id)` keeps working for an arbitrary id.** Only `PHTDescription` loses its export.
- `src/slideio/drivers/scn/scnslide.cpp:88` constructs `SVSSmallScene`. That class is not touched by this plan.
- `src/tests/main/test_svs_driver.cpp:211` constructs `SVSTiledScene` **on the stack**. Task 3 changes this call site.

---

## File Map

**Create:**
- `src/slideio/drivers/svs/phtiffimagedriver.hpp` / `.cpp` — `PHTIFFImageDriver`: id, file specs, content sniff.
- `src/slideio/drivers/svs/phtiffslide.hpp` / `.cpp` — `PHTIFFSlide`: Philips `init`, image extraction, scene creation, metadata tree.
- `src/slideio/drivers/svs/phtiffscene.hpp` / `.cpp` — `PHTIFFTiledScene`: Philips `processImageDescription`.
- `src/slideio/drivers/svs/phtmetadata.hpp` / `.cpp` — `PHTMetadata` value struct and the one function that builds it.

**Modify:**
- `src/slideio/drivers/svs/phtdescription.hpp` / `.cpp` — nested namespace, `string_view` constants, no export, const getters.
- `src/slideio/drivers/svs/svsimagedriver.hpp` / `.cpp` — lose the id constructor parameter and both id branches.
- `src/slideio/drivers/svs/svsslide.hpp` / `.cpp` — Philips code leaves; `init` becomes virtual; private members become protected.
- `src/slideio/drivers/svs/svstiledscene.hpp` / `.cpp` — factories; `processImageDescription` virtual; Philips reader leaves.
- `src/slideio/drivers/svs/CMakeLists.txt` — the four new source pairs.
- `src/slideio/slideio/imagedrivermanager.cpp` — construct `PHTIFFImageDriver`.
- `src/slideio/drivers/pke/pkeslide.cpp`, `src/slideio/drivers/ome-tiff/otslide.cpp` — internal linkage for three globals.
- `src/tests/phtiff/CMakeLists.txt` — compile `phtdescription.cpp` into the test binary.
- `src/tests/phtiff/test_phtiff_driver.cpp` — namespace qualification, `MockPHTIFFSlide`, new tests.
- `src/tests/main/test_svs_driver.cpp` — one scene construction site.

**Not touched:** `svsscene.{hpp,cpp}`, `svssmallscene.{hpp,cpp}`, `svstools.{hpp,cpp}`, and every driver other than svs, pke and ome-tiff.

**Deferred, not in this plan:** `3305` → `33005` in `svstiledscene.cpp:150` and `pketiledscene.cpp:92`. The spec puts it out of scope because it changes what Aperio JP2K/RGB files report from `Unknown` to `Jpeg2000`. It needs its own commit and its own test.

---

## Task 1: Constants into `slideio::phtiff`, internal linkage for the duplicated globals

Mechanical, no behaviour. Lands first so it is not noise in the restructuring diff.

**Files:**
- Modify: `src/slideio/drivers/svs/phtdescription.hpp`, `phtdescription.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.cpp`, `svstiledscene.cpp`
- Modify: `src/slideio/drivers/pke/pkeslide.cpp:19-21`, `src/slideio/drivers/ome-tiff/otslide.cpp:20-22`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Produces: `namespace slideio::phtiff` holding all 32 constants; `PHTDescription::Attribute` with `std::string_view` members.

- [ ] **Step 1: Change `Attribute` to hold `string_view` and move the constants**

In `phtdescription.hpp`, change the member types and wrap every constant in a nested namespace. The class itself stays in `namespace slideio`:

```cpp
namespace slideio
{
    class SLIDEIO_SVS_EXPORTS PHTDescription
    {
    public:
        class Attribute
        {
        public:
            std::string_view Name;
            std::string_view Group;
            std::string_view Element;
        };
        // ... rest of the class unchanged for now; Task 2 revisits it
    };

    // The philips metadata vocabulary. Nested so that names like MANUFACTURER, LABEL,
    // WSI and BITS_STORED do not sit at slideio scope, where they are collision prone.
    // inline constexpr rather than namespace-scope const: one object for the whole
    // program instead of a copy per translation unit.
    namespace phtiff
    {
        // Slide (root DataObject) level attributes
        inline constexpr PHTDescription::Attribute MANUFACTURER = { "DICOM_MANUFACTURER", "0x0008", "0x0070" };
        // ... every existing constant, unchanged values, in its existing order and
        // keeping its existing section comments ...
        inline constexpr std::string_view SCANNED_IMAGE = "DPScannedImage";
        inline constexpr std::string_view WSI = "WSI";
        inline constexpr std::string_view PIXEL_DATA_REPRESENTATION = "PixelDataRepresentation";
        inline constexpr std::string_view DP_UFS_IMPORT = "DPUfsImport";
    }
}
```

Add `#include <string_view>` to the header.

- [ ] **Step 2: Follow the type change through `phtdescription.cpp`**

Three helpers take `const std::string&` and must take `std::string_view`:

```cpp
bool equalIgnoreCase(const char* text, std::string_view value);
```

and `getObjectList`'s `name` parameter becomes `std::string_view` (header and definition). `findAttribute`'s comparison `attribute.Name != name` keeps working: `std::string_view` compares against `const char*`. Streaming a `string_view` into `SLIDEIO_LOG` and `RAISE_RUNTIME_ERROR` works in C++17.

Add `using namespace slideio::phtiff;` below the existing `using namespace slideio;` — a using-directive inside a `.cpp` is fine; the point of the namespace is to keep these names out of a consumer's `slideio`.

- [ ] **Step 3: Qualify the use sites**

Add `using namespace slideio::phtiff;` to `svsslide.cpp` and `svstiledscene.cpp` after their existing `using namespace slideio;`. In `svsslide.cpp` the anonymous-namespace helpers currently write `slideio::IMAGE_TYPE`, `slideio::SCANNED_IMAGES` and so on — change those qualifications to `slideio::phtiff::`.

- [ ] **Step 4: Give the three duplicated globals internal linkage**

In `svsslide.cpp:26-28`, `pkeslide.cpp:19-21` and `otslide.cpp:20-22`, replace:

```cpp
const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";
```

with:

```cpp
// constexpr, not const char*: a const char* is a non-const pointer and so has external
// linkage, and all three driver libraries define these same three symbols.
namespace
{
    constexpr const char* THUMBNAIL = "Thumbnail";
    constexpr const char* MACRO = "Macro";
    constexpr const char* LABEL = "Label";
}
```

- [ ] **Step 5: Fix the test file**

Add `using namespace slideio::phtiff;` near the existing `using namespace slideio;`. One site needs a conversion: `phRemoveAttribute(xml, IMAGE_TYPE.Name, 1)` passes a `string_view` to a `const std::string&` parameter — change the helper's parameter to `std::string_view` and build the needle with `std::string(name)`.

- [ ] **Step 6: Build**

Run: `python install.py -a build-only -c release`
Expected: clean. Any missed qualification is a compile error, not a silent change.

- [ ] **Step 7: Run all four affected suites**

```
./build/bin/Release/slideio_phtiff_tests.exe
./build/bin/Release/slideio_tests.exe
./build/bin/Release/slideio_pke_tests.exe
./build/bin/Release/slideio_ometiff_tests.exe
```
Expected: 92, 489, 15, 98 — all passing, no assertion edited.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/drivers/svs/phtdescription.hpp src/slideio/drivers/svs/phtdescription.cpp \
        src/slideio/drivers/svs/svsslide.cpp src/slideio/drivers/svs/svstiledscene.cpp \
        src/slideio/drivers/pke/pkeslide.cpp src/slideio/drivers/ome-tiff/otslide.cpp \
        src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "move the philips metadata vocabulary out of slideio scope"
```

---

## Task 2: `PHTDescription` becomes an internal, const-correct parser

**Files:**
- Modify: `src/slideio/drivers/svs/phtdescription.hpp`, `phtdescription.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.cpp`, `svstiledscene.cpp` (callers)
- Modify: `src/tests/phtiff/CMakeLists.txt`

**Interfaces:**
- Consumes: the `slideio::phtiff` namespace from Task 1.
- Produces: `PHTDescription` with no export macro, `const` getters, and `getObjectList` returning `std::vector<const tinyxml2::XMLElement*>`.

- [ ] **Step 1: Drop the export, drop `getDocument`, make the getters const**

In `phtdescription.hpp`: remove `SLIDEIO_SVS_EXPORTS` from the class, delete the `getDocument()` member entirely, and mark every getter `const`:

```cpp
    class PHTDescription
    {
    public:
        // ... constructors, isPhilipsDescription unchanged ...
        std::vector<const tinyxml2::XMLElement*> getObjectList(const tinyxml2::XMLElement* parent,
            const Attribute& arrayAttribute, std::string_view name) const;
        int getAttributeInt(const tinyxml2::XMLElement* element, const Attribute& attribute) const;
        std::string getAttributeText(const tinyxml2::XMLElement* element, const Attribute& attribute) const;
        std::vector<std::string> getAttributeTextList(const tinyxml2::XMLElement* element, const Attribute& attribute) const;
        std::vector<double> getAttributeDoubleList(const tinyxml2::XMLElement* element, const Attribute& attribute) const;
        const tinyxml2::XMLElement* getRoot() const;
        bool hasAttribute(const tinyxml2::XMLElement* element, const Attribute& attribute) const;
    private:
        std::unique_ptr<tinyxml2::XMLDocument> m_doc;
    };
```

`getRoot()` returning `const XMLElement*` and `getObjectList` returning a vector of `const` pointers is what removes the `const_cast` in `collectObjects` — delete the cast there and change `objects` to `std::vector<const tinyxml2::XMLElement*>&`.

Note the `#if defined(_MSC_VER) #pragma warning(disable: 4251)` guard around the header exists because of the exported `unique_ptr` member. Leave it; it is harmless and the class may regain members later.

- [ ] **Step 2: Follow `const` through the callers**

In `svsslide.cpp` and `svstiledscene.cpp`, every `std::vector<tinyxml2::XMLElement*>` holding a `getObjectList` result becomes `std::vector<const tinyxml2::XMLElement*>`, and `tinyxml2::XMLElement* slide = philips.getRoot()` becomes `const tinyxml2::XMLElement*`. These are compile errors if missed.

- [ ] **Step 3: Compile the parser into the test binary**

In `src/tests/phtiff/CMakeLists.txt`:

```cmake
set(TEST_SOURCES
  test_phtiff_driver.cpp
  ${CMAKE_SOURCE_DIR}/src/slideio/drivers/svs/phtdescription.cpp
)
```

The class is no longer exported, so the test compiles the implementation it tests. Two copies exist in a test build; that is the accepted cost of keeping the ~40 existing unit tests working verbatim.

- [ ] **Step 4: Build and run**

Run: `python install.py -a build-only -c release`, then `slideio_phtiff_tests` and `slideio_tests`.
Expected: 92 and 489 passing, no assertion edited.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/drivers/svs/phtdescription.hpp src/slideio/drivers/svs/phtdescription.cpp \
        src/slideio/drivers/svs/svsslide.cpp src/slideio/drivers/svs/svstiledscene.cpp \
        src/tests/phtiff/CMakeLists.txt
git commit -m "make PHTDescription an internal const-correct parser"
```

---

## Task 3: Scene construction moves behind factories

The prerequisite for virtual dispatch. **Nothing becomes virtual in this task** — this is the mechanical safety step that makes Task 4 possible.

**Files:**
- Modify: `src/slideio/drivers/svs/svstiledscene.hpp`, `svstiledscene.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.cpp:422`, `:593`
- Modify: `src/tests/main/test_svs_driver.cpp:211`

**Interfaces:**
- Produces: `SVSTiledScene::create(...)` returning `std::shared_ptr<SVSTiledScene>`; `initialize()` protected.

- [ ] **Step 1: Add the factories, stop initializing from the constructors**

In `svstiledscene.hpp`, move `initialize()` from public to protected, and add two factories mirroring the two constructors:

```cpp
    public:
        // Constructs and initializes. A factory rather than a constructor call because
        // initialize() reads the image description through a virtual method, and a
        // virtual call made from a constructor does not reach a derived override.
        static std::shared_ptr<SVSTiledScene> create(const std::string& filePath,
            const std::string& driverId, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
        static std::shared_ptr<SVSTiledScene> create(const std::string& filePath,
            const std::string& driverId, libtiff::TIFF* hFile, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
```

In `svstiledscene.cpp`, delete the `initialize();` call from both constructor bodies and add:

```cpp
std::shared_ptr<SVSTiledScene> SVSTiledScene::create(const std::string& filePath,
    const std::string& driverId, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) {
    std::shared_ptr<SVSTiledScene> scene(new SVSTiledScene(filePath, driverId, name, dirs));
    scene->initialize();
    return scene;
}

std::shared_ptr<SVSTiledScene> SVSTiledScene::create(const std::string& filePath,
    const std::string& driverId, libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) {
    std::shared_ptr<SVSTiledScene> scene(new SVSTiledScene(filePath, driverId, hFile, name, dirs));
    scene->initialize();
    return scene;
}
```

Also make `m_directories` protected — Task 4's subclass reads it.

- [ ] **Step 2: Update the two production call sites**

`svsslide.cpp:422` (in `initSVS`) and `svsslide.cpp:593` (in `phCreateImageScene`) currently read
`std::shared_ptr<SVSTiledScene> tScene(new SVSTiledScene(...));`. Replace each with
`auto tScene = SVSTiledScene::create(...);` passing the same arguments.

- [ ] **Step 3: Update the stack construction in the SVS tests**

`src/tests/main/test_svs_driver.cpp:211` reads:

```cpp
slideio::SVSTiledScene scene(fake_path, "fake_name", "fake", dirs);
```

`initialize()` is protected now, so a stack object cannot be initialized. Replace with:

```cpp
auto scene = slideio::SVSTiledScene::create(fake_path, "fake_name", "fake", dirs);
```

and change the uses below it from `scene.` to `scene->`.

- [ ] **Step 4: Build and run**

Run: `python install.py -a build-only -c release`, then `slideio_phtiff_tests` and `slideio_tests`.
Expected: 92 and 489 passing. If a scene comes back with zero levels or a zero resolution, a construction site was missed.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/drivers/svs/svstiledscene.hpp src/slideio/drivers/svs/svstiledscene.cpp \
        src/slideio/drivers/svs/svsslide.cpp src/tests/main/test_svs_driver.cpp
git commit -m "construct tiled scenes through a factory instead of the constructor"
```

---

## Task 4: `PHTIFFTiledScene`

**Files:**
- Create: `src/slideio/drivers/svs/phtiffscene.hpp`, `phtiffscene.cpp`
- Modify: `src/slideio/drivers/svs/svstiledscene.hpp`, `svstiledscene.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.cpp` (`phCreateImageScene`)
- Modify: `src/slideio/drivers/svs/CMakeLists.txt`

**Interfaces:**
- Consumes: `SVSTiledScene::create` and protected `initialize()`/`m_directories` from Task 3.
- Produces: `PHTIFFTiledScene::create(filePath, hFile, name, dirs)` returning `std::shared_ptr<PHTIFFTiledScene>`.

- [ ] **Step 1: Make the hook virtual and delete the Philips branch**

In `svstiledscene.hpp`, replace the three `processImageDescription*` declarations with one virtual method:

```cpp
    protected:
        // Reads the format specific fields — resolution, magnification, raw metadata —
        // out of the description of the base directory. Called by initialize().
        virtual void processImageDescription();
```

In `svstiledscene.cpp`, delete `processImageDescription()`'s driver-id branch and rename `processImageDescriptionSVS` to `processImageDescription`. Move the whole body of `processImageDescriptionPhTiff` out to the new file, and drop the now-unused `phtdescription.hpp` and `svsimagedriver.hpp` includes.

- [ ] **Step 2: Add the subclass**

`phtiffscene.hpp`:

```cpp
#pragma once
#include "slideio/drivers/svs/svstiledscene.hpp"

namespace slideio
{
    // The whole slide image of a philips tiff. Everything but the reading of the philips
    // metadata is the shared tiff behaviour of SVSTiledScene.
    class SLIDEIO_SVS_EXPORTS PHTIFFTiledScene : public SVSTiledScene
    {
    public:
        static std::shared_ptr<PHTIFFTiledScene> create(const std::string& filePath,
            libtiff::TIFF* hFile, const std::string& name,
            const std::vector<slideio::TiffDirectory>& dirs);
    protected:
        PHTIFFTiledScene(const std::string& filePath, libtiff::TIFF* hFile,
            const std::string& name, const std::vector<slideio::TiffDirectory>& dirs);
        void processImageDescription() override;
    };
}
```

`phtiffscene.cpp` holds the constructor (forwarding to the `SVSTiledScene` constructor with `PHTIFF_DRIVER_ID`) and `processImageDescription` — the former `processImageDescriptionPhTiff` body verbatim, including its raw-metadata, magnification and resolution work — plus the factory:

```cpp
PHTIFFTiledScene::PHTIFFTiledScene(const std::string& filePath, libtiff::TIFF* hFile,
    const std::string& name, const std::vector<slideio::TiffDirectory>& dirs)
    : SVSTiledScene(filePath, PHTIFF_DRIVER_ID, hFile, name, dirs) {
}

std::shared_ptr<PHTIFFTiledScene> PHTIFFTiledScene::create(const std::string& filePath,
    libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) {
    std::shared_ptr<PHTIFFTiledScene> scene(new PHTIFFTiledScene(filePath, hFile, name, dirs));
    scene->initialize();
    return scene;
}
```

- [ ] **Step 3: Create the Philips scene through the subclass**

In `svsslide.cpp`, `phCreateImageScene` replaces `SVSTiledScene::create(...)` with `PHTIFFTiledScene::create(m_filePath, keeper.release(), "Image", image_dirs)`, and calls `tScene->setDriverId(m_driverId)` as it does today.

- [ ] **Step 4: Add the sources to CMake**

In `src/slideio/drivers/svs/CMakeLists.txt`, add `phtiffscene.hpp` and `phtiffscene.cpp` to `SOURCE_FILES`.

- [ ] **Step 5: Build and run**

Run the build, then `slideio_phtiff_tests` and `slideio_tests`.
Expected: 92 and 489 passing. The tests that would catch a dispatch failure are `imageSceneMagnificationComesFromTheZoomLevels`, `imageSceneDescribesItsTiffDirectory` and `metadataOfTheTestFiles`: if the override is not reached, magnification is 0 and the resolution is the Aperio one.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/svs/phtiffscene.hpp src/slideio/drivers/svs/phtiffscene.cpp \
        src/slideio/drivers/svs/svstiledscene.hpp src/slideio/drivers/svs/svstiledscene.cpp \
        src/slideio/drivers/svs/svsslide.cpp src/slideio/drivers/svs/CMakeLists.txt
git commit -m "read the philips image description in a PHTIFFTiledScene subclass"
```

---

## Task 5: `PHTIFFSlide`

The largest move. Everything Philips leaves `svsslide.cpp`.

**Files:**
- Create: `src/slideio/drivers/svs/phtiffslide.hpp`, `phtiffslide.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.hpp`, `svsslide.cpp`
- Modify: `src/slideio/drivers/svs/CMakeLists.txt`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: `PHTIFFTiledScene::create` from Task 4.
- Produces: `PHTIFFSlide` with `static std::shared_ptr<SVSSlide> openFile(const std::string& filePath)`; `SVSSlide::init` virtual; `SVSSlide`'s members protected.

- [ ] **Step 1: Open up `SVSSlide` and make `init` virtual**

In `svsslide.hpp`: move `m_Scenes`, `m_auxImages` and `m_filePath` from private to protected. Replace the Philips declarations with a virtual hook and a shared open helper. `PHTLevel` stays in this header only if `PHTIFFSlide` still needs it — move the struct to `phtiffslide.hpp` and delete it here.

```cpp
    public:
        // Unchanged, and used by the afi driver with its own id.
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id);
    protected:
        // Opens the file, scans the directories and hands them to slide->init(). The
        // caller supplies the instance, which is what selects the format.
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id,
                                                  std::shared_ptr<SVSSlide> slide);
        // Builds the scenes from the scanned directories. The keeper owns the tiff handle
        // until the scene that reads from it is created.
        virtual void init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper);
```

`initSVS` is renamed to `init`. Delete `phExtractImages`, `phCreateImageScene`, `phCreateAuxScenes` and `initPhTiff` from this header. `buildMetadataTree` loses its driver-id branch and keeps only the Aperio path.

In `svsslide.cpp`, the public `openFile` becomes:

```cpp
std::shared_ptr<SVSSlide> SVSSlide::openFile(const std::string& filePath, const std::string& driverId)
{
    return openFile(filePath, driverId, std::shared_ptr<SVSSlide>(new SVSSlide));
}
```

and the three-argument overload is today's body with `slide.reset(new SVSSlide)` replaced by the passed-in instance and the id branch replaced by `slide->init(directories, keeper)`.

- [ ] **Step 2: Move the Philips code**

`phtiffslide.hpp` declares `PHTLevel` (moved verbatim, comments included) and:

```cpp
namespace slideio
{
    class SLIDEIO_SVS_EXPORTS PHTIFFSlide : public SVSSlide
    {
    public:
        static std::shared_ptr<SVSSlide> openFile(const std::string& filePath);
    protected:
        PHTIFFSlide() = default;
        void init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper) override;
        MetadataBuilder buildMetadataTree() const override;
        void extractImages(const std::vector<TiffDirectory>& directories,
            std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages);
        void createImageScene(const std::vector<TiffDirectory>& directories,
            const std::vector<PHTLevel>& imagePyramid, libtiff::TIFF* tiff);
        void createAuxScenes(const std::vector<TiffDirectory>& directories,
            const std::map<std::string, int>& auxImages);
    };
}
```

`extractImages` is a member, not a static — closing the review's minor item. The three `ph` prefixes drop from the method names since the class name now carries the format.

`phtiffslide.cpp` receives, moved verbatim from `svsslide.cpp`: the anonymous-namespace helpers `phEqualIgnoreCase`, `phAuxImageName`, `phDeclaredLevels`, `phLevelContentSize`, `phCropLevelPadding`, `PH_MAX_LEVEL_NUMBER`, `PHDeclaredLevel`, the metadata-tree helpers (`phWarnSkipped`, `phSetText`, `phSetInt`, `phSetList`, `phSetTextList`, `phSetDoubleList`, `phHasAny`, `phBuildLevelNode`, `phBuildImageNode`, `phBuildMetadataTree`), and the bodies of the four member functions. `THUMBNAIL`/`MACRO`/`LABEL` are needed by `phAuxImageName`, so the anonymous-namespace block from Task 1 Step 4 is copied here and deleted from `svsslide.cpp` if nothing there still uses it — `initSVS` does, so keep both.

`PHTIFFSlide::openFile` is:

```cpp
std::shared_ptr<SVSSlide> PHTIFFSlide::openFile(const std::string& filePath)
{
    return SVSSlide::openFile(filePath, PHTIFF_DRIVER_ID, std::shared_ptr<PHTIFFSlide>(new PHTIFFSlide));
}
```

- [ ] **Step 3: Add the sources to CMake**

Add `phtiffslide.hpp` and `phtiffslide.cpp` to `SOURCE_FILES`.

- [ ] **Step 4: Retarget the test mock**

In `test_phtiff_driver.cpp`, `MockSVSSlide` becomes `MockPHTIFFSlide : public PHTIFFSlide`, and its forwarding methods call the renamed members:

```cpp
class MockPHTIFFSlide : public PHTIFFSlide
{
public:
    static void extractImagesMock(const std::vector<TiffDirectory>& directories,
        std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages) {
        MockPHTIFFSlide slide;
        slide.extractImages(directories, imagePyramid, auxImages);
    }
    // createImageSceneMock, createAuxScenesMock, initMock, metadataTreeOf,
    // phMetadataTree, createFakeXml — bodies unchanged apart from the renames
};
```

`extractImages` is no longer static, so `extractImagesMock` makes an instance. Rename every call site (`phExtractImagesMock` → `extractImagesMock`, `phCreateImageSceneMock` → `createImageSceneMock`, `phCreateAuxScenesMock` → `createAuxScenesMock`, `initPhTiffMock` → `initMock`) and every `MockSVSSlide::` → `MockPHTIFFSlide::`. Roughly 20 sites. **No assertion changes.**

`aFailedOpenDoesNotLeaveTheFileOpen` calls `SVSSlide::openFile(copy.string(), PHTIFF_DRIVER_ID)`; change it to `PHTIFFSlide::openFile(copy.string())`, keeping the `EXPECT_THROW` and the deletion check exactly as they are.

- [ ] **Step 5: Build and run**

Expected: 92 and 489 passing, no assertion edited.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/svs/phtiffslide.hpp src/slideio/drivers/svs/phtiffslide.cpp \
        src/slideio/drivers/svs/svsslide.hpp src/slideio/drivers/svs/svsslide.cpp \
        src/slideio/drivers/svs/CMakeLists.txt src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "move the philips slide into a PHTIFFSlide subclass"
```

---

## Task 6: `PHTIFFImageDriver`

**Files:**
- Create: `src/slideio/drivers/svs/phtiffimagedriver.hpp`, `phtiffimagedriver.cpp`
- Modify: `src/slideio/drivers/svs/svsimagedriver.hpp`, `svsimagedriver.cpp`
- Modify: `src/slideio/slideio/imagedrivermanager.cpp:87`
- Modify: `src/slideio/drivers/svs/CMakeLists.txt`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: `PHTIFFSlide::openFile(filePath)` from Task 5.
- Produces: `PHTIFFImageDriver` with `getID() == "PHTIFF"`.

- [ ] **Step 1: Write the failing tests**

Add to `test_phtiff_driver.cpp`:

```cpp
// The two drivers claim different files. Before the split a single class served both
// and getFileSpecs treated any id that was not SVS as philips, so a third format added
// to it would silently have inherited the philips pattern.
TEST_F(PhTiffImageDriverTests, driversClaimTheirOwnFileSpecs) {
	EXPECT_EQ("*.svs", SVSImageDriver().getFileSpecs());
	EXPECT_EQ("*.tif;*.tiff", PHTIFFImageDriver().getFileSpecs());
	EXPECT_EQ("SVS", SVSImageDriver().getID());
	EXPECT_EQ("PHTIFF", PHTIFFImageDriver().getID());
}

// An svs file is identified by its extension alone and must not be content sniffed;
// a philips file is a *.tif that only its metadata identifies.
TEST_F(PhTiffImageDriverTests, onlyThePhilipsDriverSniffsContent) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	const std::string philips = TestTools::getFullTestImagePath("philips", "Philips-4.tiff");
	const std::string plainTiff = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
	EXPECT_TRUE(PHTIFFImageDriver().canOpenFile(philips));
	EXPECT_FALSE(PHTIFFImageDriver().canOpenFile(plainTiff));
	EXPECT_FALSE(SVSImageDriver().canOpenFile(philips)) << "wrong extension for the svs driver";
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `slideio_phtiff_tests --gtest_filter="*driversClaimTheirOwnFileSpecs*:*onlyThePhilipsDriverSniffsContent*"`
Expected: compile failure — `PHTIFFImageDriver` does not exist.

- [ ] **Step 3: Split the driver**

`svsimagedriver.hpp`: the constructor loses its parameter and `m_driverId`; `getID()` returns `SVS_DRIVER_ID`; `getFileSpecs()` returns `*.svs`; `canOpenFile` is deleted so the base-class extension check applies. Keep both id constants in this header — `PHTIFF_DRIVER_ID` is still the public id string.

`phtiffimagedriver.hpp` / `.cpp`:

```cpp
namespace slideio
{
    // Philips files are *.tif and *.tiff, an extension that says nothing: gdal reads
    // plain tiff and the ome-tiff driver reads its own flavour. Only the metadata in the
    // description of the first directory identifies a philips file.
    class SLIDEIO_SVS_EXPORTS PHTIFFImageDriver : public SVSImageDriver
    {
    public:
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
        bool canOpenFile(const std::string& filePath) const override;
    };
}
```

`canOpenFile` is today's PHTIFF branch verbatim — the `ImageDriver::canOpenFile` extension check, then `TIFFKeeper` plus `PHTDescription::isPhilipsDescription`, with the same `catch (const std::exception&) { return false; }`. `openFile` returns `PHTIFFSlide::openFile(filePath)`.

- [ ] **Step 4: Register it**

`imagedrivermanager.cpp:87`: `auto driver = std::make_shared<SVSImageDriver>(PHTIFF_DRIVER_ID);` becomes `auto driver = std::make_shared<PHTIFFImageDriver>();`, with the include added. `driverOrder` is unchanged — it already lists `"PHTIFF"`.

- [ ] **Step 5: Add the sources to CMake, build, run**

Expected: the two new tests pass; 94 phtiff and 489 main passing.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/svs/phtiffimagedriver.hpp src/slideio/drivers/svs/phtiffimagedriver.cpp \
        src/slideio/drivers/svs/svsimagedriver.hpp src/slideio/drivers/svs/svsimagedriver.cpp \
        src/slideio/slideio/imagedrivermanager.cpp src/slideio/drivers/svs/CMakeLists.txt \
        src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "give the philips format a driver class of its own"
```

---

## Task 7: Parse the philips metadata once per open

**Files:**
- Create: `src/slideio/drivers/svs/phtmetadata.hpp`, `phtmetadata.cpp`
- Modify: `src/slideio/drivers/svs/phtiffslide.hpp`, `phtiffslide.cpp`
- Modify: `src/slideio/drivers/svs/phtiffscene.hpp`, `phtiffscene.cpp`
- Modify: `src/slideio/drivers/svs/CMakeLists.txt`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: `PHTIFFSlide` and `PHTIFFTiledScene` from Tasks 4 and 5.
- Produces: `PHTMetadata readPHTMetadata(const std::string& description)`; `PHTIFFTiledScene::create(filePath, hFile, name, dirs, metadata)`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_F(PhTiffImageDriverTests, readPHTMetadataReadsTheImagesAndTheirLevels) {
	const PHTMetadata metadata = readPHTMetadata(
		MockPHTIFFSlide::createFakeXml(1024, 768, 3, {"MACROIMAGE"}));
	ASSERT_EQ(2u, metadata.images.size());
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1024, wsi->size.width);
	EXPECT_EQ(768, wsi->size.height);
	EXPECT_DOUBLE_EQ(phDefaults::PIXEL_SPACING, wsi->spacing.x);
	ASSERT_EQ(3u, wsi->levels.size());
	EXPECT_EQ(1, wsi->levels[1].number);
	EXPECT_EQ(512, wsi->levels[1].declaredSize.width);
	EXPECT_TRUE(metadata.images[1].levels.empty()) << "an auxiliary image has no pyramid";
}

// The skip-with-warning behaviour the robustness work added lives in the parse now.
TEST_F(PhTiffImageDriverTests, readPHTMetadataSkipsIncompleteDeclarations) {
	const std::string noType = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2, {"MACROIMAGE"}), IMAGE_TYPE.Name, 1);
	EXPECT_EQ(1u, readPHTMetadata(noType).images.size());

	const std::string noNumber = phRemoveAttribute(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_NUMBER.Name, 1);
	// Bind the metadata to a local: wholeSlideImage() returns a pointer into it, so
	// calling it on the temporary would leave a dangling pointer.
	const PHTMetadata metadata = readPHTMetadata(noNumber);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1u, wsi->levels.size());
}

TEST_F(PhTiffImageDriverTests, readPHTMetadataRaisesOnADescriptionItCannotParse) {
	EXPECT_THROW(readPHTMetadata("this is not xml at all"), slideio::RuntimeError);
}
```

- [ ] **Step 2: Run to verify they fail**

Expected: compile failure — `readPHTMetadata` does not exist.

- [ ] **Step 3: Add the value struct**

`phtmetadata.hpp` — no tinyxml2:

```cpp
#pragma once
#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/base/resolution.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace slideio
{
    // One zoom level as the philips metadata declares it. The declared size is optional:
    // the metadata may omit LEVEL_COLUMNS/LEVEL_ROWS, and a zero size means "not stated".
    struct PHTLevelDeclaration
    {
        int number = 0;
        cv::Size declaredSize = {};
        Resolution spacing = {};
    };

    // One DPScannedImage: the whole slide image or an auxiliary one.
    struct PHTImageDeclaration
    {
        std::string type;
        cv::Size size = {};
        Resolution spacing = {};
        std::vector<PHTLevelDeclaration> levels;   // empty for an auxiliary image
    };

    // Everything the driver needs from the philips xml, parsed once per open. What only
    // the metadata tree needs is deliberately absent: building that tree needs nearly the
    // whole document, and it is built lazily, so folding it in here would parse 844 KB on
    // every open for callers that never ask for metadata.
    struct SLIDEIO_SVS_EXPORTS PHTMetadata
    {
        std::vector<PHTImageDeclaration> images;
        const PHTImageDeclaration* wholeSlideImage() const;
    };

    // Raises if the description is not parseable philips metadata. An image or level
    // declaration that is incomplete is skipped with a warning, not raised on.
    SLIDEIO_SVS_EXPORTS PHTMetadata readPHTMetadata(const std::string& description);
}
```

`phtmetadata.cpp` implements `wholeSlideImage()` (first image whose `type == WSI`, else `nullptr`) and `readPHTMetadata`, which constructs one `PHTDescription` and walks it. The per-attribute guards move here verbatim from `phDeclaredLevels` and `processImageDescription`: `hasAttribute` before every read, skip-with-warning for a missing `IMAGE_TYPE` or `LEVEL_NUMBER`, and the existing warning texts.

- [ ] **Step 4: Run the new tests**

Expected: PASS.

- [ ] **Step 5: Feed it through the slide and the scene**

`PHTIFFSlide::init` builds the metadata once and passes it on:

```cpp
void PHTIFFSlide::init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper) {
    if (directories.empty()) {
        RAISE_RUNTIME_ERROR << "PHTIFFSlide: empty directory list!";
    }
    const PHTMetadata metadata = readPHTMetadata(directories.front().description);
    std::vector<PHTLevel> imagePyramid;
    std::map<std::string, int> auxImages;
    extractImages(directories, metadata, imagePyramid, auxImages);
    createImageScene(directories, metadata, imagePyramid, keeper.release());
    createAuxScenes(directories, auxImages);
    m_rawMetadata = directories.front().description;
    m_metadataFormat = MetadataFormat::XML;
}
```

`extractImages` and `createImageScene` gain a `const PHTMetadata&` parameter; `extractImages` uses `metadata.wholeSlideImage()->levels` in place of its own `phDeclaredLevels` call, and `phDeclaredLevels` is deleted.

`PHTIFFTiledScene::create` gains a `const PHTMetadata&` parameter, stored in a member, and its `processImageDescription` takes the resolution from `metadata.wholeSlideImage()->spacing` instead of parsing. The raw-metadata and magnification work is unchanged — the magnification comes from the tiff directory descriptions, not the xml.

The `PHTDescription` construction in `processImageDescription` is deleted. Open-time parses are now one.

- [ ] **Step 6: Build and run everything**

Expected: 97 phtiff and 489 main passing, no assertion edited.

- [ ] **Step 7: Confirm the parse count**

Temporarily add `SLIDEIO_LOG(WARNING) << "PHTDescription parsed";` to `PHTDescription`'s constructor, run `slideio_phtiff_tests --gtest_filter="*openSlide2*"`, and confirm exactly **one** line. Then remove it. This is the only direct evidence the task's goal was met; the suites cannot see a parse count.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/drivers/svs/phtmetadata.hpp src/slideio/drivers/svs/phtmetadata.cpp \
        src/slideio/drivers/svs/phtiffslide.hpp src/slideio/drivers/svs/phtiffslide.cpp \
        src/slideio/drivers/svs/phtiffscene.hpp src/slideio/drivers/svs/phtiffscene.cpp \
        src/slideio/drivers/svs/CMakeLists.txt src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "parse the philips metadata once per open"
```

---

## Task 8: Final verification

- [ ] **Step 1: Confirm no driver-id branch survives**

Run: `grep -rn "PHTIFF_DRIVER_ID\|SVS_DRIVER_ID" src/slideio/`
Expected: only the two definitions in `svsimagedriver.hpp`, the `getID()` implementations, and `PHTIFFSlide::openFile`'s single use. No `if` on either.

- [ ] **Step 2: Confirm `svsslide.cpp` shrank**

Run: `wc -l src/slideio/drivers/svs/*.cpp`
Expected: `svsslide.cpp` near 400 lines, down from 690.

- [ ] **Step 3: Run every suite**

```
slideio_tests, slideio_phtiff_tests, slideio_pke_tests, slideio_ometiff_tests,
slideio_ndpi_tests, slideio_vsi_tests, slideio_converter_tests, slideio_transformer_tests
```
Expected: 489, 97, 15, 98, 29, 30, 140, 39 — all passing.

- [ ] **Step 4: Update the spec's status line**

Set `**Status:** Implemented <date>` in `software-docs/specs/2026-08-15-phtiff-driver-refactor-design.md` and commit.
