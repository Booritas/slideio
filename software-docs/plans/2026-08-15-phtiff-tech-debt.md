# Philips TIFF Tech Debt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the eight remaining Philips TIFF follow-ups recorded in section 2 of the tech debt log — the hardening the v2.9.0 branch review deliberately left, plus the untested invariants underneath the tile-padding crop.

**Architecture:** Four correctness changes to the Philips read path (`phtmetadata.cpp`, `phtiffslide.cpp`), one structural change moving the driver ids out of the driver header, and one batch of small cleanups. No public API changes except the removal of one unused `Tools` utility.

**Tech Stack:** C++17, tinyxml2, OpenCV, libtiff, GoogleTest, CMake, Conan.

**Spec:** `software-docs/TECH_DEBT.md` section 2, *Philips TIFF driver follow-ups*. Each numbered item there records the problem and the reasoning; this plan implements items 1-4 and 6-9, and the surviving half of item 5.

## Global Constraints

- Build: `python install.py -a build-only -c release` from the repo root. Run a suite: `./build/bin/Release/<name>.exe` (Windows) or `./build/release/bin/<name>` (Linux/macOS).
- Baseline before this plan starts: `slideio_phtiff_tests` 97, `slideio_tests` 489, `slideio_pke_tests` 15, `slideio_ometiff_tests` 98, `slideio_ndpi_tests` 29, `slideio_vsi_tests` 30, `slideio_converter_tests` 140, `slideio_transformer_tests` 39. Tests are ADDED by this plan; no existing test may change its assertions. If an existing assertion starts failing, STOP and report — it means a change altered behaviour that was not meant to change.
- The private dataset must be enabled (`SLIDEIO_TEST_DATA_PRIV_PATH` set), or the real-file tests skip and the net has holes.
- C++17. Every file keeps its three-line licence header. Comments explain *why*, matching the density and voice of the surrounding code.
- The Philips code lives in `src/slideio/drivers/svs/`: `phtiffslide.{hpp,cpp}`, `phtiffscene.{hpp,cpp}`, `phtmetadata.{hpp,cpp}`, `phtiffimagedriver.{hpp,cpp}`, `phtdescription.{hpp,cpp}`. The Philips metadata constants live in `namespace slideio::phtiff`; the `.cpp` files reach them through `using namespace slideio::phtiff;`.

---

## File Map

**Modify:**
- `src/slideio/drivers/svs/phtmetadata.cpp` — tolerate non-numeric attribute values (Task 1).
- `src/slideio/drivers/svs/phtiffslide.cpp` — crop guards (Task 2), two-pass level matching (Task 4).
- `src/slideio/drivers/svs/svstools.{hpp,cpp}` — add `extractPhilipsLevelNumber` (Task 4).
- `src/slideio/drivers/svs/svsslide.cpp` — delete a dead include (Task 5).
- `src/slideio/drivers/svs/phtiffslide.cpp`, `phtiffscene.cpp`, `phtiffimagedriver.hpp`, `svsimagedriver.hpp` — driver ids move (Task 5).
- `src/slideio/imagetools/tiffkeeper.hpp` — document the global handler swap (Task 6).
- `src/slideio/core/tools/tools.{hpp,cpp}` — remove `isXml` (Task 6).
- `src/tests/phtiff/test_phtiff_driver.cpp` — new tests throughout; id constants (Task 6).
- `src/tests/main/test_tools.cpp` — remove the `isXml` test (Task 6).
- `software-docs/TECH_DEBT.md` — strike the items as they land (each task).

**Create:**
- `src/slideio/drivers/svs/svsdriverids.hpp` — the two driver id constants (Task 5).

**Not touched:** every driver other than svs; the converter; `phtdescription.{hpp,cpp}`.

---

## Task 1: A non-numeric attribute value no longer aborts the open

Closes the surviving half of tech debt item 5.

**Files:**
- Modify: `src/slideio/drivers/svs/phtmetadata.cpp`
- Test: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Produces: no signature change. `readPHTMetadata` keeps raising only when the *document* cannot be parsed.

- [ ] **Step 1: Write the failing tests**

Add these beside the existing `readPHTMetadata` tests. `phRemoveAttribute` and `MockPHTIFFSlide::createFakeXml` already exist in this file; `phSetAttributeValue` is new and defined in the same step.

