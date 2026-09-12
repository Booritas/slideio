# Colour / ICC handling in the public API

Date: 2026-09-12
Branch: v2.10.0
Status: design approved, not yet implemented

## Purpose

Give machine-learning callers pixels that mean the same thing across scanners.

Today slideio hands back whatever RGB the file happened to store. An SVS from an
Aperio scanner and an NDPI from a Hamamatsu scanner, imaging the same H&E slide,
produce measurably different RGB, and a model trained on one generalises poorly to
the other. Colour normalisation is currently left entirely to the caller, who has
no access to the colorimetric information the file already carries: nothing in
slideio reads an ICC profile from any of the twelve supported formats.

This design adds two things:

1. **Introspection** — `Scene` exposes the embedded ICC profile, as raw bytes and
   as parsed header facts, so a pipeline can audit and log what it has.
2. **Correction** — a `ColorManagement` scene transformation that converts blocks
   into a chosen device-independent target space using the embedded profile.

Stain normalisation (Macenko, Vahadane, Reinhard) is explicitly **not** specified
here, but is a requirement on the architecture: the hooks this design adds must
accommodate it without further plumbing. See "Extension: stain normalisation".

## Decisions taken

| Decision | Choice | Rejected alternatives |
|---|---|---|
| Delivery shape | Scene decorator via `transformScene`, plus introspection getters on `Scene` | Mutable colour mode on `Scene`; profile access with no engine |
| ICC engine | lcms2 (`lcms/2.16`) from conan center | Hand-rolled matrix/TRC maths; lcms2 as an `extern/` submodule |
| Missing profile | Configurable policy, default `AssumeSRGB` | Always assume sRGB; always fail; shipping a vendor fallback profile table |
| Target spaces | sRGB, CIELAB, linear RGB, CIEXYZ | sRGB only |
| Driver coverage | All twelve formats | TIFF family only; a narrow proof slice |
| Module placement | Split across core / imagetools / transformer / slideio | A new `slideio-color` library; everything in the transformer |

Two of these deserve their reasoning recorded, because both were chosen against a
more convenient option.

**No vendor fallback profile table.** It was tempting to ship known-scanner
profiles keyed by detected vendor and model, to cover the many files that embed
nothing. It was rejected because it asserts colorimetry that was never measured on
the specific device that produced the slide. In a regulated imaging context that is
a claim the library is not in a position to make.
`ColorManagement::setSourceProfileOverride` is the honest substitute: a lab that
has actually characterised its scanner supplies the profile and owns the claim.

**`AssumeSRGB` is the default, but absence is always visible.** A batch read over a
mixed corpus must not die on the first unprofiled file, so the default policy lets
every read succeed. The risk that creates — a pipeline believing it normalised
across scanners when for half the corpus nothing happened — is answered by making
absence reportable rather than by making reads fail: `ColorProfileInfo::present`
and `ColorProfileSource::Assumed` say plainly what occurred, and callers that need
the guarantee opt into `MissingProfilePolicy::Fail`.

## Module placement

lcms2 must not be visible to the twelve drivers, and `slideio-core` must stay free
of heavy dependencies. That constraint drives the whole layout.

| Module | Adds | Sees lcms2 |
|---|---|---|
| `slideio-core` | `ColorProfile`, `ColorProfileInfo`, `ColorTarget`, `IccColorSpace`, `RenderingIntent`, `ColorProfileSource`; one virtual on `CVScene` | no |
| `slideio-imagetools` | `IccTransform` (engine, header parsing, sRGB synthesis) | **yes, exclusively** |
| `slideio-transformer` | `ColorManagement`, `ColorManagementWrap`, `TransformationType::ColorManagement`, `TransformationEx::bindToSource` | no |
| `slideio` | `Scene::getColorProfile`, `Scene::getColorProfileInfo` | no |
| drivers (11) | profile extraction only, as raw bytes | no |

The existing link graph supports this unchanged: `slideio` links `imagetools`, and
`transformer` links `core` and `slideio`, reaching imagetools transitively. The
transformer's `target_link_libraries` gains an explicit `${IMAGETOOLS_LIB_NAME}`
rather than relying on transitivity.

`lcms2` is added to `src/slideio/imagetools/conanfile.py` and linked `PRIVATE` into
`slideio-imagetools`. It resolves from conan center, so the CLAUDE.md dependency
rule is satisfied with no `extern/` submodule and no private remote.

