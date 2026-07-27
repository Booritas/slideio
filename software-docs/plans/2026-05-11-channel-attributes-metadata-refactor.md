# Channel Attributes via `Metadata` — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the four channel-attribute methods on `Scene` (and five on `CVScene`) with a single `getChannelAttributes()` accessor returning a `Metadata` tree, swap the underlying storage from `vector<vector<string>>` to `nlohmann::json`, and allow typed values (Bool/Int/Double/String).

**Architecture:** Bridge migration to keep the build green throughout. (1) Add `getChannelAttributes()` synthesizing from the existing vectors. (2) Migrate every consumer to the new API. (3) Swap storage to `nlohmann::json` and rewire the accessor to read it directly. (4) Add typed setter overloads. (5) Remove the old methods and vectors.

**Tech Stack:** C++17, Google Test, Conan v2, CMake, `nlohmann::json` (already a dep, used by the existing `getMetadata()` infrastructure in `src/slideio/core/metadata*`).

**Reference spec:** `docs/superpowers/specs/2026-05-11-channel-attributes-metadata-refactor-design.md`

---

## Pre-flight

Build once at the start to confirm the workspace is in a known-good state. All subsequent build commands assume `release` config unless noted.

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: build succeeds; all `ChannelAttributesTest.*` cases pass against the existing implementation.

---

## Task 1: Add `CVScene::getChannelAttributes()` synthesized from existing storage

**Files:**
- Modify: `src/slideio/core/cvscene.hpp` (add declaration + cache members)
- Modify: `src/slideio/core/cvscene.cpp` (add implementation)
- Test: `src/tests/main/test_channel_attributes.cpp` (add one new-API test alongside existing tests)

- [ ] **Step 1: Add a failing test for the new accessor**

Append to `src/tests/main/test_channel_attributes.cpp`, just before the closing brace of the file:

```cpp
TEST_F(ChannelAttributesTest, GetChannelAttributesTreeShape) {
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure");
    scene->setChannelAttribute(0, "wavelength", "488nm");
    scene->setChannelAttribute(0, "exposure",   "100ms");
    scene->setChannelAttribute(1, "wavelength", "561nm");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    ASSERT_EQ(attrs.type(), slideio::Metadata::Type::Array);
    ASSERT_EQ(attrs.size(), 3u);                              // numChannels == 3
    EXPECT_EQ(attrs[0].type(), slideio::Metadata::Type::Object);
    EXPECT_EQ(attrs[0]["wavelength"].asString(), "488nm");
    EXPECT_EQ(attrs[0]["exposure"].asString(),   "100ms");
    EXPECT_EQ(attrs[1]["wavelength"].asString(), "561nm");
    EXPECT_FALSE(attrs[1].contains("exposure"));
    EXPECT_EQ(attrs[2].type(), slideio::Metadata::Type::Object);
    EXPECT_EQ(attrs[2].size(), 0u);                           // empty object for unset channel
}
```

- [ ] **Step 2: Run test to verify it fails (no such method)**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error referencing `getChannelAttributes` — undeclared identifier.

- [ ] **Step 3: Declare the accessor on `CVScene`**

In `src/slideio/core/cvscene.hpp`, in the `public:` section near the existing channel-attribute methods (around line 218), add:

```cpp
        /**@brief returns channel attributes as a Metadata tree.
         *
         * Always an Array of length getNumChannels(). Each element is an
         * Object keyed by attribute name; channels with no attributes get an
         * empty Object. Built lazily on first call; setters must not be
         * called after the first read.
         */
        const Metadata& getChannelAttributes() const;
```

Then in the `private:` section at the bottom of the class (next to `m_metadataOnce` / `m_metadata`, around line 245), add:

```cpp
        mutable std::once_flag m_channelAttrsOnce;
        mutable Metadata       m_channelAttributesMeta;
```

- [ ] **Step 4: Implement the accessor (synthesize from existing vectors)**

In `src/slideio/core/cvscene.cpp`, append at end of file (after `getMetadata()` definition):

```cpp
const Metadata& CVScene::getChannelAttributes() const
{
    std::call_once(m_channelAttrsOnce, [this]
    {
        const int numChannels = getNumChannels();
        nlohmann::json root = nlohmann::json::array();
        for (int ch = 0; ch < numChannels; ++ch) {
            nlohmann::json obj = nlohmann::json::object();
            if (ch < static_cast<int>(m_channelAttributes.size())) {
                const auto& row = m_channelAttributes[ch];
                for (size_t i = 0; i < row.size() && i < m_channelAttributeNames.size(); ++i) {
                    obj[m_channelAttributeNames[i]] = row[i];
                }
            }
            root.push_back(std::move(obj));
        }
        m_channelAttributesMeta = detail::makeMetadataFromJson(std::move(root));
    });
    return m_channelAttributesMeta;
}
```

