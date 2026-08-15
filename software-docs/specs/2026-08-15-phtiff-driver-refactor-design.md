# Philips TIFF Driver Refactor — Design

**Status:** Approved 2026-08-15
**Driver:** `src/slideio/drivers/svs`, `src/slideio/slideio/imagedrivermanager.cpp`
**Addresses:** the *Design and maintainability* section of the PHTIFF driver review

## 1. Goal

Separate the Philips TIFF format from the Aperio SVS format that currently
hosts it, without changing what either driver returns.

- No branch on a driver id remains in the svs driver. Format-specific
  behaviour is reached by virtual dispatch.
- The Philips metadata is parsed once per open instead of twice.
- `PHTDescription` stops being an exported class, and tinyxml2 leaves the
  include path of anything that links the svs driver.
- **No observable behaviour changes.** The existing suite passing unchanged is
  the acceptance criterion for the whole refactor.

## 2. Current state

PHTIFF is not a driver. It is `SVSImageDriver` constructed with a different id
string, and five places branch on a driver id — four on `PHTIFF_DRIVER_ID` and
one on `SVS_DRIVER_ID`:

| Location | Branch |
|---|---|
| `svsimagedriver.cpp:34` | `getFileSpecs` returns `*.svs` for the SVS id and `*.tif;*.tiff` for anything else |
| `svsimagedriver.cpp:45` | `canOpenFile` runs the content sniff only for the PHTIFF id |
| `svsslide.cpp:649` | `openFile` calls `initPhTiff` or `initSVS` |
| `svsslide.cpp:676` | `buildMetadataTree` builds the Philips tree or the Aperio one |
| `svstiledscene.cpp:39` | `processImageDescription` dispatches to the Philips or Aperio reader |

Consequences visible in the tree today:

- `svsslide.cpp` has grown from 411 to 690 lines, most of the growth Philips
  specific, in a file whose name says SVS.
- `getFileSpecs` treats *any* non-SVS id as Philips, so a third format added to
  this class would silently inherit the Philips file pattern.
- The Philips XML is parsed three times: `phExtractImages` (`svsslide.cpp:505`)
  and `processImageDescriptionPhTiff` (`svstiledscene.cpp:86`) at open, and
  `phBuildMetadataTree` (`svsslide.cpp:290`) lazily on `getMetadata()`.
  Philips-2.tiff's description is 844 KB.
- `PHTDescription` is exported with `SLIDEIO_SVS_EXPORTS`, has tinyxml2 in its
  public header and a `std::unique_ptr<tinyxml2::XMLDocument>` member — fragile
  across a DLL boundary under MSVC — while being used by nothing outside the svs
  library and the test binary. Every getter is non-`const`, and `getObjectList`
  `const_cast`s away constness to hand out mutable pointers from a read-only
  parse. `getDocument()`'s only caller is a commented-out line.
- 32 constants sit at `slideio` namespace scope in that public header.
  `THUMBNAIL`, `MACRO` and `LABEL` are additionally defined as `const char*` —
  a non-const pointer, so external linkage — at global scope in `svsslide.cpp`,
  `pkeslide.cpp` and `otslide.cpp`: the same symbol in three shared libraries.

Two items from the review's section are already resolved and are not restated
here: the auxiliary image names now follow the cross-driver `Macro`/`Label`/
`Thumbnail` convention, and the `std::list` and `imageWidthMap` items no longer
exist.

## 3. Scope

In scope:

- `PHTIFFImageDriver`, `PHTIFFSlide` and `PHTIFFTiledScene` as subclasses inside
  the existing svs driver library, replacing all five id branches.
- A `PHTMetadata` value struct parsed once per open.
- `PHTDescription` as an internal, non-exported, const-correct parser.
- Moving the 32 constants into a nested namespace, and giving the three
  duplicated globals internal linkage.
- `phExtractImages` becoming an ordinary member function.

Out of scope:

- **A separate `src/slideio/drivers/phtiff/` library.** Considered and rejected:
  PHTIFF shares its whole raster path with SVS — tile reading, `SVSSmallScene`,
  the TIFF handle plumbing — so a separate library would either duplicate that
  code or make one driver library link another, which no existing driver does.
  The subclass approach gets the same separation of *behaviour* without touching
  the pixel path.
