# Philips TIFF Format Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `openSlide(path)` without a driver id open a Philips TIFF with the PHTIFF driver, while a plain TIFF still goes to GDAL and an OME-TIFF to OMETIFF.

**Architecture:** Detection stays a property of the ordered driver list in `ImageDriverManager::findDriver`. `SVSImageDriver` gains a `canOpenFile` override that, for the PHTIFF id only, opens the file and identifies Philips metadata in the image description of the first TIFF directory; `"PHTIFF"` is then inserted into `driverOrder` immediately before `"GDAL"`, so a non-Philips TIFF falls through to GDAL on its own. No TIFF special case is added to the manager.

**Tech Stack:** C++17, libtiff, tinyxml2, OpenCV, GoogleTest, CMake, Conan, Visual Studio 17 2022.

**Spec:** `software-docs/specs/2026-08-11-phtiff-format-detection-design.md`

## Global Constraints

- Driver library: `src/slideio/drivers/svs` builds `slideio-svs`; the PHTIFF driver is the same `SVSImageDriver` class constructed with a different id.
- Build: `cmake --build build --config Release --target <target>` from the repository root. Configure already done; do not re-run `install.py`.
- Test binaries land in `build/bin/Release/`.
- Test data comes from three environment variables: `SLIDEIO_TEST_DATA_PATH` via `TestTools::getTestImagePath(folder, name)`, and `SLIDEIO_IMAGES_PATH` via `TestTools::getFullTestImagePath(folder, name)`. The Philips files are only under the latter.
- Philips test files, for reference: `Philips-1.tiff` (pyramid only), `Philips-2.tiff` (pyramid + macro), `Philips-3.tiff` (pyramid + macro + label), `Philips-4.tiff` (pyramid + macro).
- Every task ends green on both `slideio_phtiff_tests` and `slideio_tests`.
- All AI-written code needs human review before merge; the change alters which driver reads a slide, so treat it as a candidate design change under IEC 62304 / ISO 13485 (see spec §8).

---

## File Map

**Modify:**
- `src/slideio/drivers/svs/phtdescription.hpp` — add the `DP_UFS_IMPORT` root object type constant; declare `PHTDescription::isPhilipsDescription`.
- `src/slideio/drivers/svs/phtdescription.cpp` — implement `isPhilipsDescription`.
- `src/slideio/drivers/svs/svsimagedriver.hpp` — add the `SVS_DRIVER_ID` / `PHTIFF_DRIVER_ID` constants; declare the `canOpenFile` override.
- `src/slideio/drivers/svs/svsimagedriver.cpp` — use the constants in `getFileSpecs`; implement `canOpenFile`.
- `src/slideio/drivers/svs/svsslide.cpp` — replace the `"PHTIFF"` literal at the `initPhTiff` branch with the constant.
- `src/slideio/drivers/svs/svstiledscene.cpp` — replace the `"PHTIFF"` literal in `processImageDescription` with the constant.
- `src/slideio/imagetools/tiffkeeper.cpp` — install `TIFFMessageHandler` in the path constructor.
- `src/slideio/slideio/imagedrivermanager.cpp` — use the constant at the PHTIFF registration; add `"PHTIFF"` to `driverOrder`.
- `src/tests/phtiff/test_phtiff_driver.cpp` — new `isPhilipsDescription` tests; rewrite `canOpenFile`; new `findDriver` and `openSlideWithoutDriverId` tests.

**Create:** nothing.

---

### Task 1: `PHTDescription::isPhilipsDescription`

A predicate over the image description string, with no file access, so its edge cases are unit-testable on their own.

**Files:**
- Modify: `src/slideio/drivers/svs/phtdescription.hpp:84-86` (the block of object type constants), and the public method list at `:33-41`
- Modify: `src/slideio/drivers/svs/phtdescription.cpp`
- Test: `src/tests/phtiff/test_phtiff_driver.cpp` (the `PHTDescriptionTests` suite, at the end of the file)