- [ ] **Step 5: Build + run the new test**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.GetChannelAttributesTreeShape'
```
Expected: build succeeds; new test passes; existing `ChannelAttributesTest.*` cases continue to pass.

- [ ] **Step 6: Run the rest of the suite to confirm no regression**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: all `ChannelAttributesTest.*` cases pass.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp src/tests/main/test_channel_attributes.cpp
git commit -m "core: add CVScene::getChannelAttributes() Metadata accessor"
```

---

## Task 2: Add `Scene::getChannelAttributes()` public wrapper

**Files:**
- Modify: `src/slideio/slideio/scene.hpp`
- Modify: `src/slideio/slideio/scene.cpp`

- [ ] **Step 1: Declare on `Scene`**

In `src/slideio/slideio/scene.hpp`, in the `public:` section near the existing channel-attribute declarations (around line 282, immediately above the closing `};`), add:

```cpp
        /**@brief returns channel attributes as a Metadata tree.
         *
         * Array of length getNumChannels(); each element is an Object keyed
         * by attribute name (empty Object if a channel has no attributes).
         */
        const Metadata& getChannelAttributes() const;
```

- [ ] **Step 2: Implement the wrapper**

In `src/slideio/slideio/scene.cpp`, append at the end of the file:

```cpp
const slideio::Metadata& Scene::getChannelAttributes() const
{
    SLIDEIO_LOG(INFO) << "Scene::getChannelAttributes ";
    return m_scene->getChannelAttributes();
}
```