- The embedded auxiliary rasters of Philips-1 and Philips-3, which are a feature
  rather than a refactor and need their own design.
- `3305` → `33005` in `svstiledscene.cpp:150` and `pketiledscene.cpp:92`. The
  review files it under minor items, but it sits in the `Compression::Unknown`
  fallback, so correcting it makes Aperio JP2K/RGB files report `Jpeg2000` where
  they now report `Unknown`. That is a visible change on the SVS and PKE paths,
  not the Philips one, and it belongs in its own commit with its own test rather
  than riding along in a rename sweep.

## 4. Approach

### 4.1 Class structure

```
ImageDriver
└── SVSImageDriver          *.svs, extension only
    └── PHTIFFImageDriver   *.tif;*.tiff, content sniff

CVSlide
└── SVSSlide                init() = Aperio, buildMetadataTree() = Aperio
    └── PHTIFFSlide         init() = Philips, buildMetadataTree() = Philips

SVSScene
└── SVSTiledScene           processImageDescription() = Aperio
    └── PHTIFFTiledScene    processImageDescription() = Philips
```

`SVSImageDriver` loses its `driverId` constructor parameter; each class reports
its own id. `ImageDriverManager` constructs `PHTIFFImageDriver` instead of
`SVSImageDriver(PHTIFF_DRIVER_ID)`. `PHTIFF_DRIVER_ID` survives only as that
driver's `getID()` value, so the id remains the public contract it is today —
`openSlide(path, "PHTIFF")` keeps working — while no longer steering control
flow.

The open sequence stays in one place. `SVSSlide` gains:

```cpp
protected:
    // Opens the file, scans the directories, and hands them to slide->init().
    static std::shared_ptr<SVSSlide> openFile(const std::string& filePath,
                                              std::shared_ptr<SVSSlide> slide);
    virtual void init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper);
```

Each driver's `openFile` supplies an instance of the right class. `init` is
today's `initSVS` on the base and today's `initPhTiff` on `PHTIFFSlide`.

These move from `svsslide.cpp` to `phtiffslide.cpp`: `phExtractImages` (as a
member, not a static), `phCreateImageScene`, `phCreateAuxScenes`,
`phCropLevelPadding`, `phDeclaredLevels`, `phAuxImageName`, `phLevelContentSize`
and the metadata-tree helpers. `svsslide.cpp` returns to roughly 400 lines.

### 4.2 Initialization order

`SVSTiledScene`'s constructors call `initialize()`, which calls
`processImageDescription()`. **A virtual call from a base class constructor does
not dispatch to a derived override.** Making `processImageDescription` virtual
without addressing this would leave PHTIFF scenes silently running the Aperio
reader.

A derived constructor body is also too late: `initialize()` fills `m_levels`
using `m_magnification`, so the Philips values must be known before the levels
are built.

Both constructors therefore stop calling `initialize()`, and each concrete scene
class gains a static factory that constructs and then initializes:

```cpp
static std::shared_ptr<SVSTiledScene> create(...);
static std::shared_ptr<PHTIFFTiledScene> create(..., const PHTMetadata& metadata);
```

A factory rather than a bare two-phase init, so no later caller can construct a
scene and forget to initialize it. `initialize()` becomes protected.

This is the change most likely to fail quietly, and it is the reason the
existing test suite — not new tests — is the acceptance criterion.

### 4.3 Parse once

`PHTMetadata` is a plain value struct with no tinyxml2 in its header:

```cpp
struct PHTLevelDeclaration {
    int number = 0;
    cv::Size declaredSize = {};       // zero when the metadata omits it
    Resolution spacing = {};
};

struct PHTImageDeclaration {
    std::string type;                 // "WSI", "MACROIMAGE", "LABELIMAGE", ...
    cv::Size size = {};
    Resolution spacing = {};
    std::vector<PHTLevelDeclaration> levels;   // empty for auxiliary images
};

struct PHTMetadata {
    std::vector<PHTImageDeclaration> images;
    const PHTImageDeclaration* wholeSlideImage() const;   // null when absent
};
```

`PHTIFFSlide::init` builds it once from the IFD 0 description and passes it to
both `phExtractImages` and `PHTIFFTiledScene::create`. Open-time parses go from
two to one.