**Interfaces:**
- Consumes: nothing from earlier tasks. Uses the file-local constants already in `phtdescription.cpp`: `DATA_OBJECT_TAG` (`"DataObject"`) and `OBJECT_TYPE_PROPERTY` (`"ObjectType"`).
- Produces: `static bool PHTDescription::isPhilipsDescription(const std::string& description);` — true when the string is an XML document whose root element is `DataObject` with `ObjectType="DPUfsImport"`. Also `const std::string slideio::DP_UFS_IMPORT = "DPUfsImport";` in `phtdescription.hpp`. Task 3 calls the predicate.

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/phtiff/test_phtiff_driver.cpp`, after the last `PHTDescriptionTests` test (`createFakeXmlClampsLevelSizeToOnePixel`):

```cpp
// The description of the first tiff directory is what identifies a philips file:
// the extension *.tif says nothing, gdal and ome-tiff use it too.
TEST_F(PHTDescriptionTests, isPhilipsDescriptionAcceptsPhilipsMetadata) {
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(phSampleXML));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockSVSSlide::createFakeXml()));
	EXPECT_TRUE(PHTDescription::isPhilipsDescription(MockSVSSlide::fakeXML));
}

TEST_F(PHTDescriptionTests, isPhilipsDescriptionRejectsOtherDescriptions) {
	// The description of an ome-tiff file.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<?xml version=\"1.0\"?><OME xmlns=\"http://www.openmicroscopy.org/Schemas/OME/2016-06\">"
		"<Image ID=\"Image:0\"/></OME>"));
	// The description of an aperio svs file.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"Aperio Image Library v11.2.1\r\n46920x33014 [0,100 46000x32914] (256x256)"
		" JPEG/RGB Q=30|AppMag = 20"));
	// The description of a zoom level of a philips file, which is not the metadata.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("level=1 mag=22 quality=80"));
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(""));
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("   \r\n\t"));
	// The marker alone, in text that is not xml.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("DPUfsImport"));
	// Xml, but not the philips import object.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<DataObject ObjectType=\"DPScannedImage\">DPUfsImport</DataObject>"));
	// The philips import object, but not as the root element.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription(
		"<Wrapper><DataObject ObjectType=\"DPUfsImport\"/></Wrapper>"));
	// Malformed xml.
	EXPECT_FALSE(PHTDescription::isPhilipsDescription("<DataObject ObjectType=\"DPUfsImport\">"));
}

// A description may carry a utf-8 byte order mark.
TEST_F(PHTDescriptionTests, isPhilipsDescriptionAcceptsBomPrefixedMetadata) {
	EXPECT_TRUE(PHTDescription::isPhilipsDescription("\xEF\xBB\xBF" + MockSVSSlide::fakeXML));
}
```

- [ ] **Step 2: Run the tests to verify they do not compile yet**

Run: `cmake --build build --config Release --target slideio_phtiff_tests`
Expected: FAIL to compile — `isPhilipsDescription` is not a member of `PHTDescription`. This is the "test errors" case of the TDD cycle: proceed to step 3, then get a real failure only if the implementation is wrong.

- [ ] **Step 3: Declare the constant and the method**

In `src/slideio/drivers/svs/phtdescription.hpp`, add to the object type constants at the bottom of the namespace, next to `SCANNED_IMAGE`:

```cpp
	const std::string DP_UFS_IMPORT = "DPUfsImport";
```

and add to the public section of `PHTDescription`, after the constructor block:

```cpp
        // True if the text is the xml metadata of a philips tiff file. Used to tell a
        // philips file from any other tiff, which the *.tif extension cannot do.
        static bool isPhilipsDescription(const std::string& description);
```

- [ ] **Step 4: Implement it**

In `src/slideio/drivers/svs/phtdescription.cpp`, add after the constructor definitions:

```cpp
bool PHTDescription::isPhilipsDescription(const std::string& description) {
    // The cheap search comes first: it rejects the description of any other tiff flavour
    // without building a dom, and a philips description can be large (844 KB in
    // Philips-2.tiff, which embeds the macro image as base64).
    if (description.find(DP_UFS_IMPORT) == std::string::npos) {
        return false;
    }
    tinyxml2::XMLDocument doc;
    if (doc.Parse(description.c_str(), description.size()) != tinyxml2::XML_SUCCESS) {
        return false;
    }
    // A document with no root element parses without error; it is not philips metadata.
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr || root->Name() == nullptr || std::strcmp(root->Name(), DATA_OBJECT_TAG) != 0) {
        return false;
    }
    const char* objectType = root->Attribute(OBJECT_TYPE_PROPERTY);
    return objectType != nullptr && DP_UFS_IMPORT == objectType;
}
```

`<cstring>` is already included by `phtdescription.cpp`.

- [ ] **Step 5: Run the tests to verify they pass**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*isPhilipsDescription*"
```
Expected: 3 tests PASS.