- [ ] **Step 3: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 4: Smoke-check via existing suite**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/slideio/scene.hpp src/slideio/slideio/scene.cpp
git commit -m "slideio: add Scene::getChannelAttributes() public wrapper"
```

---

## Task 3: Migrate `tiffconverter.cpp` to the new read API

**Files:**
- Modify: `src/slideio/converter/tiffconverter.cpp:181-200`

- [ ] **Step 1: Inspect the current loop**

The current code (`tiffconverter.cpp:181-200`) iterates by attribute index:

```cpp
    int id = 0;
    const auto& sceneChannelRange = m_parameters.getChannelRange();
    for (int channel = sceneChannelRange.start; channel < sceneChannelRange.end; ++channel) {
        std::string idAttr = std::string("Channel:0:") + std::to_string(id++);
        auto* xmlChannel = doc.NewElement("Channel");
        for (int attIndex = 0; attIndex < (int)m_scene->getNumChannelAttributes(); ++attIndex) {
            std::string attrValue = m_scene->getChannelAttributeValue(channel, attIndex);
            const std::string& attrName = m_scene->getChannelAttributeName(attIndex);
            if (attrName == "Color" && ColorTools::detectHexColorFormat(attrValue) != HexColorFormat::UNKNOWN) {
                attrValue = ColorTools::hexToInt32String(attrValue);
            }
            xmlChannel->SetAttribute(attrName.c_str(), attrValue.c_str());
        }
        xmlChannel->SetAttribute("ID", idAttr.c_str());
```

- [ ] **Step 2: Rewrite the inner loop to walk the channel's Object**

Replace lines 186–193 with:

```cpp
        const slideio::Metadata& chanAttrs = m_scene->getChannelAttributes();
        const slideio::Metadata chan = chanAttrs[channel];
        for (const std::string& attrName : chan.keys()) {
            std::string attrValue = chan[attrName].asString();
            if (attrName == "Color" && ColorTools::detectHexColorFormat(attrValue) != HexColorFormat::UNKNOWN) {
                attrValue = ColorTools::hexToInt32String(attrValue);
            }
            xmlChannel->SetAttribute(attrName.c_str(), attrValue.c_str());
        }
```

If `slideio/core/metadata.hpp` is not already included via the existing headers, add `#include "slideio/core/metadata.hpp"` to the top of `tiffconverter.cpp`. (Check the existing includes; `slideio/slideio/scene.hpp` already pulls it transitively, so an extra include is typically unnecessary.)

- [ ] **Step 3: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 4: Run converter tests**

```bash
./build/release/bin/slideio_converter_tests
```
Expected: all converter tests pass (the converter test file still uses the old read API but it operates on `Scene`, not on the converter itself — its assertions are independent).

- [ ] **Step 5: Commit**

```bash
git add src/slideio/converter/tiffconverter.cpp
git commit -m "converter: read channel attributes via Metadata tree"
```

---

## Task 4: Migrate `test_channel_attributes.cpp` to the new read API

**Files:**
- Modify: `src/tests/main/test_channel_attributes.cpp`

- [ ] **Step 1: Replace test bodies to use `getChannelAttributes()`**

Open `src/tests/main/test_channel_attributes.cpp`. The fixture and `SetUp()` (lines 12–21) stay as-is.

Replace the test bodies as follows. For each replacement, keep the `TEST_F(ChannelAttributesTest, ...)` header line; replace only the body. Where a test was asserting on the old API, rewrite to assert against the tree.

**Note on lazy-build freeze:** `getChannelAttributes()` caches on first call (via `std::call_once`). Within a single test, call `getChannelAttributes()` **at most once**, after all setter calls. Tests that need to read between setter batches must re-create the scene (see `OverwriteAttributeValue` below).

Delete `DefineChannelAttribute` (lines 23–38) entirely — `defineChannelAttribute` itself goes away in Task 10 and the "re-define is a no-op" semantic only describes the legacy indexed API.

Delete `GetChannelAttributeIndex` (lines 40–53) entirely — the indexed lookup is gone in the new API.

Replace `SetAndGetChannelAttribute` (lines 55–79) with:

```cpp
TEST_F(ChannelAttributesTest, SetAndGetChannelAttribute) {
    scene->setChannelAttribute(0, "wavelength",   "488nm");
    scene->setChannelAttribute(0, "exposure_time","100ms");
    scene->setChannelAttribute(1, "wavelength",   "561nm");
    scene->setChannelAttribute(1, "exposure_time","150ms");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_EQ(attrs[0]["wavelength"].asString(),    "488nm");
    EXPECT_EQ(attrs[0]["exposure_time"].asString(), "100ms");
    EXPECT_EQ(attrs[1]["wavelength"].asString(),    "561nm");
    EXPECT_EQ(attrs[1]["exposure_time"].asString(), "150ms");
}
```

Replace `SetAttributeInvalidChannelIndex` (lines 81–87) with:

```cpp
TEST_F(ChannelAttributesTest, SetAttributeInvalidChannelIndex) {
    EXPECT_THROW(scene->setChannelAttribute(-1, "wavelength", "488nm"), slideio::RuntimeError);
    EXPECT_THROW(scene->setChannelAttribute(3,  "wavelength", "488nm"), slideio::RuntimeError);
}
```

Delete `GetAttributeInvalidChannelIndex` (lines 89–96) — there is no longer a getter that takes a channel index and throws; out-of-range channel access via `attrs[ch]` is the user's responsibility, parallel to other Metadata array indexing.

Replace `GetAttributeNonExistent` (lines 98–104) with:

```cpp
TEST_F(ChannelAttributesTest, GetAttributeAbsent) {
    scene->setChannelAttribute(0, "wavelength", "488nm");
    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_TRUE(attrs[0].contains("wavelength"));
    EXPECT_FALSE(attrs[0].contains("non_existent"));
}
```

Replace `GetChannelAttributes` (lines 106–122) with:

```cpp
TEST_F(ChannelAttributesTest, GetChannelAttributes) {
    scene->setChannelAttribute(0, "wavelength",    "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain",          "2.5");

    const slideio::Metadata chan0 = scene->getChannelAttributes()[0];
    EXPECT_EQ(chan0["wavelength"].asString(),    "488nm");
    EXPECT_EQ(chan0["exposure_time"].asString(), "100ms");
    EXPECT_EQ(chan0["gain"].asString(),          "2.5");
    EXPECT_EQ(chan0.size(), 3u);
}
```

Replace `MultipleChannelsDifferentAttributes` (lines 124–155) with:

```cpp
TEST_F(ChannelAttributesTest, MultipleChannelsDifferentAttributes) {
    scene->setChannelAttribute(0, "wavelength",    "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain",          "2.5");
    scene->setChannelAttribute(1, "wavelength",    "561nm");
    scene->setChannelAttribute(1, "exposure_time", "150ms");
    scene->setChannelAttribute(1, "gain",          "3.0");
    scene->setChannelAttribute(2, "wavelength",    "640nm");
    scene->setChannelAttribute(2, "exposure_time", "200ms");
    scene->setChannelAttribute(2, "gain",          "3.5");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_EQ(attrs[0]["wavelength"].asString(),    "488nm");
    EXPECT_EQ(attrs[0]["exposure_time"].asString(), "100ms");
    EXPECT_EQ(attrs[0]["gain"].asString(),          "2.5");
    EXPECT_EQ(attrs[1]["wavelength"].asString(),    "561nm");
    EXPECT_EQ(attrs[1]["exposure_time"].asString(), "150ms");
    EXPECT_EQ(attrs[1]["gain"].asString(),          "3.0");
    EXPECT_EQ(attrs[2]["wavelength"].asString(),    "640nm");
    EXPECT_EQ(attrs[2]["exposure_time"].asString(), "200ms");
    EXPECT_EQ(attrs[2]["gain"].asString(),          "3.5");
}
```

Delete `EmptyAttributeValues` (lines 157–168) entirely. The bridge synthesis added in Task 1 skips slots where the stored string is empty (because legacy `defineChannelAttribute` pre-fills rows with `""`), so explicitly-empty values cannot be round-tripped through the new accessor during the bridge. Task 8 re-adds an `EmptyAttributeValues` test once the storage is `nlohmann::json` and the distinction is clean.

Replace `OverwriteAttributeValue` (lines 170–180) with:

```cpp
TEST_F(ChannelAttributesTest, OverwriteAttributeValue) {
    scene->setChannelAttribute(0, "wavelength", "488nm");
    EXPECT_EQ(scene->getChannelAttributes()[0]["wavelength"].asString(), "488nm");
    // Re-build via a fresh scene to bypass the lazy-build freeze contract.
    scene = std::make_shared<TestScene>();
    scene->setNumChannels(3);
    scene->setChannelAttribute(0, "wavelength", "561nm");
    EXPECT_EQ(scene->getChannelAttributes()[0]["wavelength"].asString(), "561nm");
}
```

Delete `NumChannelAttributes` (lines 182–197) — there is no global count; per-channel `keys().size()` is the substitute. Coverage of "re-defining the same name doesn't duplicate" is preserved by `DefineChannelAttribute` above.

Keep the `GetChannelAttributesTreeShape` test added in Task 1 (it stays at the end of the file).

- [ ] **Step 2: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds; all old-API usages from this file are gone.

- [ ] **Step 3: Run the migrated suite**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: all listed tests pass. There should be: `DefineChannelAttribute`, `SetAndGetChannelAttribute`, `SetAttributeInvalidChannelIndex`, `GetAttributeAbsent`, `GetChannelAttributes`, `MultipleChannelsDifferentAttributes`, `EmptyAttributeValues`, `OverwriteAttributeValue`, `GetChannelAttributesTreeShape`.

- [ ] **Step 4: Commit**

```bash
git add src/tests/main/test_channel_attributes.cpp
git commit -m "tests: migrate channel-attribute tests to Metadata API"
```

---

## Task 5: Migrate `test_czi_driver.cpp` channel-attribute assertions

**Files:**
- Modify: `src/tests/main/test_czi_driver.cpp:675-684`

- [ ] **Step 1: Replace the assertions**

Open `src/tests/main/test_czi_driver.cpp`. Replace lines 675–684:

```cpp
    const int numChannelAttributes = scene->getNumChannelAttributes();
    EXPECT_GE(scene->getChannelAttributeIndex("Name"), 0);
    EXPECT_EQ(scene->getChannelAttributeValue(0, "Name"), "ChS1");
    EXPECT_EQ(scene->getChannelAttributeValue(1, "Name"), "Ch2");
    EXPECT_EQ(scene->getChannelAttributeValue(2, "Name"), "NDD T1");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "EmissionWavelength"), "610.63882650000005");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "ChannelType"), "Unspecified");
    EXPECT_EQ(scene->getChannelAttributeValue(1, "PinholeSizeAiry"), "1");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "AcquisitionMode"), "LaserScanningConfocalMicroscopy");
```

with:

```cpp
    const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
    EXPECT_TRUE(chanAttrs[0].contains("Name"));
    EXPECT_EQ(chanAttrs[0]["Name"].asString(),                "ChS1");
    EXPECT_EQ(chanAttrs[1]["Name"].asString(),                "Ch2");
    EXPECT_EQ(chanAttrs[2]["Name"].asString(),                "NDD T1");
    EXPECT_EQ(chanAttrs[0]["EmissionWavelength"].asString(),  "610.63882650000005");
    EXPECT_EQ(chanAttrs[0]["ChannelType"].asString(),         "Unspecified");
    EXPECT_EQ(chanAttrs[1]["PinholeSizeAiry"].asString(),     "1");
    EXPECT_EQ(chanAttrs[0]["AcquisitionMode"].asString(),     "LaserScanningConfocalMicroscopy");
```

Note: the previously unused local `numChannelAttributes` is dropped entirely (it was assigned and never read).

If `slideio/core/metadata.hpp` is not yet included by this test, add `#include "slideio/core/metadata.hpp"`. It is typically pulled transitively via `slideio/slideio/scene.hpp`, so an extra include is usually unnecessary.

- [ ] **Step 2: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 3: Run only the CZI test that touches channel attributes**

The test name (per the surrounding code in `test_czi_driver.cpp`) is in the `CZIImageDriverTests` suite. Use a pattern that matches the channel-attribute test, e.g.:

```bash
./build/release/bin/slideio_tests --gtest_filter='*Channel*Attribut*'
```
Expected: the migrated test runs and passes (requires the corresponding test image present in the test data tree; if the image is missing the test is `DISABLED` or skipped — that's fine, the build is the gate here).

- [ ] **Step 4: Commit**

```bash
git add src/tests/main/test_czi_driver.cpp
git commit -m "tests/czi: migrate channel-attribute assertions to Metadata API"
```

---

## Task 6: Migrate `test_converter.cpp` channel-attribute assertions

**Files:**
- Modify: `src/tests/converter/test_converter.cpp:230-238`

- [ ] **Step 1: Replace the assertions**

Open `src/tests/converter/test_converter.cpp`. Replace lines 230–238:

```cpp
    EXPECT_EQ(scene->getNumChannelAttributes(), 10);
    EXPECT_GE(scene->getChannelAttributeIndex("Name"), 0);
    EXPECT_EQ(scene->getChannelAttributeValue(0, "Name"), "ChS1");
    EXPECT_EQ(scene->getChannelAttributeValue(1, "Name"), "Ch2");
    EXPECT_EQ(scene->getChannelAttributeValue(2, "Name"), "NDD T1");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "EmissionWavelength"), "610.63882650000005");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "ChannelType"), "Unspecified");
    EXPECT_EQ(scene->getChannelAttributeValue(1, "PinholeSizeAiry"), "1");
    EXPECT_EQ(scene->getChannelAttributeValue(0, "AcquisitionMode"), "LaserScanningConfocalMicroscopy");
```

with:

```cpp
    const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
    EXPECT_EQ(chanAttrs[0].size(), 10u);                                          // 10 attributes on channel 0
    EXPECT_TRUE(chanAttrs[0].contains("Name"));
    EXPECT_EQ(chanAttrs[0]["Name"].asString(),                "ChS1");
    EXPECT_EQ(chanAttrs[1]["Name"].asString(),                "Ch2");
    EXPECT_EQ(chanAttrs[2]["Name"].asString(),                "NDD T1");
    EXPECT_EQ(chanAttrs[0]["EmissionWavelength"].asString(),  "610.63882650000005");
    EXPECT_EQ(chanAttrs[0]["ChannelType"].asString(),         "Unspecified");
    EXPECT_EQ(chanAttrs[1]["PinholeSizeAiry"].asString(),     "1");
    EXPECT_EQ(chanAttrs[0]["AcquisitionMode"].asString(),     "LaserScanningConfocalMicroscopy");
```

Note the semantic shift: `getNumChannelAttributes()` returned the global count of distinct attribute names (10). `chanAttrs[0].size()` returns the number of attributes that were actually set on channel 0. For the existing CZI scene used in this test, channel 0 has all 10 attributes populated, so the count matches. If a future regression makes channel 0 sparse, this assertion will surface it correctly.

- [ ] **Step 2: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 3: Run converter tests**

```bash
./build/release/bin/slideio_converter_tests --gtest_filter='*Channel*Attribut*'
```
Expected: tests run; assertions on a present test image pass.

- [ ] **Step 4: Commit**

```bash
git add src/tests/converter/test_converter.cpp
git commit -m "tests/converter: migrate channel-attribute assertions to Metadata API"
```

---

## Task 7: Migrate `test_ometiff_driver.cpp` `channelAttributes` test

**Files:**
- Modify: `src/tests/ometiff/test_ometiff_driver.cpp:690-733`

- [ ] **Step 1: Rewrite the test body**

Open `src/tests/ometiff/test_ometiff_driver.cpp`. Replace the entire `TEST_F(OTImageDriverTests, channelAttributes)` body (lines 690–733):

```cpp
TEST_F(OTImageDriverTests, channelAttributes) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "private/test.ome.tif");
    OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    auto scene = slide->getScene(0);
    const int numChannels = scene->getNumChannels();
    const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
    ASSERT_EQ(static_cast<int>(chanAttrs.size()), numChannels);

    typedef std::tuple<std::string, std::vector<std::string>> AttInfo;
    std::vector<AttInfo> expectedAttNames = {
        {"ID", {"Channel:0:0", "Channel:0:1","Channel:0:2","Channel:0:3",
            "Channel:0:4","Channel:0:5", "Channel:0:6","Channel:0:7","Channel:0:8",
            "Channel:0:9","Channel:0:10","Channel:0:11", "Channel:0:12","Channel:0:13",
            "Channel:0:14","Channel:0:15"}},
        {"SamplesPerPixel", {"1","1","1","1","1",
            "1","1","1","1","1",
            "1","1","1","1","1"}},
        {"Name", {"DAPI", "CD8", "CD4", "CD11c", "CD68",
            "DAPI2", "CD11b", "PD-1", "CD56", "CD20",
            "DAPI3", "CD3", "CD14","CD206", "CK"}},
        {"Color", {"65535","-10092289", "-3407617", "-872480513", "1711210751",
            "65535", "16738047", "16763903", "13369343", "6750207",
            "65535", "1694564351", "-872349697", "-16724993", "-16750849"  }},
        {"ContrastMethod", {"Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence",
            "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence",
            "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence"}},
        {"EmissionWavelength", {"440", "371", "392", "413", "434",
            "440", "476", "497", "518", "539",
            "440", "581","602", "623", "645"}}
    };

    for (const AttInfo& info : expectedAttNames) {
        const std::string& expectedAttName = std::get<0>(info);
        const std::vector<std::string>& expectedValues = std::get<1>(info);
        for (int channel = 0; channel < numChannels && channel < static_cast<int>(expectedValues.size()); ++channel) {
            ASSERT_TRUE(chanAttrs[channel].contains(expectedAttName))
                << "Channel " << channel << " missing attribute " << expectedAttName;
            EXPECT_EQ(chanAttrs[channel][expectedAttName].asString(), expectedValues[channel])
                << "Channel " << channel << " attribute " << expectedAttName;
        }
    }
}
```

Note: the original test also called `getNumChannelAttributes()` and `ASSERT_EQ(numAtts, 6)`. That global-count concept is gone. To preserve the intent (the image has six named attributes per channel), assert per-channel `size() == 6` on channel 0 if a quick sanity check is wanted; otherwise the per-attribute `contains` checks cover the meaningful invariant.

If you want the size check, add right after the `chanAttrs.size()` assertion:

```cpp
    EXPECT_EQ(chanAttrs[0].size(), 6u);
```

- [ ] **Step 2: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 3: Run OME-TIFF tests**

```bash
./build/release/bin/slideio_ometiff_tests --gtest_filter='*channelAttributes*'
```
Expected: test compiles and runs (passes if the test image is present; otherwise is skipped — the build is the gate).

- [ ] **Step 4: Commit**

```bash
git add src/tests/ometiff/test_ometiff_driver.cpp
git commit -m "tests/ometiff: migrate channelAttributes test to Metadata API"
```

---

## Task 8: Swap storage to `nlohmann::json` and rewire `getChannelAttributes()`

At this point no consumer reads the old methods. We can replace the underlying storage and have `getChannelAttributes()` wrap the json directly.

**Files:**
- Modify: `src/slideio/core/cvscene.hpp` (add json member, keep old vector members until Task 10)
- Modify: `src/slideio/core/cvscene.cpp` (rewrite `setChannelAttribute(string,string,string)` and `getChannelAttributes()`)

- [ ] **Step 1: Add the json storage member**

In `src/slideio/core/cvscene.hpp`, in the `protected:` section near the existing storage (around line 241, next to `m_channelAttributeNames` and `m_channelAttributes`), add:

```cpp
        nlohmann::json m_channelAttributesJson;
```

That requires `#include <nlohmann/json_fwd.hpp>` at the top of `cvscene.hpp`. Check the current includes — if `nlohmann/json.hpp` is already included transitively (via `metadata_internal.hpp`), no change is needed in this header. If the build complains about an incomplete type for `nlohmann::json` as a member, add:

```cpp
#include <nlohmann/json.hpp>
```

near the existing `#include` block at the top of `cvscene.hpp` (alongside `#include "slideio/core/metadata.hpp"`).

- [ ] **Step 2: Rewrite the string setter to write into the json**

In `src/slideio/core/cvscene.cpp`, replace the body of `setChannelAttribute(int, const std::string&, const std::string&)` (currently at lines 260–276):

```cpp
void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &attributeName, const std::string &attributeValue)
{
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    if (!m_channelAttributesJson.is_array()) {
        m_channelAttributesJson = nlohmann::json::array();
    }
    while (static_cast<int>(m_channelAttributesJson.size()) <= channelIndex) {
        m_channelAttributesJson.push_back(nlohmann::json::object());
    }
    m_channelAttributesJson[channelIndex][attributeName] = attributeValue;
}
```

Note: the old vector storage (`m_channelAttributeNames`, `m_channelAttributes`) is intentionally **no longer written** here. That storage will be removed in Task 10; any remaining code reading it (the old getters, `defineChannelAttribute`, the second `getChannelAttributeValue` overload) is also removed in Task 10.

- [ ] **Step 3: Rewrite `getChannelAttributes()` to wrap the json directly**

In `src/slideio/core/cvscene.cpp`, replace the body of `CVScene::getChannelAttributes()` added in Task 1:

```cpp
const Metadata& CVScene::getChannelAttributes() const
{
    std::call_once(m_channelAttrsOnce, [this]
    {
        const int numChannels = getNumChannels();
        nlohmann::json root = m_channelAttributesJson.is_array()
                                  ? m_channelAttributesJson
                                  : nlohmann::json::array();
        while (static_cast<int>(root.size()) < numChannels) {
            root.push_back(nlohmann::json::object());
        }
        m_channelAttributesMeta = detail::makeMetadataFromJson(std::move(root));
    });
    return m_channelAttributesMeta;
}
```

- [ ] **Step 4: Make the old read methods inert (temporary)**

Because the old vectors are no longer written but the old read methods still reference them, the existing methods must continue to compile without UB. They will be deleted in Task 10. For this task, leave them as-is — they will simply return empty/throw "not found" data because the vectors are empty. No consumer reads them after Tasks 3–7, so this is benign for the build green-ness.

If any internal CVScene method other than the getters reads `m_channelAttributeNames` / `m_channelAttributes`, audit and fix. (A grep over `cvscene.cpp` for those two members shows only the methods listed in the spec; no audit gap is expected.)

- [ ] **Step 5: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 6: Re-add the `EmptyAttributeValues` test**

The bridge synthesis in Task 1 dropped explicitly-empty string values, so an `EmptyAttributeValues` test was deleted in Task 4. With the json-backed storage in this task, empty strings round-trip cleanly. Append:

```cpp
TEST_F(ChannelAttributesTest, EmptyAttributeValues) {
    scene->setChannelAttribute(0, "wavelength", "");
    scene->setChannelAttribute(0, "comment",    "");
    const slideio::Metadata chan0 = scene->getChannelAttributes()[0];
    EXPECT_TRUE(chan0.contains("wavelength"));
    EXPECT_TRUE(chan0.contains("comment"));
    EXPECT_EQ(chan0["wavelength"].asString(), "");
    EXPECT_EQ(chan0["comment"].asString(),    "");
}
```

- [ ] **Step 7: Run the channel-attribute test suite end-to-end**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: all pass against the new json-backed storage, including the newly-restored `EmptyAttributeValues`.

- [ ] **Step 8: Run the broader driver tests that touch attributes**

```bash
./build/release/bin/slideio_tests              --gtest_filter='*Channel*Attribut*'
./build/release/bin/slideio_converter_tests    --gtest_filter='*Channel*Attribut*'
./build/release/bin/slideio_ometiff_tests      --gtest_filter='*channelAttributes*'
```
Expected: all pass (or are skipped because the test image is absent — neither outcome is a regression).

- [ ] **Step 9: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp \
        src/tests/main/test_channel_attributes.cpp
git commit -m "core: back channel attributes with nlohmann::json storage"
```

---

## Task 9: Add typed setter overloads

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`
- Test: `src/tests/main/test_channel_attributes.cpp` (add typed-value tests)

- [ ] **Step 1: Write failing tests for typed setters**

Append to `src/tests/main/test_channel_attributes.cpp`:

```cpp
TEST_F(ChannelAttributesTest, TypedAttributeValues) {
    scene->setChannelAttribute(0, "wavelength_nm", static_cast<int64_t>(488));
    scene->setChannelAttribute(0, "exposure_s",    0.1);
    scene->setChannelAttribute(0, "active",        true);
    scene->setChannelAttribute(0, "lookup",        "488nm");          // const char* overload
    const std::string s = "manual";
    scene->setChannelAttribute(0, "mode",          s);                // string overload

    const slideio::Metadata chan0 = scene->getChannelAttributes()[0];
    EXPECT_EQ(chan0["wavelength_nm"].type(), slideio::Metadata::Type::Int);
    EXPECT_EQ(chan0["wavelength_nm"].asInt(), 488);
    EXPECT_EQ(chan0["exposure_s"].type(),    slideio::Metadata::Type::Double);
    EXPECT_DOUBLE_EQ(chan0["exposure_s"].asDouble(), 0.1);
    EXPECT_EQ(chan0["active"].type(),        slideio::Metadata::Type::Bool);
    EXPECT_TRUE(chan0["active"].asBool());
    EXPECT_EQ(chan0["lookup"].type(),        slideio::Metadata::Type::String);
    EXPECT_EQ(chan0["lookup"].asString(),    "488nm");
    EXPECT_EQ(chan0["mode"].type(),          slideio::Metadata::Type::String);
    EXPECT_EQ(chan0["mode"].asString(),      "manual");
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error — `setChannelAttribute(int, const char*, int64_t)` etc. unresolved, or the `bool` literal `true` resolving to `int64_t` causing the wrong type stored.

- [ ] **Step 3: Declare the overloads on `CVScene`**

In `src/slideio/core/cvscene.hpp`, find the existing `setChannelAttribute` declaration (around line 211). Replace its block with:

```cpp
        /**@brief adds a new attribute to channels (string value) */
        virtual void setChannelAttribute(int channelIndex, const std::string& attributeName, const std::string& attributeValue);
        /**@brief adds a new attribute to channels (string literal) */
        virtual void setChannelAttribute(int channelIndex, const std::string& attributeName, const char* attributeValue);
        /**@brief adds a new attribute to channels (bool value) */
        virtual void setChannelAttribute(int channelIndex, const std::string& attributeName, bool attributeValue);
        /**@brief adds a new attribute to channels (int value) */
        virtual void setChannelAttribute(int channelIndex, const std::string& attributeName, int64_t attributeValue);
        /**@brief adds a new attribute to channels (double value) */
        virtual void setChannelAttribute(int channelIndex, const std::string& attributeName, double attributeValue);
```

- [ ] **Step 4: Implement the overloads on `CVScene`**

In `src/slideio/core/cvscene.cpp`, add right after the existing string-typed `setChannelAttribute` definition (after the body finalized in Task 8):

```cpp
namespace {
    nlohmann::json& growToChannel(nlohmann::json& storage, int channelIndex) {
        if (!storage.is_array()) {
            storage = nlohmann::json::array();
        }
        while (static_cast<int>(storage.size()) <= channelIndex) {
            storage.push_back(nlohmann::json::object());
        }
        return storage[channelIndex];
    }
}

void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &name, const char* value) {
    setChannelAttribute(channelIndex, name, std::string(value));
}

void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &name, bool value) {
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    growToChannel(m_channelAttributesJson, channelIndex)[name] = value;
}

