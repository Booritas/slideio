# Open options for `openSlide`, and MRXS crop-to-content

Date: 2026-11-15. Status: implemented on feature/open-slide-options (2026-11-15),
slideio and slideio-python; manual review of AI-written code pending before merge.
Points marked *(my call)* are design decisions taken without a requirement behind
them; review them first.

## 1. Intent

QuPath shows an MRXS slide cropped to the scanned area. slideio shows the whole
nominal canvas, most of it background fill. The crop rectangle is not tissue
detection. It is the bounding box of the level-0 tiles the scanner stored, which
OpenSlide publishes as `openslide.bounds-x/y/width/height`. OpenSlide itself
does not crop: its level dimensions are the full canvas. QuPath applies the crop
on top of OpenSlide.

slideio already computes the same rectangle. `Geometry::build` stores it as
`bounds0` (`drivers/mrxs/mrxsgeometry.cpp`), and `describe` publishes it rounded
outward as metadata `mrxs.contentBounds`. What is missing is a way to ask the
driver to *present* the slide cropped.

That needs a channel from the caller to the driver at open time, and none exists:
`openSlide(path, driver)` takes nothing else. This design adds a generic one,
**open options**, a string key/value map passed to `openSlide`. MRXS crop is its
first consumer. The aim is that the next driver needing an open-time switch
(a decoder choice, a cache size) adds keys, not API.

## 2. Goals, non-goals, anti-goals

Goals:
- A generic, driver-agnostic way to pass options when opening a slide, in C++ and
  Python.
- `mrxs.crop_to_content`, **on by default**: the MRXS scene is the content box,
  as QuPath shows it. It is smaller, its coordinates start at (0, 0) at the box
  corner, and every pyramid level shrinks to match. `false` gives the full
  nominal canvas, as OpenSlide reports it.
- Binary compatibility with 2.10: existing callers keep working without
  relinking, and no driver other than MRXS changes behaviour.
- Ships in **2.11.0**, the first release with the MRXS driver. That release is
  still an unpublished draft, so MRXS has no readers whose coordinates the
  default could move. This is the one moment the default can be chosen freely.

Non-goals:
- Tissue segmentation, or a tighter outline than one rectangle. Two tissue
  pieces far apart still give one box with background between them.
- Splitting an MRXS slide into one scene per tissue region.
- Options for `convertScene`, `transformScene` or any call other than
  `openSlide`.
- Environment variables or process-global settings.

Anti-goals (a failure even if it works):
- A typo in an option key silently falls back to the default.
- A caller of any driver other than MRXS sees different coordinates, sizes or
  pixels.
- The MRXS default changes again after 2.11.0 is published. From then on, it
  would be a breaking change.
- The exported `openSlide(const std::string&, const std::string&)` symbol
  changes or disappears.
- Options lost on the converter's per-thread re-open, so a cropped conversion
  silently degrades to one shared scene.

## 3. Approaches considered

1. **A per-driver public accessor** (`Scene::getContentBounds()` or similar). Gives
   callers the box, but the method only means something for MRXS, and the caller
   still crops by hand. Rejected: public surface for one driver.
2. **A second driver ID** (`"MRXS-CROPPED"`) handling the same files differently.
   No signature change, but two drivers claim one file, AUTO detection can only
   pick one, and every future option multiplies IDs. Rejected.
3. **An environment variable** (`SLIDEIO_MRXS_CROP=1`). No API change, but hidden
   process-global state that differs between slides in the same process.
   Rejected.
4. **Open options on `openSlide`** — chosen. Generic, discoverable, scoped to one
   slide, reusable by every driver, and maps directly to Python keyword
   arguments or a dict.

## 4. Public API

### 4.1 C++

In `slideio/slideio/slideio.hpp`, alongside the existing function:

```cpp
using SlideOpenOptions = std::map<std::string, std::string>;

SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path,
                                                 const std::string& driver,
                                                 const SlideOpenOptions& options);
```

- **An overload, not a default argument.** Adding `const SlideOpenOptions& = {}`
  to the existing function would change its mangled name and break every binary
  linked against 2.10/2.11. The existing function, `openSlide(path, driver = "")`,
  stays exactly as it is, default included, and forwards to the new one with an
  empty map. The new overload has no defaults. A one- or two-argument call
  resolves to the old function, a three-argument call to the new one, so there is
  no ambiguity.