```cpp
// Replaces the text of the `occurrence`-th (0 based) <Attribute> with the given name.
// Used to plant a value of the wrong type, which is what a scanner writing an empty or
// corrupt field produces in practice.
static std::string phSetAttributeValue(const std::string& xml, std::string_view name,
	int occurrence, const std::string& value) {
	const std::string needle = "Name=\"" + std::string(name) + "\"";
	size_t position = 0;
	for (int found = 0; found <= occurrence; ++found) {
		position = xml.find(needle, (found == 0) ? 0 : position + needle.size());
		if (position == std::string::npos) {
			return xml;
		}
	}
	const size_t open = xml.find('>', position);
	const size_t close = xml.find("</Attribute>", open);
	if (open == std::string::npos || close == std::string::npos) {
		return xml;
	}
	return xml.substr(0, open + 1) + value + xml.substr(close);
}

// A level number that is present but not a number cannot place the level, exactly as a
// missing one cannot. It costs the caller that level, not the slide.
TEST_F(PhTiffImageDriverTests, readPHTMetadataSkipsALevelWhoseNumberIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_NUMBER.Name, 1, "not a number");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(1u, wsi->levels.size()) << "the level with the unreadable number is dropped";
	EXPECT_EQ(0, wsi->levels[0].number);
}

// A size that will not parse costs the level its size, not its place in the pyramid: a
// level with no declared size is already handled, by positional fallback.
TEST_F(PhTiffImageDriverTests, readPHTMetadataKeepsALevelWhoseSizeIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), LEVEL_COLUMNS.Name, 1, "");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	ASSERT_EQ(2u, wsi->levels.size());
	EXPECT_EQ(0, wsi->levels[1].declaredSize.width) << "unreadable size left at its default";
	EXPECT_EQ(1, wsi->levels[1].number) << "the level itself survives";
}

// The same for the image's own dimensions.
TEST_F(PhTiffImageDriverTests, readPHTMetadataKeepsAnImageWhoseSizeIsNotANumber) {
	const std::string xml = phSetAttributeValue(
		MockPHTIFFSlide::createFakeXml(1024, 768, 2), IMAGE_COLUMNS.Name, 0, "wide");
	const PHTMetadata metadata = readPHTMetadata(xml);
	const PHTImageDeclaration* wsi = metadata.wholeSlideImage();
	ASSERT_TRUE(wsi != nullptr);
	EXPECT_EQ(0, wsi->size.width) << "unreadable size left at its default";
	EXPECT_EQ(WSI, wsi->type) << "the image itself survives";
}
```

- [ ] **Step 2: Run them to verify they fail**

Run: `./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*NotANumber*"`
Expected: all three FAIL by throwing `slideio::RuntimeError` out of the test body — "cannot convert the value ... to an integer".

- [ ] **Step 3: Make the integer reads tolerant**

In `phtmetadata.cpp`, add this helper to the anonymous namespace, above `phReadLevels`:

```cpp
    // Reads an integer attribute, or reports why it could not and leaves the caller's
    // value alone. A value that is present but not an integer is a scanner's mistake in
    // one field; it must not cost the caller the whole slide, which is what letting
    // getAttributeInt raise through readPHTMetadata would do.
    bool phReadInt(PHTDescription& philips, const tinyxml2::XMLElement* element,
                   const PHTDescription::Attribute& attribute, int& value) {
        if (!philips.hasAttribute(element, attribute)) {
            return false;
        }
        try {
            value = philips.getAttributeInt(element, attribute);
            return true;
        }
        catch (const std::exception& error) {
            SLIDEIO_LOG(WARNING) << "PHTIFF: the philips attribute " << attribute.Name
                << " is not readable as a number and is ignored: " << error.what();
            return false;
        }
    }
```

Then route the five integer reads through it. In `phReadLevels`, replace the `hasAttribute(level, LEVEL_NUMBER)` guard and the reads under it with:

```cpp
            // The level number is what says how much of the slide a level covers, so a
            // level whose number is missing or unreadable cannot be placed in the pyramid
            // at all. It is dropped and the rest of the pyramid is kept, rather than the
            // file being refused over one incomplete declaration.
            PHTLevelDeclaration declared;
            if (!phReadInt(philips, level, LEVEL_NUMBER, declared.number)) {
                SLIDEIO_LOG(WARNING) << "PHTIFF: a zoom level of the philips file declares"
                    " no usable level number. The level is ignored.";
                continue;
            }
            int columns = 0;
            int rows = 0;
            if (phReadInt(philips, level, LEVEL_COLUMNS, columns)
                && phReadInt(philips, level, LEVEL_ROWS, rows)) {
                declared.declaredSize = {columns, rows};
            }
```

and in `readPHTMetadata`, replace the image size read with:

```cpp
        int columns = 0;
        int rows = 0;
        if (phReadInt(philips, image, IMAGE_COLUMNS, columns)
            && phReadInt(philips, image, IMAGE_ROWS, rows)) {
            declared.size = {columns, rows};
        }
```

- [ ] **Step 4: Run the new tests and the whole phtiff suite**

Run: `./build/bin/Release/slideio_phtiff_tests.exe`
Expected: 100 tests, all passing. `phExtractImagesSkipsAZoomLevelWithoutANumber` must still pass — the missing-attribute path now runs through `phReadInt`'s `hasAttribute` check and must behave identically.

- [ ] **Step 5: Run the main suite**

Run: `./build/bin/Release/slideio_tests.exe`
Expected: 489 passing.

- [ ] **Step 6: Strike the item and commit**

In `software-docs/TECH_DEBT.md` section 2, replace item 5's body with a one-line `**5. ~~A non-numeric attribute value still aborts the whole slide open.~~ Fixed** (`phReadInt` in `phtmetadata.cpp`).` and keep the heading.

```bash
git add src/slideio/drivers/svs/phtmetadata.cpp src/tests/phtiff/test_phtiff_driver.cpp software-docs/TECH_DEBT.md
git commit -m "keep a philips slide readable when one attribute value is not a number"
```

---

## Task 2: Guard the two invariants `phCropLevelPadding` relies on

Closes tech debt items 1 and 4. Both are in the same function and share a test cycle.

**Files:**
- Modify: `src/slideio/drivers/svs/phtiffslide.cpp`
- Test: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: no signature change.

- [ ] **Step 1: Write the failing test**

The crop is only safe because shrinking a directory to its content size leaves the tile count unchanged. Add:

```cpp
// phCropLevelPadding rewrites a level's dimensions but not its tile layout, so the crop
// is only safe while the content occupies the same number of tiles as the stored raster.
// Philips pads each level to its own tile grid, which makes that true on every real file
// -- but if it were ever false the tile indices would skew silently: wrong pixels, no
// error. Here level 1 is stored 1024 wide with 512-pixel tiles (2 tile columns) while its
// content is 512 (1 tile column), so cropping would change the tile count and must be
// refused, leaving the level at its stored size.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_refusesACropThatWouldChangeTheTileCount) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(1024, 1024, 2, {});
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 1024, 1024),
		// Stored twice the size of its content, which no real philips file does -- the
		// point is the guard, not the layout.
		makeImageDir("level=1 mag=20 quality=80", 1024, 1024),
	};
	directories[1].tileWidth = 512;
	directories[1].tileHeight = 512;
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, true} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const LevelInfo* info = slide.getScene(0)->getZoomLevelInfo(1);
	ASSERT_TRUE(info != nullptr);
	EXPECT_EQ(1024, info->getSize().width)
		<< "content 512 is 1 tile but stored 1024 is 2, so the crop must be refused";
	EXPECT_EQ(1024, info->getSize().height);
}
```

Without the guard this test fails with 512 against the expected 1024 — the crop goes through and the level reports a size whose tile count no longer matches its stored layout.

- [ ] **Step 2: Add the two guards**

In `phCropLevelPadding` in `phtiffslide.cpp`, add the argument check at the top, immediately after the `imagePyramid.empty()` early return:

```cpp
        // The caller is trusted to pass parallel ranges -- one directory per pyramid
        // level, in the same order. Checking it here costs one comparison and turns a
        // future caller's mistake into a refusal instead of an out-of-bounds read.
        if (dirs.size() != imagePyramid.size()) {
            SLIDEIO_LOG(WARNING) << "PHTIFF: the zoom level list and the directory list"
                " have different lengths (" << imagePyramid.size() << " and " << dirs.size()
                << "). Tile padding is not cropped.";
            return;
        }
```

and the tile-count invariant immediately before the two assignments at the end of the loop body:

```cpp
            // Shrinking the directory does not change its tile layout, so the crop is only
            // safe while the content occupies the same number of tiles as the stored
            // raster. Philips pads every level to its own tile grid, which makes this hold
            // on every real file; if it ever did not, the tile indices would skew and the
            // level would read the wrong pixels with no error at all.
            if (dir.tileWidth > 0 && dir.tileHeight > 0) {
                const int storedTilesX = (dir.width - 1) / dir.tileWidth + 1;
                const int storedTilesY = (dir.height - 1) / dir.tileHeight + 1;
                const int contentTilesX = (contentSize.width - 1) / dir.tileWidth + 1;
                const int contentTilesY = (contentSize.height - 1) / dir.tileHeight + 1;
                if (storedTilesX != contentTilesX || storedTilesY != contentTilesY) {
                    SLIDEIO_LOG(WARNING) << "PHTIFF: cropping philips zoom level " << levelNumber
                        << " from " << dir.width << "x" << dir.height << " to "
                        << contentSize.width << "x" << contentSize.height
                        << " would change its tile count. The level is not cropped.";
                    continue;
                }
            }
```