void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &name, int64_t value) {
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    growToChannel(m_channelAttributesJson, channelIndex)[name] = value;
}

void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &name, double value) {
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    growToChannel(m_channelAttributesJson, channelIndex)[name] = value;
}
```

While we're here, refactor the string-typed setter from Task 8 to use the same helper to avoid duplication. Replace its body with:

```cpp
void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &name, const std::string &value)
{
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    growToChannel(m_channelAttributesJson, channelIndex)[name] = value;
}
```

- [ ] **Step 5: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds.

- [ ] **Step 6: Run the new typed test plus the rest of the suite**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```
Expected: all pass, including `TypedAttributeValues`.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp src/tests/main/test_channel_attributes.cpp
git commit -m "core: add typed setter overloads for channel attributes"
```

---

## Task 10: Remove old read methods, `defineChannelAttribute`, and old vector storage

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`
- Modify: `src/slideio/slideio/scene.hpp`
- Modify: `src/slideio/slideio/scene.cpp`
- Test: `src/tests/main/test_channel_attributes.cpp` (drop the now-orphan `defineChannelAttribute` calls)

- [ ] **Step 1: Remove from `CVScene` header**

In `src/slideio/core/cvscene.hpp`, delete the following declarations from the `public:` section (around lines 206–221):

- `virtual int defineChannelAttribute(const std::string& attributeName);`
- `virtual int getChannelAttributeIndex(const std::string& attributeName) const;`
- `virtual std::string getChannelAttributeValue(int channelIndex, const std::string& attributeName) const;`
- `virtual const std::string& getChannelAttributeValue(int channelIndex, int attributeIndex) const;`
- `virtual const std::string& getChannelAttributeName(int index) const;`
- `virtual int getNumChannelAttributes() const { ... }`