The third parse stays where it is. `buildMetadataTree` is lazy and needs nearly
the whole document; folding it into `PHTMetadata` would mean parsing all 844 KB
of Philips-2's description on every open, including for the callers that never
ask for metadata. Trading a conditional cost for an unconditional one is the
wrong direction. The target is therefore *one parse per open, plus one more only
if `getMetadata()` is called* — not one parse ever.

Every field is optional in the same sense as today: an attribute the file does
not carry leaves its member at the default, and the skip-with-warning behaviour
added for the robustness findings moves into the parse unchanged.

### 4.4 PHTDescription

- Drop `SLIDEIO_SVS_EXPORTS`.
- Drop `getDocument()`.
- Make every getter `const` and return `const tinyxml2::XMLElement*`, which
  removes the `const_cast`.
- Add `phtdescription.cpp` to `slideio_phtiff_tests`' sources so its ~40 unit
  tests keep working verbatim.

The class becomes an implementation detail of `PHTIFFSlide`, whose only product
is a `PHTMetadata`. The cost is that the implementation is compiled twice in a
test build, which is the accepted price for keeping the test coverage intact.

### 4.5 Constants

The 32 constants move from `slideio` to `slideio::phtiff` as
`constexpr std::string_view`, which also ends the per-translation-unit static
init copies. This changes `PHTDescription::Attribute`'s members from
`std::string` to `std::string_view`; the comparisons against them are already
by value, so the change is mechanical.

`THUMBNAIL`, `MACRO` and `LABEL` become `constexpr` (internal linkage) in
`svsslide.cpp`, `pkeslide.cpp` and `otslide.cpp`. Touching the pke and ome-tiff
drivers is outside PHTIFF, but it is the same defect, the review names those
files, and the change is three lines per file with no behaviour attached.

## 5. Sequencing

1. **Constants and internal linkage.** Mechanical, no behaviour, lands first so
   it is not noise in the restructuring diff.
2. **The restructure.** Subclasses, initialization order, parse-once and
   `PHTDescription` together, since each depends on the others: parse-once needs
   the slide to construct the scene, and the value struct it introduces is what
   lets tinyxml2 leave the public header.

`3305` → `33005` is a third, independent commit, sequenced whenever its
behaviour change is acceptable to land.

## 6. Testing

The refactor changes no behaviour, so the existing suites are the acceptance
criterion and must pass unchanged:

- `slideio_phtiff_tests` — 92 tests
- `slideio_tests` — 489 tests
- `slideio_pke_tests` and `slideio_ometiff_tests` for the constants work

Test changes required by the move:

- `MockSVSSlide` becomes `MockPHTIFFSlide`, since the protected members it
  reaches (`phExtractImages`, `phCreateImageScene`, `phCreateAuxScenes`,
  `initPhTiff`) move to `PHTIFFSlide`. Mechanical, roughly 20 call sites.
- `aFailedOpenDoesNotLeaveTheFileOpen` calls `SVSSlide::openFile(path, id)`
  directly and follows the new signature.
- `phtdescription.cpp` joins the test target's sources.

New tests:

- `PHTMetadata` parsing: the images and their declared levels from
  `createFakeXml`, an absent attribute leaving a default, and a malformed
  description raising.
- `ImageDriverManager::findDriver` returns a driver whose `getID()` is
  `"PHTIFF"` for a Philips file, and `openSlide` on one yields a `PHTIFFSlide`.
- `PHTIFFImageDriver::getFileSpecs` returns the TIFF patterns and
  `SVSImageDriver::getFileSpecs` returns `*.svs`, so the removed
  "any non-SVS id is Philips" fallback cannot come back.

## 7. Risks

| Risk | Handling |
|---|---|
| Virtual call from a base constructor silently not dispatching | Factories, section 4.2. The Philips resolution and magnification assertions in the existing suite catch a regression immediately. |
| `SVSSlide::openFile`'s signature is used directly by a test | Updated with the change; it is not part of the public `slideio.hpp` API. |
| The `Attribute` member type change touching every constant | Mechanical and compiler-checked; a missed site fails to build rather than misbehaving. |
| Two copies of `phtdescription.cpp` in a test build | Accepted, and confined to the test binary. |
| Refactor drifting into behaviour change | The suites above must pass unchanged. Any diff in test expectations is a signal to stop, not to update the test. |