- [ ] **Step 3: Prove both guards bite**

Temporarily change `dirs.size() != imagePyramid.size()` to `false` and re-run the phtiff suite: no test should fail, because no current caller violates it — record that, then restore the guard. This one is insurance, not a bug fix, and the plan says so rather than pretending a test covers it.

For the tile-count guard, temporarily invert its condition to `storedTilesX == contentTilesX && storedTilesY == contentTilesY` and re-run `phCreateImageScene_cropsTilePaddingOfZoomLevels`: it must FAIL, proving the guard is on the crop path and reachable. Restore it afterwards.

- [ ] **Step 4: Run the suites**

Run `./build/bin/Release/slideio_phtiff_tests.exe` (101 passing) and `./build/bin/Release/slideio_tests.exe` (489 passing).

- [ ] **Step 5: Strike the items and commit**

Mark items 1 and 4 struck through in `software-docs/TECH_DEBT.md` with a pointer to `phCropLevelPadding`.

```bash
git add src/slideio/drivers/svs/phtiffslide.cpp src/tests/phtiff/test_phtiff_driver.cpp software-docs/TECH_DEBT.md
git commit -m "guard the two invariants the philips tile padding crop relies on"
```

---

## Task 3: Pin the rounding rule of `phLevelContentSize`

Closes tech debt item 3. Test-only — no production change.

**Files:**
- Test: `src/tests/phtiff/test_phtiff_driver.cpp`
- Modify: `src/slideio/drivers/svs/phtiffslide.cpp` (comment only)

**Interfaces:** none.

- [ ] **Step 1: Write the test**

Every real Philips base size is a multiple of the 512 tile grid, so `base / 2^level` divides exactly and the `ceil` in `phLevelContentSize` never rounds — on real files or in any existing test. Rounding up versus down is therefore an untested decision. Pin it:

```cpp
// A base width that does not divide by the level's downsample is the only case where the
// rounding rule is observable, and no real philips file has one: every real base is a
// multiple of the 512 tile grid. Rounding UP is what keeps the level able to hold the
// whole slide -- 4098 pixels halve to 2049, and a level of 2048 would drop the last
// column. This test exists because that decision is otherwise never exercised.
TEST_F(PhTiffImageDriverTests, phCreateImageScene_roundsAContentSizeUpNotDown) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(4098, 4098, 2, {});
	std::vector<TiffDirectory> directories = {
		makeImageDir(xml, 4098, 4098),
		// Padded to the tile grid by philips; the content is ceil(4098/2) = 2049.
		makeImageDir("level=1 mag=20 quality=80", 2560, 2560),
	};
	directories[1].tileWidth = 512;
	directories[1].tileHeight = 512;
	const std::vector<PHTLevel> imagePyramid = { {0, 0, true}, {1, 1, true} };

	MockPHTIFFSlide slide;
	slide.createImageSceneMock(directories, imagePyramid, nullptr);

	ASSERT_EQ(1, slide.getNumScenes());
	const LevelInfo* info = slide.getScene(0)->getZoomLevelInfo(1);
	ASSERT_TRUE(info != nullptr);
	EXPECT_EQ(2049, info->getSize().width) << "ceil(4098/2), not floor";
	EXPECT_EQ(2049, info->getSize().height);
}
```

Check the tile arithmetic before running: stored 2560 with 512-pixel tiles is 5 tiles; content 2049 is `ceil(2049/512)` = 5 tiles. The counts agree, so Task 2's guard does not refuse this crop. If you get a different stored size, adjust it so the two tile counts match, and say so in your report.

- [ ] **Step 2: Run it**

Run: `./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*roundsAContentSizeUp*"`
Expected: PASS — the production behaviour is already correct; this test records it.

- [ ] **Step 3: Prove the test bites**

Temporarily change `phLevelContentSize` to use integer division (`baseSize.width / divisor`) and re-run: the test must FAIL with 2048 against the expected 2049. Restore the `ceil` afterwards. A test that has never failed proves nothing, and this one exists precisely to pin a decision no other test touches.

- [ ] **Step 4: Record the reasoning where the code is**