Keep:
- The `setChannelAttribute` overloads added in Task 9.
- The `getChannelAttributes()` declaration added in Task 1.

In the `protected:` section, delete:

- `std::vector<std::string> m_channelAttributeNames;`
- `std::vector<std::vector<std::string>> m_channelAttributes;`

Keep:
- `nlohmann::json m_channelAttributesJson;`

- [ ] **Step 2: Remove from `CVScene` implementation**

In `src/slideio/core/cvscene.cpp`, delete the bodies of the methods removed in Step 1 (currently around lines 232–315):

- `CVScene::defineChannelAttribute`
- `CVScene::getChannelAttributeIndex`
- `CVScene::getChannelAttributeValue(int, const std::string&)`
- `CVScene::getChannelAttributeValue(int, int)`
- `CVScene::getChannelAttributeName`

The `setChannelAttribute` overloads (string, const char*, bool, int64_t, double) and `getChannelAttributes()` remain.

- [ ] **Step 3: Remove from `Scene` header**

In `src/slideio/slideio/scene.hpp`, delete lines 279–282:

```cpp
virtual int getNumChannelAttributes() const;
virtual int getChannelAttributeIndex(const std::string& attributeName) const;
virtual std::string getChannelAttributeName(int attributeIndex) const;
virtual std::string getChannelAttributeValue(int channelIndex, const std::string& attributeName) const;
```