## Core types

`src/slideio/core/colorprofile.hpp`, new file. No parsing, no lcms2 — this header
is included by drivers, which must stay engine-free.

```cpp
namespace slideio {

/**@brief where a scene's colour profile came from */
enum class ColorProfileSource {
    None,      // the file carries no profile
    Embedded,  // a real ICC profile read out of the file
    Assumed,   // none embedded; sRGB assumed under MissingProfilePolicy
};

/**@brief colour space of ICC profile data. Distinct from the transformer's
 * ColorSpace enum, which describes OpenCV conversions and is unaffected. */
enum class IccColorSpace { Unknown, Gray, RGB, CMYK, Lab, XYZ, YCbCr };

enum class RenderingIntent {
    Perceptual, RelativeColorimetric, Saturation, AbsoluteColorimetric
};

/**@brief device-independent space a scene's pixels may be converted into */
enum class ColorTarget { sRGB, LinearRGB, Lab, XYZ };

/**@brief raw ICC profile bytes as found in a slide.
 *
 * A byte container only: it does not parse or validate its contents. Use
 * Scene::getColorProfileInfo() for the parsed header, or hand getData() to an
 * external colour management system. Parsing lives in slideio-imagetools
 * because it needs lcms2, which neither this module nor any driver may see.
 */
class SLIDEIO_CORE_EXPORTS ColorProfile
{
public:
    ColorProfile();                                     // absent
    explicit ColorProfile(std::vector<uint8_t> iccBytes);
    bool isEmpty() const;
    ColorProfileSource getSource() const;
    void setSource(ColorProfileSource source);
    const std::vector<uint8_t>& getData() const;
    size_t getSize() const;
};

/**@brief parsed ICC header facts. Populated by slideio-imagetools. */
struct SLIDEIO_CORE_EXPORTS ColorProfileInfo
{
    bool present = false;
    ColorProfileSource source = ColorProfileSource::None;
    std::string description;                  // 'desc' tag, e.g. "Aperio RGB"
    std::string manufacturer;
    std::string model;
    std::string version;
    IccColorSpace dataSpace = IccColorSpace::Unknown;
    IccColorSpace connectionSpace = IccColorSpace::Unknown;
    RenderingIntent intent = RenderingIntent::RelativeColorimetric;
    std::array<double,3> whitePoint{0.0, 0.0, 0.0};   // XYZ
    size_t dataSize = 0;
    std::string toString() const;
};
}
```

`ColorProfile::sRGB()` is deliberately absent: synthesising an sRGB profile needs
`cmsCreate_sRGBProfile`, so that factory is `IccTransform::createSRGBProfile()`.

## Scene surface

One new virtual on `CVScene`, defaulted so all eleven driver libraries compile
untouched:

```cpp
// cvscene.hpp
virtual ColorProfile getColorProfile() const { return ColorProfile(); }
```

Per **scene**, not per slide: a label and a macro image are captured through
different optics than the tissue scan, and a multi-scene SCN may legitimately
differ. A driver that finds the profile at file level hands the same one to each
scene it constructs.

Public API on `Scene`, wrapped 1:1 in `PyScene`:

```cpp
ColorProfile     getColorProfile() const;      // raw bytes, possibly empty
ColorProfileInfo getColorProfileInfo() const;  // parsed; present=false if absent
```

`getColorProfileInfo()` is implemented in `slideio` (which links imagetools) by
calling `IccTransform::describe()` on the bytes the scene returns.

## The engine

`src/slideio/imagetools/icctransform.hpp` — the only file in the project that
includes `lcms2.h`.

```cpp
class SLIDEIO_IMAGETOOLS_EXPORTS IccTransform
{
public:
    IccTransform(const ColorProfile& source, ColorTarget target,
                 RenderingIntent intent, bool blackPointCompensation,
                 DataType sourceType);
    void apply(const cv::Mat& src, cv::OutputArray dst) const;
    DataType getOutputDataType() const;
    static ColorProfileInfo describe(const ColorProfile& profile);
    static ColorProfile createSRGBProfile();
};
```

The `cmsHTRANSFORM` is compiled once in the constructor and held immutable. `apply`
is `const` and performs no allocation beyond the output array.