- `slideio.hpp` gains `#include <map>`: public headers include what they use
  (see CLAUDE.md, *Public headers*).
- `Slide` declares the new overload as a friend, as it does the existing one.
- A map of strings rather than a struct: a struct with a field per option would
  be the per-driver-surface problem again, and every new option would change a
  public type. *(my call: the name `SlideOpenOptions`.)*

Doxygen on the overload documents the key rules (§5) and lists the keys that
exist, at first only `mrxs.crop_to_content`.

### 4.2 Python (`slideio-python` repository)

```python
slideio.open_slide(path, driver="AUTO", options=None)   # options: dict | None
slideio.Slide(path, driver, options=None)               # driver stays required, as today
```

- Values are converted to strings before crossing into C++: `bool` becomes
  `"true"`/`"false"`, numbers become `str(value)`, strings pass through.
  Anything else raises `TypeError` in Python. *(my call)*
- `src/pyglobals.cpp` / `src/pybind.cpp`: the pybind function `open_slide`
  (imported in Python as `core_open_slide`) gains an optional third argument.
  Its two existing arguments stay required.
- A dict rather than `**kwargs`: keys contain dots (`mrxs.crop_to_content`),
  which are not valid Python identifiers. *(my call)*

## 5. Option keys

- **Every key is namespaced by a driver ID**: `<driver-id>.<name>`. The prefix is
  everything before the first `.`. It is compared case-insensitively with
  `ImageDriver::getID()` and has no alias table, so it is the ID that `getDriverIDs()`
  returns. Docs write it lowercase: `mrxs.`, `ometiff.`, `qptiff.` (PKE's ID),
  `phtiff.`. *(my call: no aliases. A second spelling is a second thing to keep
  in sync.)*
- **Keys with no prefix are rejected** for now. They are reserved for options
  several drivers might share later. Rejecting them today keeps that space free.
  *(my call)*
- **The prefix must name a known driver**, or `openSlide` throws. This catches
  `mrsx.crop_to_content`.
- **Keys addressed to the driver that opens the file are the driver's to
  validate.** An unknown name under its own prefix throws, naming the key. So does
  a malformed value.
- **Keys addressed to other known drivers are ignored.** One options map can then
  serve AUTO detection over a folder of mixed formats. This holds when the driver
  is named explicitly too, for consistency. *(my call)*
- **A driver that defines no options still validates**: a key under its prefix
  is unknown to it and throws.
- **Boolean values** accept `true`/`false`/`1`/`0`, case-insensitive. Anything
  else throws. *(my call)*
- Errors are `slideio::RuntimeError` via `RAISE_RUNTIME_ERROR`, like other open
  failures, with a message that names the key and the driver.

## 6. Internal plumbing

All of these are internal headers (`imagedriver.hpp`, `cvscene.hpp`,
`cvslide.hpp` and `imagedrivermanager.hpp` do not ship), so nothing here touches
the public surface or `BREAKING_CHANGES.md`.

- `ImageDriverManager::openSlide(path, driver, options)` validates prefixes (§5),
  selects the driver exactly as today, and strips its options down to the chosen
  driver's keys.
- `ImageDriver` gains a non-pure virtual
  `openFile(const std::string& filePath, const std::map<std::string, std::string>& options)`.
  The base implementation throws on any key and otherwise calls
  `openFile(filePath)`, so the 15 other registered driver classes change nothing.
  MRXS overrides it.
  - The driver receives its keys without the prefix (`crop_to_content`). It has
    no use for its own name. *(my call)*
  - A driver that overrides only one `openFile` hides the other from name lookup
    on the derived type. The manager calls through `ImageDriver*`, so this does
    not matter there. MRXS should add `using ImageDriver::openFile;` or override
    both.
- **Options must survive the converter's re-open.** `TiffConverter::tryReopenScene`
  re-opens the origin scene with `openSlide(filePath, driverId)`. `CVScene` gains
  `virtual std::map<std::string, std::string> getOpenOptions() const`, empty by
  default and returning full prefixed keys. `tryReopenScene` passes the result to
  the three-argument `openSlide`. A driver that honours options must override it.
  `describesSameRaster` remains the safety net: a forgotten override still
  converts correctly, just over one shared scene, and a test (§9) pins the MRXS
  case.
- `TransformerScene` needs nothing: the converter already walks to the origin
  scene before re-opening.

## 7. MRXS `crop_to_content`

### 7.0 Default

The option defaults to `true`, decided by the user on 2026-11-15. Both driver
entry points apply it: `MRXSImageDriver::openFile(path)` and
`openFile(path, options)` with the key absent both crop. So
`openSlide(path)`, `openSlide(path, "MRXS")` and
`openSlide(path, "AUTO", {})` all return the cropped scene. Below the driver,
the internal building blocks state the choice explicitly:
- `MRXSSlide::openFile` takes the flag with no default.
- `MRXSDataset::open` keeps `false` as its default. Its own tests describe the
  nominal geometry, and it is not reachable from the public API.

Existing tests that assert the nominal canvas, including the corpus suites that
compare against OpenSlide's level dimensions, pass `crop_to_content=false`
explicitly.

### 7.1 The crop box

- **C** = `bounds0` rounded outward to integers (exactly as `contentBounds` is
  today), intersected with the level-0 canvas `[0, w0) × [0, h0)`. The clip is
  needed: bounds can be negative or overhang the canvas, e.g. the `rawmap-d2`
  fixture gives `contentBounds` starting at (-3, -2).
- If there are no level-0 drawables, or the intersection is empty, the option is
  accepted and does nothing: the scene stays the full canvas. *(my call: an error
  would make a batch with one blank slide fail.)*

### 7.2 What the scene reports

With the option on:
- `getRect()` = `(0, 0, C.width, C.height)`. Coordinates are relative to the box
  corner. *(my call: origin (0, 0) rather than `(C.x, C.y)`, because callers
  treat the scene rect's size as the readable area starting at 0.)*
- Level *l* with downsample *d*: its box is
  `x0 = floor(C.x / d)`, `y0 = floor(C.y / d)`,
  `x1 = min(ceil((C.x + C.width) / d), level width)`,
  `y1 = min(ceil((C.y + C.height) / d), level height)`.
  Its `LevelInfo` size is `(x1 - x0, y1 - y0)`. The level's scale and
  magnification are unchanged. Level 0's box is C itself.
- Resolution, magnification, compression, channels, background and auxiliary
  images are unchanged.
- Metadata: `mrxs.contentBounds` and `mrxs.nominalSize` keep their meaning
  (nominal level-0 canvas coordinates). A new `mrxs.crop = [x, y, width, height]`
  appears only when the crop is active, so annotations from a full-canvas tool
  can be translated. *(my call)*
- **The crop state lives in the `MRXSDataset`**, decided at open time from the
  option and computed once (C and the per-level boxes). `MRXSSlide` and `MRXSScene`
  both build their metadata from `describe(*m_dataset)`, so slide and scene
  metadata agree by construction. *(my call: one open slide has one crop, and the
  dataset is already the per-open-slide state both share.)*

### 7.3 Reading

MRXS overrides both read entry points, and each one translates in its own
coordinate space:

- **Level-addressed reads** (`readResampledLevelBlockChannelsEx`): the rect is in
  level *l* pixels. Add the level box's `(x0, y0)` and clamp against that box
  instead of `(0, 0, level width, level height)`.
- **Scaled reads** (`readResampledBlockChannelsEx`): the rect is in scene
  (level-0) coordinates. Add `(C.x, C.y)` and clamp against C **before** level
  selection and scaling, then continue exactly as the uncropped path does.
  Translating after scaling would be wrong: `floor(C.x/d) + ceil(W/d)` is not
  always `ceil((C.x + W)/d)`, so the cropped read could cover one more or one
  fewer source column than the uncropped read of the same box, and the resized
  output would differ. *(my call: translate in level-0 space so the result is
  exact rather than ±1.)*

The invariants tests rely on, both bit-exact:
- a cropped level read of rect R at level *l* equals an uncropped level read of R
  shifted by that level's `(x0, y0)`;
- a cropped scaled read of rect R at any output size equals an uncropped scaled
  read of R shifted by `(C.x, C.y)` at the same output size.

`supportsConcurrentReads()` is unaffected. The crop is immutable per scene.

### 7.4 Re-open

`MRXSScene::getOpenOptions()` always returns the key explicitly, as
`{"mrxs.crop_to_content", "true"}` or `{"mrxs.crop_to_content", "false"}`.
It reports what was asked for (or defaulted to), not whether a box was found, so
a re-open makes the same decision. Being explicit keeps a re-open correct
whatever the default is.

## 8. Compatibility and documentation

- No `BREAKING_CHANGES.md` entry. The old `openSlide` symbol is untouched,
  `ImageDriver`/`CVScene` are internal, and MRXS has never been released.
- `CHANGELOG.md`: both items go under the existing `[2.11.0]` section. The MRXS
  driver line says it crops to the scanned area by default. A separate line
  covers open options.
- No version bump: this is part of 2.11.0. The `v2.11.0` tag was already pushed
  (at `73ea70ab`), with a draft release built from it. After the merge, the tag
  has to be moved to the new commit and the draft release rebuilt. That rewrites
  a pushed tag, so it needs the user's explicit go-ahead at the time.
- slideio-python ships its 2.11.0 with the `options` argument.
- `Booritas/slideio-doc`: the `open_slide` reference gains `options`, and the MRXS
  page documents the key, what the box is (scanned area, not tissue) and that
  coordinates become box-relative.
- Python: `slideio-python` docstrings and its own changelog.

## 9. Tests

C++, corpus-free. All MRXS ones use the committed fixtures in
`src/tests/mrxs/data/`, so none is named `MRXSCorpus*`.

Option handling (main suite):
- A key with no prefix throws, as does an unknown driver prefix.
- A key for another driver is ignored, with the driver named explicitly and with
  AUTO.
- A key under the prefix of a driver that defines no options throws when that
  driver opens the file.
- The two-argument `openSlide` behaves exactly as before. Existing suites cover
  this. Add one assertion that it equals the three-argument form with an empty
  map.

MRXS (`slideio_mrxs_tests`):
- An unknown `mrxs.` key and a malformed bool both throw.
- Crop on `rawmap-d2`: the scene size equals the clipped box. The clip at the
  negative origin is exercised. Its bounds are (-3, -2, 93, 70), so after the clip
  C starts at (0, 0) and every offset is zero. That makes it useless for the
  shift tests below.
- **The shift tests need a fixture whose content starts at a positive offset that
  does not divide by the coarser downsamples** (e.g. C.x = 37), so both the
  translation and the rounding in §7.3 are exercised. If none of the committed
  fixtures has one, add it through `auxfiles/mrxs-fixtures/generate.py` and
  commit the regenerated bytes (the directory is `-text`).
- On that fixture, for every level: the reported size matches §7.2, and a cropped
  level read equals the uncropped read shifted by `(x0, y0)`, bit for bit.
- On that fixture: a cropped scaled read, of the whole scene and of a sub-rect at
  a non-integer scale, equals the uncropped scaled read shifted by `(C.x, C.y)`,
  bit for bit.
- Slide metadata and scene metadata both carry the same `mrxs.crop`.
- With no option, the scene is cropped, identical to `true`. With `false`, it
  is the full canvas, metadata has no `mrxs.crop`, and `getOpenOptions()` says
  `false`. With the crop on, `mrxs.crop` equals C.
- The pre-existing MRXS tests that assert nominal geometry (scene, integration,
  render, bundle, and the `MRXSCorpus*` suites) still pass, opening with
  `crop_to_content=false`. The Python `test_cmu_slide` does the same, and gains a
  cropped counterpart.
- A fixture with no drawables (or a constructed geometry, if no fixture has
  none) gives the full canvas with the option on.
- `getOpenOptions()` round-trips. Converting a cropped scene re-opens cropped
  scenes rather than falling back to sharing. Assert that the re-open is used,
  not just that the output is correct.

Python (`slideio-python`): `options` dict passes through, bool and number
conversion, and `TypeError` on an unsupported value type.

## 10. Left to the builder

- Where the `std::map` alias lives for internal use, and whether core gets a
  named alias of its own.
- Prefix stripping (§6).
- Exact error message wording.
- Whether the validation helpers (prefix split, bool parse) live in
  `ImageDriverManager` or a small core utility a driver can reuse. The bool parse
  will be wanted by the next driver too.