Extend the comment on `phLevelContentSize` in `phtiffslide.cpp` to say why up rather than down:

```cpp
    // A philips zoom level covers the same area as the base level downsampled by
    // 2^levelNumber, rounded UP to a whole pixel: a level that rounded down would be one
    // column or row short of holding the whole slide. No real file exercises this -- every
    // real base is a multiple of the 512 tile grid, so the division is exact -- which is
    // why phCreateImageScene_roundsAContentSizeUpNotDown exists to pin it.
```

- [ ] **Step 5: Run the suites, strike the item and commit**

Run both suites (102 and 489 passing).

```bash
git add src/slideio/drivers/svs/phtiffslide.cpp src/tests/phtiff/test_phtiff_driver.cpp software-docs/TECH_DEBT.md
git commit -m "pin the rounding rule of the philips level content size"
```

---

## Task 4: Match a level to its directory by the level number the directory declares

Closes tech debt item 2, the narrow case where an identically-sized interloper can claim a declared level and get cropped as if it were real.

**Files:**
- Modify: `src/slideio/drivers/svs/svstools.hpp`, `svstools.cpp`
- Modify: `src/slideio/drivers/svs/phtiffslide.cpp`
- Test: `src/tests/phtiff/test_phtiff_driver.cpp`, `src/tests/main/test_svs_tools.cpp`

**Interfaces:**
- Produces: `static int SVSTools::extractPhilipsLevelNumber(const std::string& description)` — the level number a philips level directory's description names, or `-1` if it names none.

- [ ] **Step 1: Write the failing test for the parser**

In `src/tests/main/test_svs_tools.cpp`, beside the existing `extractPhilipsMagnification` tests:

```cpp
// A philips level directory names its own level: "level=1 mag=20 quality=80". The base
// level's directory carries the xml metadata instead and names none.
TEST(SVSTools, extractPhilipsLevelNumberReadsTheLevel)
{
	EXPECT_EQ(1, slideio::SVSTools::extractPhilipsLevelNumber("level=1 mag=20 quality=80"));
	EXPECT_EQ(8, slideio::SVSTools::extractPhilipsLevelNumber("level=8 mag=0.15625 quality=80"));
}

TEST(SVSTools, extractPhilipsLevelNumberReturnsMinusOneWhenThereIsNone)
{
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(""));
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber("interloper"));
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(
		"Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)"));
	// The aperio syntax names no philips level.
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(description));
	// "levels=" is not "level=" -- the derivation description of a converted file
	// contains "levels=10003,10002" and must not be read as a level number.
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber("levels=10003,10002 mag=20"));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `./build/bin/Release/slideio_tests.exe --gtest_filter="SVSTools.extractPhilipsLevelNumber*"`
Expected: compile failure — `extractPhilipsLevelNumber` is not a member of `SVSTools`.

- [ ] **Step 3: Add the parser**

In `svstools.hpp`, beside `extractPhilipsMagnification`:

```cpp
        // The zoom level a philips level directory's description names
        // ("level=1 mag=20 quality=80" -> 1), or -1 if it names none. The base level's
        // directory carries the xml metadata instead and always returns -1.
        static int extractPhilipsLevelNumber(const std::string& description);