`describe()` parses the ICC header for every field of `ColorProfileInfo` except
`source`, which it copies from `ColorProfile::getSource()` — provenance is known to
the caller that produced the bytes, not discoverable from the bytes themselves. An
empty `ColorProfile` yields `present == false` and `source == None`.

| Target | lcms2 pixel format | Output dtype | Range |
|---|---|---|---|
| `sRGB` | `TYPE_RGB_8` / `TYPE_RGB_16` | same as source | as source |
| `LinearRGB` | `TYPE_RGB_FLT` | `DT_Float32` | [0, 1] |
| `Lab` | `TYPE_Lab_FLT` | `DT_Float32` | L [0,100], a/b [-128,127] |
| `XYZ` | `TYPE_XYZ_FLT` | `DT_Float32` | [0, ~1.1] |

`LinearRGB` uses a synthetic profile with sRGB primaries and a linear (gamma 1.0)
tone response curve.

**Channel order.** slideio block buffers are channel-interleaved in scene channel
order, which for these scenes is RGB — *not* OpenCV's BGR convention. The engine
feeds `TYPE_RGB_*` and never reorders. Getting this wrong produces output that is
the right shape and dtype and looks entirely plausible, so it is covered by an
explicit asymmetric-patch test rather than left to review.

## The transformation

### Binding

`TransformationEx::applyTransformation(const cv::Mat&, cv::OutputArray)` receives
only the pixel block; it never sees the source scene
(`src/slideio/transformer/transformationex.hpp`). A colour-management
transformation needs the source ICC profile, so one new virtual is added:

```cpp
/**@brief returns a copy of this transformation specialised to a source scene.
 *
 * The default returns nullptr, meaning the transformation needs no binding and
 * is used as-is. A transformation whose behaviour depends on the source image --
 * colour management on the source ICC profile, stain normalisation on source
 * statistics -- overrides it and returns a new, fully prepared object.
 */
virtual std::shared_ptr<TransformationEx> bindToSource(const CVScene& source) const {
    return nullptr;
}

/**@brief lets a bound transformation amend the colour profile its scene reports.
 *
 * The default returns the input unchanged. Colour management overrides it to
 * record that it substituted an assumed sRGB profile, so a caller can tell a
 * real correction from an assumed one without knowing which transformation
 * performed it.
 */
virtual ColorProfile amendColorProfile(const ColorProfile& input) const {
    return input;
}
```

`TransformerScene`'s constructor maps its transformation list through
`bindToSource` before `initChannels()` and `computeInflationValue()`, keeping the
original wherever nullptr is returned. It reuses the existing
`dynamic_cast<TransformationEx*>` dispatch idiom already present in
`transformerscene.cpp` at the three sites that apply transformations, compute
channel data types and compute the inflation value. All seven existing filters are
unaffected.

It returns a bound **copy** rather than mutating `this`, and that is load-bearing.
The user's `ColorManagement` object stays pure configuration, so one object can be
passed to `transformScene` in a loop over hundreds of slides and each scene gets
its own compiled transform. A mutating hook would make that loop last-bind-wins,
and since the list holds `shared_ptr`s shared between scenes, racy.

### ColorManagement

`src/slideio/transformer/colormanagement.hpp`:

```cpp
enum class MissingProfilePolicy { AssumeSRGB, PassThrough, Fail };

class SLIDEIO_TRANSFORMER_EXPORTS ColorManagement : public TransformationEx
{
public:
    ColorManagement();   // sRGB, AssumeSRGB, RelativeColorimetric, BPC on
    explicit ColorManagement(ColorTarget target);
    // copy / move / assignment mirroring ColorTransformation

    ColorTarget          getTarget() const;                  void setTarget(ColorTarget);
    RenderingIntent      getIntent() const;                  void setIntent(RenderingIntent);
    bool                 getBlackPointCompensation() const;  void setBlackPointCompensation(bool);
    MissingProfilePolicy getMissingProfilePolicy() const;
    void                 setMissingProfilePolicy(MissingProfilePolicy);
    const ColorProfile&  getSourceProfileOverride() const;
    void                 setSourceProfileOverride(const ColorProfile&);

    std::shared_ptr<TransformationEx> bindToSource(const CVScene&) const override;
    ColorProfile amendColorProfile(const ColorProfile& input) const override;
    void applyTransformation(const cv::Mat&, cv::OutputArray) const override;
    std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>&) const override;
};
```