- [ ] **Step 6: Run both suites for regressions**

Run:
```bash
./build/bin/Release/slideio_phtiff_tests.exe --gtest_brief=1
```
Expected: all pass (49 before this task, 52 after).

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/svs/phtdescription.hpp src/slideio/drivers/svs/phtdescription.cpp src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "add PHTDescription::isPhilipsDescription to identify philips metadata"
```

---

### Task 2: Driver id constants

Pure refactor, no behaviour change. Task 3 adds a fourth place that compares the driver id, which is what makes the literal worth naming.

**Files:**
- Modify: `src/slideio/drivers/svs/svsimagedriver.hpp`
- Modify: `src/slideio/drivers/svs/svsimagedriver.cpp:8,31`
- Modify: `src/slideio/drivers/svs/svsslide.cpp:228` (`if (driverId == "PHTIFF")`)
- Modify: `src/slideio/drivers/svs/svstiledscene.cpp:38` (`if (m_driverId == "PHTIFF")`)
- Modify: `src/slideio/slideio/imagedrivermanager.cpp:87` (`const std::string phDriverId = "PHTIFF";`)
- Test: none of its own; the existing suites are the guard.

**Interfaces:**
- Consumes: nothing.
- Produces: `slideio::SVS_DRIVER_ID` and `slideio::PHTIFF_DRIVER_ID`, both `constexpr const char*`, declared in `svsimagedriver.hpp`. Tasks 3 and 4 use them, as do the tests.

- [ ] **Step 1: Declare the constants**

In `src/slideio/drivers/svs/svsimagedriver.hpp`, inside `namespace slideio`, above the class:

```cpp
    // SVSImageDriver serves two formats that share the tiff reading code: the aperio svs
    // format and the philips tiff format. The id decides which one an instance reads.
    constexpr const char* SVS_DRIVER_ID = "SVS";
    constexpr const char* PHTIFF_DRIVER_ID = "PHTIFF";
```

- [ ] **Step 2: Use them at every comparison site**

`src/slideio/drivers/svs/svsimagedriver.cpp` — the constructor default and `getFileSpecs`:

```cpp
slideio::SVSImageDriver::SVSImageDriver(const std::string& driverId)
    : m_driverId(driverId)
{
}
```
```cpp
std::string slideio::SVSImageDriver::getFileSpecs() const
{
	static std::string svsPattern("*.svs");
	static std::string philipsPattern("*.tif;*.tiff");
	if (m_driverId == SVS_DRIVER_ID) {
		return svsPattern;
	} 
	return philipsPattern;
}
```

In `src/slideio/drivers/svs/svsimagedriver.hpp`, change the constructor declaration default:

```cpp
        SVSImageDriver(const std::string& driverId = SVS_DRIVER_ID);