```

In `svstools.cpp`, beside `extractPhilipsMagnification`:

```cpp
int SVSTools::extractPhilipsLevelNumber(const std::string& description)
{
    // The same shape the magnification parser trusts, and deliberately not a bare
    // "level=(\d+)": the derivation description of a converted file contains
    // "levels=10003,10002", and requiring the mag field keeps that from matching.
    std::regex rgx(R"(level=(\d{1,9})\s+mag=)");
    std::smatch match;
    if (!std::regex_search(description, match, rgx)) {
        return -1;
    }
    return std::stoi(match[1]);
}
```

- [ ] **Step 4: Run the parser tests**

Run: `./build/bin/Release/slideio_tests.exe --gtest_filter="SVSTools.*"`
Expected: 14 passing.

- [ ] **Step 5: Write the failing test for the mis-binding**

In `src/tests/phtiff/test_phtiff_driver.cpp`:

```cpp
// Two declared levels can share a size -- a small slide whose base already sits on the
// tile grid comes out 1x1 at both of its lower levels. If an UNDECLARED tiled directory
// of that same size sits between their real directories, matching by size alone lets the
// interloper claim the second declared level, be marked corroborated, and get cropped as
// if it were real, while the real directory is dropped. The directories name their own
// level, so the pairing does not have to be guessed from size.
TEST_F(PhTiffImageDriverTests, phExtractImages_sameSizedInterloperDoesNotClaimADeclaredLevel) {
	const std::string xml = MockPHTIFFSlide::createFakeXml(2, 2, 3, {});
	const std::vector<TiffDirectory> directories = {
		makeLevelDir(xml, 2, 2),                                 // declared level 0
		makeLevelDir("level=1 mag=20 quality=80", 1, 1),         // declared level 1
		makeLevelDir("interloper", 1, 1),                        // undeclared, same size
		makeLevelDir("level=2 mag=10 quality=80", 1, 1),         // declared level 2
	};
	std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;

	MockPHTIFFSlide::extractImagesMock(directories, imagePyramid, auxImages);

	EXPECT_EQ((std::vector<int>{0, 1, 3}), phDirIndices(imagePyramid))
		<< "the interloper at index 2 must not take level 2's place";
	EXPECT_EQ((std::vector<PHTLevel>{{0, 0}, {1, 1}, {3, 2}}), imagePyramid);
}
```

`createFakeXml` clamps each level size to at least 1 pixel, so a base of 2 declares levels of 2, 1 and 1 — the tie this test needs.

- [ ] **Step 6: Run to verify it fails**

Run: `./build/bin/Release/slideio_phtiff_tests.exe --gtest_filter="*sameSizedInterloper*"`
Expected: FAIL — the pyramid comes back as directories `{0, 1, 2}`, the interloper having claimed declared level 2.

- [ ] **Step 7: Match by declared level number first, then by size**

In `phtiffslide.cpp`, replace the single matching loop (the one commented "In file order, pair every tiled directory with the first unclaimed declared level") with two passes:

```cpp
    // Pass one: a level directory names its own level, so pair those first and exactly.
    // Size alone cannot separate two declared levels that share a size, and a directory
    // the metadata never declared can carry that same size -- in which case matching by
    // size hands a real level's place to the interloper and drops the real directory.
    for (size_t dirPos = 0; dirPos < levelDirs.size(); ++dirPos) {
        const TiffDirectory& dir = directories[levelDirs[dirPos]];
        const int named = SVSTools::extractPhilipsLevelNumber(dir.description);
        if (named < 0) {
            continue;
        }
        for (size_t levelIndex = 0; levelIndex < declaredLevels.size(); ++levelIndex) {
            const PHTLevelDeclaration& declared = declaredLevels[levelIndex];
            if (levelClaimed[levelIndex] || declared.number != named) {
                continue;
            }
            // The declared size still has to agree. A directory that names a level but
            // does not match its declared size is a contradiction, and is left to the
            // size pass rather than trusted.
            if (declared.declaredSize.width == dir.width
                && declared.declaredSize.height == dir.height) {
                levelClaimed[levelIndex] = true;
                dirClaimed[dirPos] = true;
                PHTLevel pyramidLevel;
                pyramidLevel.dirIndex = levelDirs[dirPos];
                pyramidLevel.levelNumber = declared.number;
                pyramidLevel.corroborated = true;
                imagePyramid.push_back(pyramidLevel);
            }
            break;
        }
    }

    // Pass two: everything still unpaired, by declared size, in file order. This is what
    // places the base level, whose directory carries the xml metadata and names no level.
    for (size_t dirPos = 0; dirPos < levelDirs.size(); ++dirPos) {
        if (dirClaimed[dirPos]) {
            continue;
        }
        const TiffDirectory& dir = directories[levelDirs[dirPos]];
        for (size_t levelIndex = 0; levelIndex < declaredLevels.size(); ++levelIndex) {
            const PHTLevelDeclaration& declared = declaredLevels[levelIndex];
            if (levelClaimed[levelIndex] || declared.declaredSize.width <= 0
                || declared.declaredSize.height <= 0) {
                continue;
            }
            if (declared.declaredSize.width == dir.width && declared.declaredSize.height == dir.height) {
                levelClaimed[levelIndex] = true;
                dirClaimed[dirPos] = true;
                PHTLevel pyramidLevel;
                pyramidLevel.dirIndex = levelDirs[dirPos];
                pyramidLevel.levelNumber = declared.number;
                pyramidLevel.corroborated = true;
                imagePyramid.push_back(pyramidLevel);
                break;
            }
        }
    }
```

`imagePyramid` is sorted by level number at the end of `extractImages` already, so the two passes appending out of order is fine. Add `#include "slideio/drivers/svs/svstools.hpp"` to `phtiffslide.cpp` if it is not already there.

- [ ] **Step 8: Run the whole phtiff suite**