`ColorManagementWrap` mirrors `ColorTransformationWrap` exactly, and
`TransformationType` gains `ColorManagement`.

Defaults are chosen so that `ColorManagement()` with no configuration is the safe,
useful case: convert to sRGB, assume sRGB when nothing is embedded, relative
colorimetric intent with black point compensation — the combination that keeps a
mixed-corpus batch running and produces uint8 output of the same shape the
uncorrected read produced.

### Applicability

Binding inspects the source scene. `ColorManagement` binds only when the scene is
colorimetric RGB:

- exactly 3 channels, and
- channel dtype `DT_Byte` or `DT_UInt16`, and
- if a profile is embedded, its `dataSpace` is `RGB`.

Anything else — a five-channel fluorescence CZI, an RGBA scene, a single-channel
grey scene — **throws at bind time**, with a message naming the channel count and
dtype.

This is deliberately not governed by `MissingProfilePolicy`. That policy answers
"there is no profile"; this is "this is the wrong kind of image". Letting a
fluorescence scene pass silently through would manufacture exactly the false
confidence the policy design set out to prevent: fluorescence channel intensities
are not colorimetric quantities and no ICC transform of them is meaningful.

Grey-source ICC support is a stated non-goal for v1.

### Policy mechanics

- **`AssumeSRGB`** (default) — source becomes `IccTransform::createSRGBProfile()`
  and `ColorProfileSource::Assumed` is reported. Target `sRGB` short-circuits to a
  copy; Lab, XYZ and LinearRGB remain real conversions.
- **`Fail`** — throws at bind time when nothing is embedded.
- **`PassThrough`** — decoded pixels are returned untouched.

`PassThrough` is **rejected at bind time for any target other than `sRGB`**.
Otherwise output dtype would depend on whether a given file happened to carry a
profile — `float32` for profiled files, `uint8` for unprofiled ones — and a batch
job would silently assemble tensors of two different dtypes. Refusing the
incoherent combination removes the trap rather than documenting it.

### What a managed scene reports

`TransformerScene::getColorProfile()` forwards the origin scene's profile through
`amendColorProfile` on each bound transformation. For a colour-managed scene that
means it reports the **source** profile, with `ColorProfileSource::Assumed` stamped
on it when `AssumeSRGB` substituted a synthetic profile.

Reporting the source rather than the target is the deliberate choice. The target is
already known to the caller — they set it — whereas the audit question an ML
pipeline actually needs answered is "did this file carry real colorimetry, or did
we assume it?". `managed.get_color_profile_info().source` answers exactly that, and
it is reachable from Python without the caller ever holding the bound copy, which
lives inside `TransformerScene`.

## Concurrency

`TransformerScene` does not override `supportsConcurrentReads()` and so inherits
`CVScene`'s `false`. Wrapping any scene in a transform therefore downgrades it to
serialised reads today — an SVS scene that reads concurrently stops doing so. For
an ML tile loader with several worker threads that is the difference between a
transform being usable and not.

This design fixes it: `TransformerScene::supportsConcurrentReads()` returns the
origin scene's value. This is sound because every transformation applies as a
`const`, stateless operation over a caller-supplied block, and `ColorManagement`'s
bound state — the compiled `cmsHTRANSFORM` — is immutable after binding. lcms2
documents `cmsDoTransform` as re-entrant on a shared handle; the implementation
plan verifies this under a stress test and the TSan gate rather than relying on
the documentation.

The fix also un-serialises the seven existing filters. That is an improvement but a
behaviour change, and gets an entry in `software-docs/BREAKING_CHANGES.md` under
branch `v2.10.0`.

No `thread_local` is introduced anywhere, and no file handle is involved, so the
CLAUDE.md constraint on `thread_local` file state does not apply. `ContextPool` is
not needed: the transform handle is shared read-only rather than borrowed per
thread. If the stress test disproves the lcms2 re-entrancy claim, `ContextPool`
holding one `cmsHTRANSFORM` per borrower is the fallback, per the CLAUDE.md
concurrency contract.

## Driver extraction

A single shared change covers most formats: `TiffDirectory`
(`src/slideio/imagetools/tifftools.hpp`) gains `std::vector<uint8_t> iccProfile`,
populated from `TIFFTAG_ICCPROFILE` (34675) in `TiffTools::scanTiffDirTags`. Five
driver libraries call that function and are served by it.