Keep the `getChannelAttributes()` declaration added in Task 2.

- [ ] **Step 4: Remove from `Scene` implementation**

In `src/slideio/slideio/scene.cpp`, delete the four method bodies at lines 329–343:

- `Scene::getNumChannelAttributes`
- `Scene::getChannelAttributeIndex`
- `Scene::getChannelAttributeName`
- `Scene::getChannelAttributeValue`

The `Scene::getChannelAttributes()` body added in Task 2 remains.

- [ ] **Step 5: Drop orphan `defineChannelAttribute` calls in tests**

In `src/tests/main/test_channel_attributes.cpp`, search for `defineChannelAttribute` and delete the call lines. After Task 4, only the `DefineChannelAttribute` test references it; remove that test entirely since `defineChannelAttribute` no longer exists. The semantic it covered (re-defining doesn't duplicate, defining without setting yields an empty entry) is no longer applicable in the new model.

```bash
grep -n defineChannelAttribute src/tests/main/test_channel_attributes.cpp
```
Expected (after deletion): no matches.

- [ ] **Step 6: Build**

```bash
python3 install.py -a build -c release
```
Expected: build succeeds. If anything still references a deleted symbol, the compile error will name the file and line — fix by removing or migrating that usage. (Per Tasks 3–7, no consumer should remain.)

- [ ] **Step 7: Full test pass**

```bash
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_ometiff_tests
```
Expected: all pass (or appropriately skip when test data is absent). No new failures vs. baseline.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp \
        src/slideio/slideio/scene.hpp src/slideio/slideio/scene.cpp \
        src/tests/main/test_channel_attributes.cpp
git commit -m "core: drop legacy channel-attribute API and string-vector storage"
```

---

## Final verification

- [ ] **Confirm no stale references**

```bash
grep -rn "getNumChannelAttributes\|getChannelAttributeIndex\|getChannelAttributeName\|getChannelAttributeValue\|defineChannelAttribute" src/
```
Expected: no matches.

- [ ] **Confirm new API is present at both layers**

```bash
grep -rn "getChannelAttributes()" src/slideio/core/cvscene.hpp src/slideio/slideio/scene.hpp
```
Expected: one match in each header.

- [ ] **Run the full target test set**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_ometiff_tests
```
Expected: green across the board (skipped tests due to missing data are acceptable).