Run: `./build/bin/Release/slideio_phtiff_tests.exe`
Expected: 103 passing. Every existing extraction test must still pass unchanged — check especially `phExtractImages_extractsAuxImages`, `phExtractImages_ignoresUndeclaredPyramidDirectories`, `phExtractImages_undeclaredDirectoryDoesNotShiftLaterLevels` and `phExtractImages_ignoresADeclaredLevelTheFileDoesNotStore`, all of which use `level=N` descriptions and must land on the same directories as before.

- [ ] **Step 9: Run the real files and the main suite**

Run `./build/bin/Release/slideio_tests.exe` (489) and confirm the dataset-gated tests in the phtiff suite pass — `zoomLevelsOfPhilips3ExcludeTilePadding`, `readFromPaddedZoomLevelMatchesUnpaddedLevel`, `magnificationOfTheTestFiles` and `metadataOfTheTestFiles` are what prove the new matching lands on the same directories on all four real files.

- [ ] **Step 10: Strike the item and commit**

```bash
git add src/slideio/drivers/svs/svstools.hpp src/slideio/drivers/svs/svstools.cpp \
        src/slideio/drivers/svs/phtiffslide.cpp src/tests/phtiff/test_phtiff_driver.cpp \
        src/tests/main/test_svs_tools.cpp software-docs/TECH_DEBT.md
git commit -m "pair philips zoom levels by the level number the directory declares"
```

---

## Task 5: The driver ids get a header of their own

Closes tech debt item 7.

**Files:**
- Create: `src/slideio/drivers/svs/svsdriverids.hpp`
- Modify: `src/slideio/drivers/svs/svsimagedriver.hpp`, `phtiffimagedriver.hpp`, `phtiffslide.cpp`, `phtiffscene.cpp`, `svsslide.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces: `SVS_DRIVER_ID` and `PHTIFF_DRIVER_ID` declared in `svsdriverids.hpp`, still in `namespace slideio`, still the same string values.

- [ ] **Step 1: Create the header**

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio
{
    // The public driver ids of the two formats the svs driver library serves.
    // They live here rather than in svsimagedriver.hpp so that the slide and scene
    // classes, which need only to name an id, do not have to include the driver class
    // to get it -- data depending on the driver points the dependency the wrong way.
    constexpr const char* SVS_DRIVER_ID = "SVS";
    constexpr const char* PHTIFF_DRIVER_ID = "PHTIFF";
}
```

- [ ] **Step 2: Point everything at it**

In `svsimagedriver.hpp`, delete the two `constexpr` declarations and `#include "slideio/drivers/svs/svsdriverids.hpp"` in their place, keeping the comment that explains what the ids are for. Everything that already includes `svsimagedriver.hpp` for the ids keeps compiling.

In `phtiffslide.cpp` and `phtiffscene.cpp`, replace `#include "slideio/drivers/svs/svsimagedriver.hpp"` with `#include "slideio/drivers/svs/svsdriverids.hpp"`.

In `svsslide.cpp`, DELETE `#include "slideio/drivers/svs/svsimagedriver.hpp"` outright — that file references neither the ids nor the driver class since the refactor. Confirm with `grep -n "SVS_DRIVER_ID\|PHTIFF_DRIVER_ID\|SVSImageDriver" src/slideio/drivers/svs/svsslide.cpp` returning nothing before deleting.

Add `svsdriverids.hpp` to `SOURCE_FILES` in `src/slideio/drivers/svs/CMakeLists.txt`.

- [ ] **Step 3: Confirm the dependency now points down**

Run: `grep -rn "svsimagedriver.hpp" src/slideio/`
Expected: only `phtiffimagedriver.hpp` (which subclasses the driver and genuinely needs it), `svsimagedriver.cpp`, `imagedrivermanager.cpp`, and the CMake listing. No slide or scene file.

- [ ] **Step 4: Build and run**

Run the build, then `./build/bin/Release/slideio_phtiff_tests.exe` (103) and `./build/bin/Release/slideio_tests.exe` (489).

- [ ] **Step 5: Strike the item and commit**

```bash
git add src/slideio/drivers/svs/svsdriverids.hpp src/slideio/drivers/svs/svsimagedriver.hpp \
        src/slideio/drivers/svs/phtiffslide.cpp src/slideio/drivers/svs/phtiffscene.cpp \
        src/slideio/drivers/svs/svsslide.cpp src/slideio/drivers/svs/CMakeLists.txt \
        software-docs/TECH_DEBT.md
git commit -m "give the svs driver ids a header of their own"
```