| Format | Driver | Source | Work |
|---|---|---|---|
| SVS | svs | `TIFFTAG_ICCPROFILE` | shared `TiffTools` |
| PHTIFF | svs | `TIFFTAG_ICCPROFILE` | shared |
| QPTIFF | pke | `TIFFTAG_ICCPROFILE` | shared |
| OME-TIFF | ome-tiff | `TIFFTAG_ICCPROFILE` | shared |
| SCN | scn | `TIFFTAG_ICCPROFILE` | shared |
| VSI | vsi | tag on TIFF parts; ETS carries none | shared; absent for ETS scenes |
| AFI | afi | delegates to SVS scenes | none, inherited |
| NDPI | ndpi | same tag, but the driver has its own `NDPITiffDirectory` and `NDPITiffTools::scanTiffDirTags` | parallel change in the ndpi driver |
| DCM | dcm | ICC Profile `(0028,2000)` in Optical Path Sequence `(0048,0105)` | DCMTK `findAndGetUint8Array` |
| GDAL | gdal | metadata domain `COLOR_PROFILE`, item `SOURCE_ICC_PROFILE` (base64) | `GetMetadataItem` plus base64 decode |
| CZI | czi | the format carries no ICC profile | none; default returns absent |
| ZVI | zvi | the format carries no ICC profile | none; default returns absent |

Four entries need no code: AFI, CZI, ZVI, and ETS-only VSI scenes. The defaulted
virtual already answers correctly for them. "All twelve formats" therefore means
all twelve answer the question honestly — and for CZI and ZVI the honest answer,
`present == false`, is a useful result rather than a gap.

DICOM is the only format that specifies ICC placement normatively, so it is also
the reference case for the extraction tests.

## Errors

Every condition below is raised at **bind time**, inside `transformScene`, before
any pixel is read, so a batch job fails on the file it cannot handle rather than
several thousand tiles later:

- scene is not colorimetric RGB (channel count, dtype, or profile `dataSpace`)
- `MissingProfilePolicy::Fail` with no embedded profile
- `PassThrough` combined with a non-`sRGB` target

Corrupt or unparseable ICC bytes are **not** a hard failure. They are logged at
warning level, `ColorProfileInfo::present` becomes `false`, and the missing-profile
policy then applies — so `AssumeSRGB` keeps a batch running while `Fail` still
stops it. `Scene::getColorProfile()` continues to return the raw bytes, so a bad
profile can be inspected rather than merely reported.

## Python API

No new binding file; `pyscene.*` and `pybind.cpp` gain the surface below.

```python
import slideio as sld

scene = sld.open_slide("tumour.svs").get_scene(0)

# introspection, available without any transform
icc  = scene.get_color_profile()        # bytes | None
info = scene.get_color_profile_info()   # ColorProfileInfo

# enums
sld.ColorTarget.SRGB / LINEAR_RGB / LAB / XYZ
sld.RenderingIntent.PERCEPTUAL / RELATIVE_COLORIMETRIC / SATURATION / ABSOLUTE_COLORIMETRIC
sld.ColorProfileSource.NONE / EMBEDDED / ASSUMED
sld.MissingProfilePolicy.ASSUME_SRGB / PASS_THROUGH / FAIL
sld.IccColorSpace.RGB / GRAY / CMYK / LAB / XYZ / YCBCR / UNKNOWN

# the working path
cm = sld.ColorManagement(target=sld.ColorTarget.LAB)
cm.missing_profile_policy   = sld.MissingProfilePolicy.FAIL
cm.intent                   = sld.RenderingIntent.RELATIVE_COLORIMETRIC
cm.black_point_compensation = True

managed = sld.transform_scene(scene, cm)
tile = managed.read_block((0, 0, 1024, 1024), size=(512, 512))   # float32 Lab

info = managed.get_color_profile_info()
print(info.source)   # EMBEDDED: real colorimetry. ASSUMED: sRGB was substituted.
```

`ColorProfile` is exposed to Python as `bytes`, and as `None` when absent rather
than as an empty `bytes`, so `if icc is None` reads naturally. Plain `bytes` is
chosen because `PIL.ImageCms.ImageCmsProfile(io.BytesIO(icc))` and
`colour.io.read_ICC` consume it directly: a pipeline that prefers to do its own
colour maths gets full interoperability without slideio mediating.