```

`src/slideio/drivers/svs/svsslide.cpp`, in `openFile`:

```cpp
    if (driverId == PHTIFF_DRIVER_ID) {
```
Add the include at the top of the file, after the `svsslide.hpp` include:
```cpp
#include "slideio/drivers/svs/svsimagedriver.hpp"
```

`src/slideio/drivers/svs/svstiledscene.cpp`, in `processImageDescription`:

```cpp
    if (m_driverId == PHTIFF_DRIVER_ID) {
```
Add the include next to the existing `phtdescription.hpp` include:
```cpp
#include "slideio/drivers/svs/svsimagedriver.hpp"
```

`src/slideio/slideio/imagedrivermanager.cpp`, in `initialize`:

```cpp
        {
            auto driver = std::make_shared<SVSImageDriver>(PHTIFF_DRIVER_ID);
            driverMap[driver->getID()] = driver;
        }
```

- [ ] **Step 3: Build and verify nothing changed**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
cmake --build build --config Release --target slideio_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_brief=1
./build/bin/Release/slideio_tests.exe --gtest_brief=1
```
Expected: 52 pass in the phtiff suite, 483 in the main suite. A refactor that changes a test result is a mistake — stop and find it.

- [ ] **Step 4: Commit**

```bash
git add src/slideio/drivers/svs/svsimagedriver.hpp src/slideio/drivers/svs/svsimagedriver.cpp src/slideio/drivers/svs/svsslide.cpp src/slideio/drivers/svs/svstiledscene.cpp src/slideio/slideio/imagedrivermanager.cpp
git commit -m "name the svs and philips tiff driver ids"
```

---

### Task 3: `SVSImageDriver::canOpenFile`

The content test. After this task the driver stops claiming every TIFF, which is what allows Task 4 to add it to the detection order.

**Files:**
- Modify: `src/slideio/drivers/svs/svsimagedriver.hpp`
- Modify: `src/slideio/drivers/svs/svsimagedriver.cpp`
- Modify: `src/slideio/imagetools/tiffkeeper.cpp:16-19` (the path constructor)
- Test: `src/tests/phtiff/test_phtiff_driver.cpp` — rewrite `PhTiffImageDriverTests.canOpenFile`

**Interfaces:**
- Consumes: `PHTDescription::isPhilipsDescription` (Task 1), `PHTIFF_DRIVER_ID` and `SVS_DRIVER_ID` (Task 2), `TIFFKeeper::readStringTag(uint16_t)` and `TIFFKeeper::isValid()` from `slideio/imagetools/tiffkeeper.hpp`.
- Produces: `bool SVSImageDriver::canOpenFile(const std::string& filePath) const override;` — Task 4 relies on it returning false for non-Philips TIFFs.

- [ ] **Step 1: Rewrite the failing test**

Replace the whole `TEST_F(PhTiffImageDriverTests, canOpenFile)` body in `src/tests/phtiff/test_phtiff_driver.cpp`. The old test asserts that paths which do not exist are openable, which a content test contradicts by design.

```cpp
TEST_F(PhTiffImageDriverTests, canOpenFile) {
	// The extension is necessary but not sufficient: philips shares *.tif;*.tiff with
	// gdal and with ome-tiff, so the driver has to look into the file.
	SVSImageDriver driver(PHTIFF_DRIVER_ID);
	EXPECT_TRUE(driver.canOpenFile(TestTools::getFullTestImagePath("philips", "Philips-3.tiff")));
	EXPECT_FALSE(driver.canOpenFile(
		TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "multipage.tif")));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getFullTestImagePath("ometiff", "00001_01.ome.tiff")));
	// A path that does not exist, and a file that is not a tiff at all.
	EXPECT_FALSE(driver.canOpenFile("/projects/no-such-file.tiff"));
	EXPECT_FALSE(driver.canOpenFile(TestTools::getTestImagePath("gdal", "colors.png")));

	// Extensions the driver does not serve are rejected without opening anything.
	const std::string disallowedSuffixes[] = {
		".ometif", ".ometiff", ".ometf2", ".ometf8", ".omebtf", ".svs", ".ndpi", ".qptiff"
	};
	for (const std::string& suffix : disallowedSuffixes) {
		EXPECT_FALSE(driver.canOpenFile("/projects/image" + suffix)) << suffix;
	}
	// Both cases of the served extensions reach the content test.
	const std::string allowedSuffixes[] = { ".tif", ".tiff", ".TIF", ".TIFF" };
	for (const std::string& suffix : allowedSuffixes) {
		// The file does not exist, so the content test fails -- but the extension passed,
		// which is what distinguishes this from the block above.
		EXPECT_FALSE(driver.canOpenFile("/projects/image" + suffix)) << suffix;
	}

	// The svs id keeps deciding by extension alone: an svs file has no philips metadata.
	SVSImageDriver svsDriver(SVS_DRIVER_ID);
	EXPECT_TRUE(svsDriver.canOpenFile("/projects/image.svs"));
	EXPECT_TRUE(svsDriver.canOpenFile(TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs")));
	EXPECT_FALSE(svsDriver.canOpenFile("/projects/image.tiff"));
}
```

- [ ] **Step 2: Run it to verify it fails**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*canOpenFile*"
```
Expected: FAIL. The `EXPECT_FALSE` lines for the gdal `.tif`, `multipage.tif` and the `.ome.tiff` all report true, because the base implementation accepts any matching extension.

- [ ] **Step 3: Declare the override**

In `src/slideio/drivers/svs/svsimagedriver.hpp`, in the public section:

```cpp
        bool canOpenFile(const std::string& filePath) const override;
```

- [ ] **Step 4: Implement it**

In `src/slideio/drivers/svs/svsimagedriver.cpp`, add the includes:

```cpp
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <exception>
```

and the method:

```cpp
bool slideio::SVSImageDriver::canOpenFile(const std::string& filePath) const
{
	if (!ImageDriver::canOpenFile(filePath)) {
		return false;
	}
	if (m_driverId != PHTIFF_DRIVER_ID) {
		// The aperio format has an extension of its own.
		return true;
	}
	// *.tif and *.tiff say nothing: gdal reads plain tiff files and the ome-tiff driver
	// reads its own flavour. Only the metadata in the description of the first directory
	// identifies a philips file.
	try {
		TIFFKeeper keeper(filePath);
		if (!keeper.isValid()) {
			return false;
		}
		return PHTDescription::isPhilipsDescription(keeper.readStringTag(TIFFTAG_IMAGEDESCRIPTION));
	}
	catch (const std::exception&) {
		// A missing file or one that is not a tiff at all: not ours, and not an error
		// worth propagating out of format detection.
		return false;
	}
}
```

`TIFFKeeper` opens the file in its constructor and closes it in its destructor, and `readStringTag` returns an empty string when the tag is absent. `TIFFTAG_IMAGEDESCRIPTION` is a libtiff macro, available through `tiffkeeper.hpp`.

- [ ] **Step 5: Silence libtiff for probed files**

`TIFFKeeper(libtiff::TIFF*)` installs a `TIFFMessageHandler`, which swaps libtiff's warning and error handlers; the path constructor does not, so probing a file that is not a TIFF lets libtiff write to stderr. In `src/slideio/imagetools/tiffkeeper.cpp`:

```cpp
TIFFKeeper::TIFFKeeper(const std::string& filePath, bool readOnly)
{
    m_messageHandler = std::make_shared<TIFFMessageHandler>();
    openTiffFile(filePath, readOnly);
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*canOpenFile*"
```
Expected: PASS. One `E...exceptions.cpp` log line for the non-existent path and the `.png` is expected: `TiffTools::openTiffFile` raises and logs before the catch. It is noise, not a failure.

- [ ] **Step 7: Run both suites for regressions**

Run:
```bash
cmake --build build --config Release --target slideio_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_brief=1
./build/bin/Release/slideio_tests.exe --gtest_brief=1
```
Expected: 52 and 483 pass. `TIFFKeeper` is used by the converter and several drivers, so the main suite matters here.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/drivers/svs/svsimagedriver.hpp src/slideio/drivers/svs/svsimagedriver.cpp src/slideio/imagetools/tiffkeeper.cpp src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "identify philips tiff files by their metadata in canOpenFile"
```

---

### Task 4: PHTIFF in the detection order

**Files:**
- Modify: `src/slideio/slideio/imagedrivermanager.cpp:58` (the `driverOrder` array)
- Test: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: `SVSImageDriver::canOpenFile` (Task 3), `PHTIFF_DRIVER_ID` (Task 2).
- Produces: nothing for later tasks; this is the last one.

- [ ] **Step 1: Write the failing tests**

Add to `src/tests/phtiff/test_phtiff_driver.cpp`, after the `auxImagesOfTheTestFiles` test:

```cpp
// Detection routing for tiff files. The philips row is the order guard: gdal claims
// *.tif;*.tiff by extension alone, so it would take the philips file first if philips
// came after it in the order. The ome-tiff row asserts routing only -- it holds
// whichever side of the ome-tiff driver philips sits on, because philips rejects
// ome-xml by content.
TEST_F(PhTiffImageDriverTests, findDriver) {
	const std::list<std::pair<std::string, std::string>> expected = {
		{"PHTIFF", TestTools::getFullTestImagePath("philips", "Philips-3.tiff")},
		{"GDAL", TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif")},
		{"GDAL", TestTools::getTestImagePath("gdal", "multipage.tif")},
		{"OMETIFF", TestTools::getFullTestImagePath("ometiff", "00001_01.ome.tiff")},
		{"SVS", TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs")},
	};
	for (const auto& param : expected) {
		auto driver = ImageDriverManager::findDriver(param.second);
		ASSERT_TRUE(driver != nullptr) << param.second;
		EXPECT_EQ(param.first, driver->getID()) << param.second;
	}
}

// The whole point of the detection: a philips file opens without naming a driver, and
// it opens as a philips slide. The public Slide class exposes no driver id, so the
// assertions are on what only the philips driver produces -- gdal hands back 11 scenes,
// one per tiff directory, with no auxiliary images and no resolution.
TEST_F(PhTiffImageDriverTests, openSlideWithoutDriverId) {
	const std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath);
	ASSERT_TRUE(slide != nullptr);
	EXPECT_EQ(1, slide->getNumScenes());
	EXPECT_EQ((std::list<std::string>{"Label", "Macro"}), slide->getAuxImageNames());
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	const auto res = scene->getResolution();
	EXPECT_DOUBLE_EQ(0.000226891e-3, std::get<0>(res));
	EXPECT_DOUBLE_EQ(0.000226907e-3, std::get<1>(res));
}
```

`Slide::getAuxImageNames()` returns `const std::list<std::string>&`, and the names come
out of a `std::map`, so they are sorted: `Label` before `Macro`.

- [ ] **Step 2: Run them to verify they fail**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*findDriver*:*openSlideWithoutDriverId*"
```
Expected: both FAIL. `findDriver` returns GDAL for the Philips file, and
`openSlideWithoutDriverId` gets a GDAL slide with no auxiliary images. If GDAL cannot
open the file at all, the second test fails by throwing instead — also a valid red.

- [ ] **Step 3: Add PHTIFF to the order**

In `src/slideio/slideio/imagedrivermanager.cpp`, in `findDriver`:

```cpp
    std::string driverOrder[] = { "OMETIFF", "SVS", "CZI", "AFI", "SCN", "DCM", "ZVI", "NDPI", "VSI", "QPTIFF", "PHTIFF", "GDAL" };
```

PHTIFF goes second to last, immediately before GDAL. That is the load-bearing part: GDAL claims `*.tif;*.tiff` by extension alone, so it would take a Philips file first if PHTIFF came after it. Keeping PHTIFF after OMETIFF matches the design's reading order, but it is not what makes OME-TIFF safe — PHTIFF's content test rejects OME-XML, so an OME-TIFF routes to OMETIFF from either position.

- [ ] **Step 4: Run them to verify they pass**

Run:
```bash
cmake --build build --config Release --target slideio_phtiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*findDriver*:*openSlideWithoutDriverId*"
```
Expected: PASS.

- [ ] **Step 5: Run every suite that touches detection**

Run:
```bash
cmake --build build --config Release --target slideio_tests
cmake --build build --config Release --target slideio_ometiff_tests
./build/bin/Release/slideio_phtiff_tests.exe --gtest_brief=1
./build/bin/Release/slideio_tests.exe --gtest_brief=1
./build/bin/Release/slideio_ometiff_tests.exe --gtest_brief=1
```
Expected: all pass. `slideio_tests` contains `ImageDriverManager.findDriver`, whose table must be unaffected, and the ome-tiff suite covers the driver that now shares the extension space with an active PHTIFF.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/slideio/imagedrivermanager.cpp src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "detect philips tiff files automatically"
```

---

## Verification of the whole change

- [ ] `./build/bin/Release/slideio_phtiff_tests.exe` — all pass
- [ ] `./build/bin/Release/slideio_tests.exe` — 483 pass
- [ ] `./build/bin/Release/slideio_ometiff_tests.exe` — all pass
- [ ] `./build/bin/Release/slideio_pke_tests.exe` and `slideio_ndpi_tests.exe` — all pass; both drivers read TIFF variants and both use `TIFFKeeper`
- [ ] `git log --oneline -4` shows the four commits, working tree clean