---

## Task 6: The remaining small items

Closes tech debt items 6, 8 and 9. Four independent single-site edits, batched because none carries its own logic.

**Files:**
- Modify: `src/slideio/core/tools/tools.hpp`, `tools.cpp`, `src/tests/main/test_tools.cpp`
- Modify: `src/slideio/imagetools/tiffkeeper.hpp`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:** removes `Tools::isXml`. No other signature changes.

- [ ] **Step 1: Remove `Tools::isXml` and its test**

Delete the declaration from `tools.hpp`, the definition from `tools.cpp`, and `TEST(Tools, isXml)` from `src/tests/main/test_tools.cpp`.

It was added for the Philips detection work and never adopted, because it parses the document itself and `PHTDescription::isPhilipsDescription` cannot afford a second parse of a description that reaches 844 KB. That reason is structural, so it will not be adopted later either.

Before deleting, run `grep -rn "isXml" src/` and confirm the only hits are the three sites above. **Note in your report that `Tools` is `SLIDEIO_CORE_EXPORTS`, so this is a public API removal** — nothing in this repository uses it, but the Python bindings live in a separate repository that cannot be checked from here.

- [ ] **Step 2: Document the handler swap in the `TIFFKeeper` header**

Above the class in `src/slideio/imagetools/tiffkeeper.hpp`:

```cpp
    // Both constructors install a TIFFMessageHandler, which swaps libtiff's
    // PROCESS-GLOBAL error and warning handlers for the keeper's lifetime and restores
    // them on destruction. With overlapping, non-LIFO keeper lifetimes one destructor can
    // restore a handler while another keeper is still alive, after which libtiff messages
    // go to stderr instead of the log. There is no dangling pointer -- both handlers are
    // static functions -- so the consequence is lost log routing, not a crash.
```

- [ ] **Step 3: Name the driver id by its constant in the tests**

In `src/tests/phtiff/test_phtiff_driver.cpp`, replace the seven `openSlide(filePath, "PHTIFF")` literals with `openSlide(filePath, PHTIFF_DRIVER_ID)`, and the `{"PHTIFF", TestTools::getFullTestImagePath(...)}` pair likewise.

LEAVE the assertion `EXPECT_EQ("PHTIFF", PHTIFFImageDriver().getID())` as a literal. A test that the public id string is `"PHTIFF"` must not be written in terms of the constant it is checking, or it asserts nothing.

- [ ] **Step 4: Widen the accept-side detection coverage**

`canOpenFileByContent` currently accepts on one file. Extend it across all four, which pins that the predicate does not depend on the XML prolog — Philips-4 omits it:

```cpp
	for (const char* fileName : {"Philips-1.tiff", "Philips-2.tiff", "Philips-3.tiff", "Philips-4.tiff"}) {
		const std::string philips = TestTools::getFullTestImagePath("philips", fileName);
		EXPECT_TRUE(driver.canOpenFile(philips)) << fileName;
	}
```

Keep the existing reject-side assertions in that test exactly as they are.

- [ ] **Step 5: Build and run everything**

Run the build, then all eight suites. Expected: `slideio_tests` 488 (one fewer — the `isXml` test was removed), `slideio_phtiff_tests` 103, and pke 15, ome-tiff 98, ndpi 29, vsi 30, converter 140, transformer 39 unchanged.

- [ ] **Step 6: Strike the items and commit**

```bash
git add src/slideio/core/tools/tools.hpp src/slideio/core/tools/tools.cpp \
        src/tests/main/test_tools.cpp src/slideio/imagetools/tiffkeeper.hpp \
        src/tests/phtiff/test_phtiff_driver.cpp software-docs/TECH_DEBT.md
git commit -m "close the remaining philips follow-ups: dead utility, handler note, test ids"
```

---

## Task 7: Close out the debt entry

- [ ] **Step 1: Check every item is struck**

Read `software-docs/TECH_DEBT.md` section 2. Items 1-9 should all be struck through with a pointer to where each was fixed. If any is not, say which and stop.

- [ ] **Step 2: Collapse the section**

Replace the section's body with a short closing note: what was fixed, in which commits, and that the "Consciously accepted, not debt" paragraph at the end stays as a record of the detection-strictness decision. Keep that paragraph verbatim — it is a decision, not debt, and deleting it would let it be rediscovered as a surprise.

- [ ] **Step 3: Commit**

```bash
git add software-docs/TECH_DEBT.md
git commit -m "close the philips tiff follow-ups entry in the tech debt log"
```