Accessor style — properties versus `get_`/`set_` — follows whatever the existing
filter bindings in `pybind.cpp` already do, so `ColorManagement` reads like
`GaussianBlurFilter` rather than introducing a second convention.

`ColorProfileInfo` gets a `__repr__` via its `toString()`.

## Testing

No new test executable. Tests land in the suites CLAUDE.md already defines.

**`slideio_tests`** (covers imagetools):

- `IccTransform` against profiles synthesised with lcms2, requiring no slide files.
- Known-patch colorimetry: white to L≈100, a≈0, b≈0; 50% grey to L≈53.6;
  sRGB to Lab and back within tolerance.
- Channel-order regression: a `(255,0,0)` patch must emerge red. A BGR mix-up
  yields output of the right shape and dtype that looks plausible, so no
  shape-or-dtype assertion would catch it.
- `IccTransform::describe()` against profiles with known `desc`, whitepoint and
  PCS; and against deliberately truncated bytes, which must report
  `present == false` rather than throw.

**`slideio_transformer_tests`**:

- The policy matrix: 3 policies × 4 targets × {profiled, unprofiled}, against a
  synthetic scene.
- Bind-time error cases: non-RGB shape, `Fail` with no profile, `PassThrough`
  with a non-sRGB target.
- `computeChannelDataTypes` declaring the correct dtype per target.
- `bindToSource` returning nullptr, and `amendColorProfile` returning its input
  unchanged, for all seven existing filters, so they remain untouched.
- A colour-managed scene reports `source == Embedded` over a profiled origin and
  `source == Assumed` over an unprofiled one, under `AssumeSRGB`.
- A bound copy does not disturb the configuration object it came from, and two
  scenes bound from one `ColorManagement` hold independent transforms.

**Per-driver suites** — extraction from one real file of each format, guarded by
`SLIDEIO_SKIP_MISSING_IMAGES` per the existing convention. Regenerate
`software-docs/TEST_IMAGES.md` with `python3 auxfiles/list-test-images.py` if any
new image is added.

**Concurrency** — an N-thread stress read over a colour-managed scene, plus the
TSan gate already established on this branch for ZVI, to verify the
`cmsDoTransform` re-entrancy claim rather than trusting the documentation.

## Phasing

Ordered so the first three steps ship value before the engine exists.

1. Core types, `CVScene::getColorProfile()` virtual, `Scene` getters, Python
   getters. Introspection alone is already useful: a pipeline can audit its corpus
   for profile coverage before any transform exists.
2. lcms2 into imagetools; `IccTransform` and `describe()`; `getColorProfileInfo()`
   becomes real.
3. Extraction across all drivers: shared `TiffTools`, then ndpi, dcm, gdal.
4. `bindToSource`, `ColorManagement`, `ColorManagementWrap`, Python binding.
5. `TransformerScene::supportsConcurrentReads()` forwarding, plus the
   `BREAKING_CHANGES.md` entry.

## Extension: stain normalisation

Not specified here, but the architecture must accommodate it, and does:

- `bindToSource` is the hook a stain normaliser needs to fit reference statistics
  once, at bind time, rather than per block.
- The Lab path from step 2 is the space Reinhard-style normalisation operates in.
- `computeChannelDataTypes` already lets a transformation declare float output,
  which stain deconvolution into optical-density space requires.
- Composition works through the existing `transformSceneEx`:

```python
sld.transform_scene_ex(scene, [
    sld.ColorManagement(sld.ColorTarget.LAB),
    sld.MacenkoNormalization(reference=ref),   # future
])
```

Optical density, `-log10(I/I0)`, is not an ICC concept and is deliberately not a
`ColorTarget`. It belongs to the stain layer, computed from linear RGB.

## Non-goals

- Stain normalisation and stain deconvolution algorithms (separate spec).
- Grey-source and CMYK-source ICC profiles.
- Writing or embedding profiles on the converter's TIFF output. The engine is
  reusable for it later; this design does not specify it.
- A vendor fallback profile table.
- Colour management of fluorescence or other non-colorimetric multichannel scenes.
- Per-channel colorimetry for multiplex imaging.

## Documentation

- Doxygen comments on every new public header.
- `docs-src/Sphinx/source/*.rst` for the Python surface. This is public.
- `software-docs/BREAKING_CHANGES.md`, branch `v2.10.0`, for the
  `supportsConcurrentReads` behaviour change.
- Nothing under `docs/`.
