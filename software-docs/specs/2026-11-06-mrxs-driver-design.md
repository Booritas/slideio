# MRXS reader driver design

Date: 2026-11-06, revised 2026-11-07 after review. Status: proposed; no driver implementation in this change.

## 1. Basis and goals

Primary input: [MIRAX/MRXS format description](../../../documents/image_formats/mirax/MIRAX_MRXS_Format.md), especially sections 2–13 and 15–17. That document distinguishes observed format structures, OpenSlide policies, and partially verified Java extensions. This design preserves those distinctions. Its sample results are source evidence, not tests performed for this proposal. The format document currently lives outside this repository; it will be moved under `software-docs/`, and this link must be updated when it is.

Repository integration was checked against `ImageDriver`, `CVSlide`, `CVScene`, `LevelInfo`, `FileReader`, `TileComposer`, the PKE driver, and `ImageDriverManager`. Implement a native C++ driver named `MRXS`, under `src/slideio/drivers/mrxs`, without a runtime dependency on OpenSlide. OpenSlide is an optional test oracle.

Goals: bounded random region access, correct sparse and fractional camera geometry at every stored level, existing SlideIO scene/channel APIs, useful diagnostics, and predictable resource consumption. Never load a whole-slide raster to answer a region request.

## 2. Release scope and capability contract

The first release exposes one 8-bit, three-channel RGB scene, one Z slice and one time frame. This is a rendered RGB view, including for supported fluorescence bundles; it does not claim independent spectral channels. Use channel names Red, Green, Blue. Reject a bundle that requires multiple packed groups to represent its declared logical channels rather than silently returning an incomplete dataset.

| Area | First release | Deferred capability |
|---|---|---|
| Access | Sibling `.mrxs` bundle; explicit `Slidedat.ini` path | Directory-path convenience |
| Index | `01.02`; zoom family at hierarchy zero; primary slots `l` | Other versions and general multidimensional slot mapping |
| Pyramid codecs | JPEG, opaque RGB-normalized PNG, documented BMP24 profile | JPEG2000, JPEGXR, higher sample depths |
| Positions | Raw map, zlib map, documented nominal fallback | Vendor hardware-coordinate stitching |
| Geometry | Sparse tiles, D subdivisions, omitted original levels, fractional placement | Unverified non-aligned divisions |
| Associated images | Optional JPEG macro, label, thumbnail | Whole-slide raster, scan maps, stage maps |
| Other layers | Preserve metadata; do not composite masks or interpret scan-info XML | Verified masks and logical multichannel support |

Hardware-coordinate and mask families alone do not prevent reading a supported primary RGB pyramid: retain them as ignored layers and report the rendering profile as primary RGB without vendor masks/hardware stitching. A second image-bearing channel group or unsupported required primary codec is an explicit unsupported-profile error. A second group is recognised from metadata alone, without decoding payloads: the sections of the `Slide filter level` family name the stored group holding each filter in `DATA_IN_THIS_FILTER_LEVEL`, and more than one distinct value means more than one packed image group (format description 12.4). Checked on 2026-11-07 against all nine integration bundles: only the five-channel mIHC bundle names two (`FilterLevel_0`, `FilterLevel_1`); the other eight, including the mask- and scan-info-bearing ones, name one. Do not apply ordinary zoom validation to unrelated hierarchical XML or mask descriptors.

Entry suffix matching is case-insensitive, a deliberate extension of OpenSlide detection. There are two ways in:

- **A `.mrxs` path.** The file must exist, but its bytes are not interpreted. The data directory is its sibling with the same stem.
- **A `Slidedat.ini` path.** Its parent is the data directory, and no `.mrxs` file is required; this is what supports extracted bundles whose entry file was dropped or sits inside the data directory.

Either way the scene's file path is the path the caller opened, and the scene name is the stem of the data directory. Never rearrange source files.

AUTO detection is split between the two driver entry points. `canOpenFile` matches names only: a `*.mrxs` path, or a file whose name is `Slidedat.ini` (case-insensitive); it must not claim arbitrary INI files, and it does no I/O. The structural probe runs in `openFile`, which therefore reports a recognized but corrupt MRXS with its own error. Probing inside `canOpenFile` would be wrong: a failed probe returns false, and `ImageDriverManager::findDriver` moves on to the next driver, handing a corrupt MRXS to GDAL or to "Cannot find driver". `findDriver` walks the hard-coded `driverOrder` array in `imagedrivermanager.cpp`, so `"MRXS"` must be added to it, before `"GDAL"`; registering the driver in `initialize()` alone leaves it unreachable from AUTO.

## 3. Components and ownership

All proposed internal types live in `slideio::mrxs`. The driver, slide and scene classes are exported with the driver's `SLIDEIO_MRXS_EXPORTS` macro, following existing driver conventions. The parser, geometry and renderer classes are also exported with it, because §9 tests them in isolation from the separate `slideio_mrxs_tests` executable, which can only reach exported symbols of a shared library. (The alternative, an object library linked into both the driver and the tests, is not used by any other driver; do not introduce it here.)

| Proposed type/file stem | Responsibility |
|---|---|
| `MRXSImageDriver` / `mrxsimagedriver` | ID, detection, file specifications, open dispatch |
| `MRXSSlide` / `mrxsslide` | Main scene and auxiliary registry; metadata tree |
| `MRXSScene` / `mrxsscene` | CVScene metadata and both region-read entry points |
| `MRXSAuxScene` / `mrxsauxscene` | Lazy bounded JPEG auxiliary reads |
| `MRXSDataset` / `mrxsdataset` | Immutable shared metadata, levels, positions, descriptors, file access |
| `MRXSMetadataReader` / `mrxsmetadata` | INI preprocessing, typed parsing, family/section indirection |
| `MRXSIndexReader` / `mrxsindex` | Checked little-endian reads and page traversal |
| `MRXSGeometry` / `mrxsgeometry` | Scale, active cameras, drawable generation, spatial lookup |
| `MRXSPayloadDecoder` / `mrxsdecoder` | Bounded streams and validated RGB decode |
| `MRXSRenderer` / `mrxsrenderer` | Fractional extraction, ordered alpha compositing, final RGB output |

Scenes and auxiliary scenes retain `shared_ptr<const MRXSDataset>` so they survive destruction of the slide wrapper. No scene retains a raw owning slide pointer. Metadata and spatial tables become immutable before open returns. Mutable file-handle and decoded-image caches live in a separate synchronized `MRXSRuntime` object. The dataset holds it as `std::shared_ptr<MRXSRuntime>`: constness of the dataset is shallow over that pointer, so a `const MRXSDataset` can still reach its caches without any `mutable` member, and the runtime's own mutex is the only thing guarding them.

Core records:

```cpp
struct PayloadRef { int fileNumber; uint64_t offset, length; };
struct EncodedImage { int level; int64_t imageIndex; PayloadRef payload; };
struct CameraPosition { int64_t x, y; uint8_t flags; };
struct Drawable {
    uint32_t encodedImageId;
    cv::Rect2d source, destination;
    int64_t gridRow, gridColumn;
};
```

The actual implementation must check container narrowing, including the proposed 32-bit image ID, rather than assume all datasets fit. Store encoded descriptors once. Avoid allocating a heap object for every potential subtile: retain compact descriptor tables and generate intersecting subtiles on demand, or use compact drawable arrays where measured memory allows it. Choose between these representations at the geometry milestone by measuring the TCIA bundle `003` (§9), the only million-descriptor sample. Both must produce identical ordering and query results.

## 4. Open and parse pipeline

1. Resolve entry and data directory; read `Slidedat.ini` with an explicit size limit. Start with 1 MiB for compatibility, report the limit clearly, and keep it a named internal policy constant.
2. Strip UTF-8 BOM and empty-key lines whose first non-whitespace character is `=`. Preserve group/key case, raw source metadata, and unknown entries. Use locale-independent numbers. Parse with a small driver-local reader rather than a new dependency: OpenSlide reads the file with GLib's `GKeyFile`, no generic INI library is known to match it, and GLib is not one of this project's dependencies. The reader follows `GKeyFile` semantics for repeated keys and repeated groups: a repeated key takes its last value, and a repeated group merges into the first. Both were confirmed against OpenSlide 4.0.1 on 2026-11-07 with the `ini-duplicates` fixture (§9); pin each rule with a unit test.
3. Resolve referenced sections, ordered data filenames, and index filename. Reject absolute filenames and normalized paths escaping the bundle, including symlink escapes. This containment rule is a driver policy; report it explicitly if encountered.
4. Validate positive dimensions and D, whole camera blocks (`Nx % D == 0` and `Ny % D == 0`), required fields, finite geometry, exponent bounds, and supported formats. Default D to one only when absent; malformed D fails. Require positive finite MPP when present as required level metadata. Invalid optional objective power remains unknown.
5. Read five version bytes and exactly the UTF-8 byte length of `SLIDE_ID`; compare both. Read table pointers at `5+S` and `9+S`, never fixed UUID offsets.
6. Traverse required primary levels, then selected nonhierarchical records. Validate file ranges without decoding image pixels. Build positions, level-zero active cameras, and all level geometry in that order.
7. Publish the immutable dataset and scene only after validation succeeds. Decode pyramid payloads and associated JPEG pixels lazily.

Use existing positional `FileReader`, not shared stream cursors or memory mapping. Parse packed fields explicitly as little-endian signed i32; promote to checked 64-bit arithmetic before addition/multiplication. Negative pointers, lengths, indices, and counts fail; signed camera coordinates remain valid. Validate ranges as `length <= size - offset` after checking `offset <= size`.

Hierarchical heads are `(0, firstPage)`. A null pointer or `(0,0)` is absent, never a request to parse offset zero. Each data page has an eight-byte header and `n*16` bytes of records. Follow absolute pointers with cycle detection and work budgets; use actual page counts, not `PAGEELEMENTCOUNT`. Note that OpenSlide 4.0.1 does not follow the next-page pointer: `process_hier_data_pages_from_indexfile` only tests it for zero and reads the following page directly after the current one. Every real bundle writes its pages contiguously, where the two readings agree; for an out-of-order chain the driver follows the pointers as the format description says, and the `pages-out-of-order` fixture tests that without an OpenSlide comparison. For the primary layer, validate image indices, cumulative-factor alignment, and duplicate spatial counters within each level. Treat a required empty primary level as unsupported rather than silently renumbering levels.

Nonhierarchical ordinals use the sum of preceding family counts. Their entries are 20 bytes, with two preserved tags followed by offset, length, file number. Enumerate safely, but selected position/associated objects use only the first entry of the first data page and require zero tags. Do not concatenate entries or apply zero-tag constraints to unknown records.

Limits must cover INI bytes, page visits, records, decoded dimensions, decoded bytes, position-map output, spatial-index bytes, handles, and cache bytes. Derive legitimate bounds from file sizes and validated metadata first; add configurable internal budgets with actionable errors. Do not reject the documented 10,592-entry page because nominal capacity is 128.

## 5. Geometry and coordinate contract

Use the source specification's section 9–10 algorithms directly as the compatibility contract, with independent synthetic tests. Important quantities are:

```text
E_l = sum(e_0 .. e_l); C_l = 2^E_l; d_l = C_l / C_0
Px = Nx / D; Py = Ny / D
positions_per_image = max(1, C_l / D)
images_per_position = max(1, D / C_l)
```

Require `E_l <= 30`, first exponent nonnegative, later exponents positive, and D/C alignment in either direction. Preserve each level's W, H and overlaps. Compute nominal original-lattice dimensions by the repeated integer-accumulation algorithm in section 9.2, including truncation after each fractional overlap subtraction; level dimensions are the resulting values divided by `C_l` with integer truncation. Reject nonpositive or API-unrepresentable dimensions. Retain populated bounds separately without shifting pixels to their minimum origin. OpenSlide publishes them as the union of the level-0 drawables rounded outward to whole pixels and not clipped to the canvas, so a negative camera origin gives a negative bounds origin; report them the same way.

Select raw positions if that family exists; otherwise compressed positions; otherwise synthesize nominal positions. A corrupt selected map fails without fallback. Maps contain exactly `9*Px*Py` bytes after bounded zlib-wrapped inflation. Validate flags as 0 or 1, but never interpret them as an activity mask. Multiply decoded signed coordinates by `C_0`; do not multiply synthesized coordinates by `C_0`.

Use the position-aware path if a standard map exists or either level-zero overlap is nonzero. Then `T=max(1,C_l/D)`, `Q=1`, and divisor `min(C_l,D)`. Otherwise use `T=1`, `Q=max(1,C_l/D)`, and divisor `C_l`. Drawable dimensions are `(W_l/T,H_l/T)`, and nominal advances subtract overlap divided by `images_per_position`.

For a descriptor `(x,y)` and subtile `(xi,yi)`, use `xx=x+xi*D`, `yy=y+yi*D`, clip to the original grid, and obtain camera `(xp,yp)=(xx/D,yy/D)`. Internal origin is camera position plus `(W_0*(xx-xp*D), H_0*(yy-yp*D))`. Destination divides that origin by `C_l`; source begins at `(xi*W_l/T,yi*H_l/T)`. No extra geometric scaling is applied to the decoded pixels.

At stored level zero skip zero-coordinate cameras except camera `(0,0)` and collect accepted cameras into a bitset. Later levels require at least one active camera in their Q-by-Q block, clipped to camera bounds. Build this bitset even if the first read requests a low-resolution level. Retain the documented limitation for `C_0>D`, where merged images can include inactive cameras with garbage coordinates; do not promise a corrected vendor stitch.

Index actual double-precision destination rectangles, using a compact packed spatial tree or offset-aware bins with bounded duplication. Query all intersecting rectangles, then sort descending grid row and column; reject ambiguous duplicate grid positions in the supported primary profile. Do not use file order. Index conservatively enough to cover sampler support and ceiling-sized intermediate surfaces, then apply the specified nominal intersection predicate so query optimization cannot change output.

## 6. Region rendering and public API

`MRXSScene` implements `readResampledBlockChannelsEx` and `readResampledLevelBlockChannelsEx`. Normal reads receive a stored-level-zero rectangle and an output size. Select the level the way every other pyramid driver does: compute `zoomX = outputWidth/inputWidth`, `zoomY = outputHeight/inputHeight`, and call `Tools::findZoomLevel(std::max(zoomX, zoomY), …)` with each level's zoom `1/d_l`. That picks the coarsest level not coarser than the request on either axis, falls back to level zero for upsampling, and snaps to a level within 1% of the requested zoom — a request a hair off a level's scale must read that level in MRXS exactly as it does in SVS or PKE. Exact-level reads use their explicit level coordinates. Convert normal read coordinates by `d_l`, never `C_l`.

The two entry points must agree: a `readResampledLevelBlockChannelsEx` read of level `l` and the `readResampledBlockChannelsEx` read of the same region scaled by `d_l` produce the same pixels, bit for bit. That is the project-wide rule for level-addressed reads (CLAUDE.md, "Level-addressed reading"). Get it by construction, the way `SVSTiledScene` does: the level read is the only rendering path, and the scaled read selects a level, converts its rectangle with `Tools::scaleRect(blockRect, 1/d_l, 1/d_l, levelRect)` and calls the level read. The equality is still an acceptance test (§9), because a later optimisation of either path could break it.

The level read renders exactly the integer level rectangle it was given — no halo, no fractional request origin — into a level-resolution RGB buffer, then resizes that buffer once to the output size with `Tools::resize` and its default `INTER_LINEAR`, the same call `TileComposer` uses, and finally extracts the requested channels in caller order. Because each output is a function of its integer level rectangle alone, adjacent requests at the same level agree on their shared pixels; seams can still appear between independently resampled requests, exactly as with every other driver.

Bound the working memory. Compositing needs a premultiplied RGBA accumulator in `float` — the saturating operator works on fractional alpha, which 8 bits cannot carry through dozens of overlapping fractional edges — at 16 bytes per pixel. Since the operator is per pixel, the level rectangle is rendered in bands of a fixed pixel budget (a named policy constant, e.g. 4 Mpixel): each band composites its intersecting drawables in the required order, flattens onto the background, and writes 8-bit RGB into the level buffer. Banding changes no output pixel, only peak memory, and the test suite checks that by rendering one region with two band sizes.

Render only intersecting tiles into a request-local premultiplied RGBA surface. Decode complete encoded payloads and normalize them to RGB once:

- **JPEG** uses the existing `ImageTools::decodeJpegStream`.
- **PNG and BMP24** use FreeImage, which `slideio-imagetools` already links (the conan profiles build it with PNG, JPEG and BMP support; only `with_raw` is off). FreeImage is called today only with a file path, so imagetools gains one in-memory entry point, `ImageTools::decodeFreeImageStream(const uint8_t* data, size_t size, cv::OutputArray output)`, built on `FreeImage_OpenMemory` and `FreeImage_LoadFromMemory`. OpenCV cannot serve here: the profiles build it with `imgcodecs=False`, so `cv::imdecode` does not exist. Reading from a memory buffer of known length makes the input bounded by construction.
- **Dimensions are validated before pixels are allocated.** Load once with `FIF_LOAD_NOPIXELS` (supported for JPEG, PNG and BMP in FreeImage 3.18), check width, height and bit depth against the descriptor's expected size and the decoded-bytes budget, then decode for real. For JPEG, read the header the same way before calling `decodeJpegStream`.
- **The actual codec must match `IMAGE_FORMAT`.** Check the stream signature (`FreeImage_GetFileTypeFromMemory`, or the JPEG SOI marker) against the declared format and fail on a mismatch; the format document records map layers that declare PNG but contain grayscale BMPs.
- **BMP24 stays strict.** FreeImage accepts far more than the format document's BMP24 profile (palettes, 8-bit, RLE, top-down rows). Before decoding, parse the 54-byte header and require the `BM` signature, a 40-byte DIB header, one plane, 24 bits per pixel, `BI_RGB` compression and a positive (bottom-up) height; the pixel offset is relative to the embedded stream. A grayscale or palette BMP selected by mistake must fail, not decode quietly.
- **PNG with alpha is rejected** (`FreeImage_GetColorType()` returning `FIC_RGBALPHA`, or `FreeImage_IsTransparent()`), as are sample depths other than 8 bits.
- FreeImage delivers BGR(A) in its own row order; the adapter converts to top-down RGB once, so the renderer never sees codec-specific layout.

Use a driver-local fractional sampler and compositor; current `Tiler::getTileRect` uses integer rectangles and cannot express this geometry. Do not alter generic TileComposer for MRXS. The renderer must reproduce the source's transparent `ceil(tileWidth) x ceil(tileHeight)` intermediate (created when the decoded image is larger than the drawable tile on either axis, per the format document's section 11.2), fractional source translation, descending tile order, and saturating alpha operator. For premultiplied colors, `Fs=min(1,(1-Ad)/As)` when As is positive, then `Cout=Cs*Fs+Cd`, `Aout=min(As,1-Ad)+Ad`. Handle As=0 without division.

A small renderer prototype must establish sampling support, edge rules and numerical tolerance before production implementation. Cairo may be used in an optional test harness to generate reference images; the proposed shipping implementation uses existing OpenCV facilities plus explicit alpha arithmetic and adds no Cairo dependency. Do not claim pixel identity until measured; fractional geometry correctness and codec-tolerant comparisons are separate acceptance criteria.

Flatten remaining transparency onto the scene's background only after compositing; filling the accumulator opaquely first would prevent SATURATE painting. The background colour is not a renderer constant: it is what `getChannelBackground` returns (§7), which MRXS derives from `IMAGE_FILL_COLOR_BGR` (low byte red, then green, then blue). Requests that extend beyond the nominal nonnegative canvas go through `CVScene::readClampedBlock` like every other driver's, and that fills the uncovered part through the same hook — so a hole inside the slide and the area past its edge get the same colour from one source. When `IMAGE_FILL_COLOR_BGR` is absent, the hook keeps the default of 255 per channel. Document the difference from OpenSlide's transparent ARGB API.

Expose `getRect()` as `(0,0,nominalWidth0,nominalHeight0)`. `LevelInfo` uses stored-level size, scale `1/d_l`, and level-adjusted magnification. Its regular integer tile grid is a logical read grid, not the irregular physical drawable layout; use a fixed positive logical tile size such as 256 square and document this distinction. MPP is level-zero metadata converted from micrometers to meters, independently in X and Y. Do not multiply it by C0 again. `getCompression()` reports the pyramid codec from the per-level `IMAGE_FORMAT` values: `Compression::Jpeg`, `Compression::Png` or `Compression::BMP` when every stored level agrees, and `Compression::Unknown` when they differ, rather than a guess. Each level's own codec goes in the metadata tree.

`getRawMetadata()` returns the original `Slidedat.ini` text unmodified, with `MetadataFormat::Text` — no new enum value. `buildMetadataTree()` exposes the parsed INI groups plus the driver's own findings: rendering profile, nominal and populated bounds, unsupported-layer notices, and the objective magnification when known. Associated images use the names the other drivers already use: `SlideThumbnail` becomes `"Macro"`, `SlideBarcode` becomes `"Label"`, and `SlidePreview` becomes `"Thumbnail"`. Missing images are absent; present malformed known-image metadata fails validation, while corrupt JPEG pixels fail on access with contextual errors. Every `MRXSAuxScene` calls `markAsAuxImage()` when created; otherwise `canReopenByPath()` reports it reopenable by path, and the converter and transformer rewrap paths would reopen the main scene in its place.

## 7. Core change: per-channel background value

Today the background value is hard-coded twice in `slideio-core`: `CVScene::initializeSceneBlock` fills with 255 for byte data and 0 otherwise, and `CVScene::readClampedBlock` repeats the rule inline in its own `setTo` call instead of reusing it. No driver can state what its background is. Add one virtual hook to `CVScene`:

```cpp
/**@brief value an uncovered pixel of channel @p channel takes: outside the scene,
 * outside a level, or wherever the driver has no data for it. */
virtual double getChannelBackground(int channel) const;  // 255 for DT_Byte, 0 otherwise
```

- **Per channel, not `cv::Scalar`.** A `Scalar` holds at most four values, and CZI and ZVI scenes have many more channels. Asking per channel also handles the caller's channel order: plane `k` of a block is filled with `getChannelBackground(channelIndices[k])`.
- **`initializeSceneBlock` uses the hook.** When all requested channels share one value it keeps the single `setTo` — the path every existing driver takes; otherwise it fills plane by plane.
- **`readClampedBlock` calls the same fill helper** instead of its inline `setTo`, so the two cannot drift apart again.
- **The default reproduces today's values exactly**, so no existing driver changes behaviour. `cvscene.hpp` is an internal header, so this is not an exported-API break and needs no `BREAKING_CHANGES.md` entry.
- **MRXS overrides it** to return the red, green and blue bytes of `IMAGE_FILL_COLOR_BGR`, and its renderer flattens onto the values the hook returns rather than a second constant.
- **`TransformerScene` does not forward it.** A transformation may change what a channel means — after a grayscale or edge filter the origin's background RGB is meaningless — so a transformer keeps the default unless a later change makes it forward channels one-to-one.

This lands as its own commit ahead of the driver, with a unit test that a scene overriding the hook gets its value in both paths of `readClampedBlock`: the request entirely outside the scene and the partially overlapping one.

## 8. Performance, concurrency, diagnostics

Share immutable indices; use request-local decoding/render buffers. Cache decoded encoded images by dataset identity, level, file, offset, length and decoder normalization profile, never by subtile. A byte-bounded LRU allows all subtiles and neighboring requests to reuse one decode. Do not retain a lock during I/O or decode; optionally deduplicate in-flight requests. A handle cache has its own count bound and returns shared ownership so eviction cannot invalidate active reads.

Keep `supportsConcurrentReads()` false initially. Enable only after concurrent-read, cache-eviction and lifetime tests show all mutable state is synchronized or request-local. Prefer FileReader over ContextPool for positional input; codec state requiring reuse can use ContextPool after a concrete need is demonstrated.

Errors include entry path, layer/level, index page offset, descriptor identity and selected data-file range when available. Distinguish corrupt data, unsupported profile and resource-limit failures. `slideio-core` has a single exception type, `slideio::RuntimeError`, and adding types would change a public header; so the category is carried in the message, as a fixed leading tag — `MRXS corrupt:`, `MRXS unsupported:`, `MRXS limit:` — which tests can match on. The driver throws `slideio::RuntimeError` directly through the `MRXS_CORRUPT`/`MRXS_UNSUPPORTED`/`MRXS_LIMIT` macros of `mrxserrors.hpp`, not through `RAISE_RUNTIME_ERROR`, which would put the source file and line in front of the tag. Do not catch decode errors and substitute background. Metadata notices should distinguish deliberate primary-RGB rendering from vendor mask/hardware interpretation.

## 9. Validation and release gates

Tests live in a new `slideio_mrxs_tests` executable under `src/tests/mrxs/`, alongside the other per-driver suites. Synthetic fixtures are mandatory and small — about 230 KB for all eighteen — and, unlike every other test image, they are committed to the repository under `src/tests/mrxs/data/`. That is a deliberate exception: the parser, geometry and malformed-input tests then need no corpus, so they can join the corpus-free filter CI already runs, and the parser is the part of the driver most exposed to hostile input. (Every other test image resolves through `SLIDEIO_IMAGES_PATH`; `SLIDEIO_TEST_DATA_PATH` is not what `TestTools::getTestImagePath` reads.) They are produced by a generator committed to this repository, `auxfiles/mrxs-fixtures/`, written in Python and sharing no code with the driver, so a geometry mistake in the driver cannot also be baked into its fixtures. The generator writes INI/index/data bundles with tiny independently coloured images, split pages, sparse camera activity, negative origins and fractional geometry, together with a JSON file of the expected dimensions, drawable rectangles and a few probe-pixel colours computed from the format document's formulas. The generator, not the stored bundles, is the source of truth: regenerating must reproduce them byte for byte. Validate parser and renderer separately so a shared incorrect geometry calculation cannot serve as its own oracle.

Real bundles live under `SLIDEIO_IMAGES_PATH/mirax/`, outside Git. The integration set is nine bundles, about 5.4 GB, chosen so each covers something no smaller one does; the format document's section 17 holds the measured profile of each. Paths are relative to `SLIDEIO_IMAGES_PATH`:

| Bundle (entry file) | Profile | What it is the test for |
|---|---|---|
| `mirax/openslide/Mirax2-Fluorescence-1/Mirax2-Fluorescence-1.mrxs` (63 MB) | D=4, C0=1, raw map, JPEG, fluorescence | The ordinary pyramid: transition from one image per tile to camera subtiles; fluorescence exposed as rendered RGB, not as spectral channels |
| `mirax/openslide/Mirax2-Fluorescence-2/Mirax2-Fluorescence-2.mrxs` (42 MB) | as above | Second instance of the ordinary profile, guarding against fitting one file |
| `mirax/openslide/CMU-1-Saved-1_2/CMU-1-Saved-1_2.mrxs` (163 MB) | D=4, C0=2, raw map | Saved pyramid with `e_0>0`, several images per camera position, map normalization by `C_0` |
| `mirax/openslide/CMU-1-Saved-1_16/CMU-1-Saved-1_16.mrxs` (5.5 MB) | D=4, C0=16, raw map | Several camera positions per image (`C_0>D`), including the documented inactive-camera limitation |
| `mirax/openslide/Mirax2.2-4-PNG/Mirax2.2-4-PNG.mrxs` (1.0 GB) | D=4, C0=1, zlib stitching map, PNG | The only real PNG pyramid; compressed position map |
| `mirax/openslide/Mirax2.2-4-BMP/Mirax2.2-4-BMP.mrxs` (1.8 GB) | as above, BMP24 | The only real BMP24 pyramid: the strict header check accepts it and the FreeImage path decodes it. It has the same 7,190-image layout as the PNG twin but different position maps (OpenSlide's bounds differ by a few pixels), so the two are not pixel-identical and no cross-codec equality test is possible |
| `mirax/zenodo/20240920152150628/20240920152150628.mrxs` (56 MB) | D=1, no standard map, zero overlap; parallel masks, hardware family | No-map nominal positions; mask and hardware layers retained but ignored; empty heads; a 10,592-record page against `PAGEELEMENTCOUNT=128`; map auxiliary rasters declared PNG but stored as BMP |
| `mirax/zenodo/PDX_H5097_ID_2053_LG_HE/PDX_H5097_ID_2053_LG_HE.mrxs` (1.4 GB) | D=8, fractional overlap ≈180.69, zlib stitching map | Fractional geometry on a real slide; active-camera filtering above `C_l=D` (level 8: 21 images, 866 drawables); hierarchical scan-info XML page kept out of the image path |
| `mirax/zenodo/2401582-38_mihc/2401582-38.mrxs` (865 MB) | Five-channel fluorescence, two JPEG groups around a mask layer | Explicit unsupported-profile rejection until logical channels are implemented (§11); tagged non-hierarchical entries |

Expected values for every bundle — level count, nominal dimensions per level, downsamples, populated bounds, associated-image names and sizes — come from the format document's section 17 and OpenSlide 4.0.1, and are asserted exactly. The CMU and Mirax2 bundles are from the OpenSlide test-data collection (CC0-1.0 or "distributable", per its `index.yaml`); each Zenodo bundle carries its attribution in its own readme. Record each bundle's checksum next to its test.

Deliberately excluded: CMU-1/2/3 and Mirax2.2-1/3 repeat covered profiles at larger size; CMU-1-Exported's no-map profile is covered by the Zenodo slide, and its one distinct trait, no associated images, is cheaper as a synthetic fixture; the Saffron HE/HES pair adds only tagged entries (covered by mIHC) and negative-origin requests (covered synthetically). The TCIA Hungarian colorectal-screening bundles are not in the test set. Bundle `003` (1,347,718 primary descriptors; a 1,009,334-record page) is the only real million-descriptor sample and is the target of the performance measurements below, which are run by hand from the archive rather than by the test suite. It is de-identified human screening material under TCIA's data-use and privacy conditions: performance runs must not write, log or compare its label image.

No real sample covers negative camera origins, a nonzero non-hierarchical next-page pointer, or a missing map with nonzero overlap; the synthetic fixtures must. A test whose bundle is absent uses the existing `SLIDEIO_SKIP_MISSING_IMAGES` mechanism — skipping with the file it wanted when the variable is set, failing when it is not, as CI requires — rather than a driver-specific skip list. Every bundle the tests name appears in `software-docs/TEST_IMAGES.md`, regenerated with `auxfiles/list-test-images.py`.

OpenSlide comparisons do not run OpenSlide inside the test suite. A reference harness committed as `auxfiles/mrxs-openslide-reference.py` (openslide-python against OpenSlide 4.0.1, run by hand) reads selected same-origin regions at every stored level of each integration bundle, flattens OpenSlide's ARGB onto the bundle's `IMAGE_FILL_COLOR_BGR`, and writes them as lossless PNGs plus a JSON manifest next to the bundles in the test data. The gtest suite compares against those stored references, so CI needs neither OpenSlide nor Python. The regions include camera seams, tiny fractional subtiles, sparse holes, negative placement, slide edges and nonzero C0. The manifest records the OpenSlide and codec versions used. Parser geometry and dimensions must match exactly; pixel tolerances are chosen from the renderer prototype and written down before the comparison tests are enabled, not loosened afterwards to hide failures.

**Measured and fixed on 2026-11-07.** `auxfiles/mrxs-renderer-prototype.py` implements this section's renderer in Python (float premultiplied accumulator, `ceil` intermediate, bilinear fractional translation with transparent outside, descending order, saturating operator, background last) and was compared with the 168 OpenSlide 4.0.1 reference regions of the nine integration bundles:

| Where | Max abs difference per channel | Worst region mean | Worst share of pixels off by more than 2 |
|---|---:|---:|---:|
| Stored level 0, every bundle | 0 | 0 | 0 |
| Zero-overlap bundles (Zenodo, mIHC), every level | 0 | 0 | 0 |
| Fractional levels of overlapping bundles | 4 | 0.35 | 0.11 % |

The residual is Cairo's 8-bit premultiplied arithmetic against the float accumulator; positions are right. The C++ comparison tests therefore use, per region and channel: **exact** where the prototype is exact (stored level 0, and every level of a zero-overlap bundle), widened only by what the C++ JPEG decoder is measured to differ from OpenSlide's on the same payloads; and elsewhere **max difference ≤ 6, mean ≤ 0.5, at most 0.5 % of pixels off by more than 2**. The margin over the measured 4 / 0.35 / 0.11 % is for codec differences, not for geometry. If the C++ decoder alone accounts for more than that margin, that is a finding to report, not a reason to widen the tolerance.

Release requires bounded malformed-input behavior, public explicit/AUTO opening (including a corrupt `.mrxs` under AUTO reporting an MRXS error, not a GDAL one), main/auxiliary lifetime correctness, exact-level and resampled reads agreeing bit for bit with each other (§6), uncovered pixels inside and outside the canvas both taking the `IMAGE_FILL_COLOR_BGR` background (§7), channel selection, converter compatibility, package linking and cross-platform CI. Track open time, index memory, cold/warm ROI latency and decode count on the TCIA bundle `003` described above; cache usage must remain under its configured budget. Exact default performance budgets are set from measured baselines at the geometry milestone.

## 10. Repository integration checklist

A new driver has to be registered in several places, and most omissions fail silently — the build stays green and the driver is simply missing somewhere. Each item below is part of the first release:

| Where | What | What breaks if missed |
|---|---|---|
| `src/slideio/drivers/mrxs/` | Driver sources, `CMakeLists.txt`, `mrxs_api_def.hpp` (`SLIDEIO_MRXS_EXPORTS`), and its own `conanfile.py` requiring zlib for compressed position maps | `install.py -a conan` finds driver conanfiles by `rglob`, so a missing one means zlib is not on the driver's prefix path |
| `src/slideio/drivers/CMakeLists.txt` | `add_subdirectory(mrxs)` | Driver not built |
| `src/slideio/slideio/CMakeLists.txt` | Link `slideio-mrxs` into the main library | Unresolved driver symbols |
| `src/slideio/slideio/imagedrivermanager.cpp` | Construct the driver in `initialize()`; add `"MRXS"` to `driverOrder` before `"GDAL"` | Without the `driverOrder` entry, explicit opening works and AUTO never finds the driver |
| Root `CMakeLists.txt` | Driver-name variable, both `NAME_TOOL_LIB_LIST` entries, the install list | On macOS the rpath rewrite silently skips a dylib not named in `NAME_TOOL_LIB_LIST`; a library missing from the install list is missing from every package |
| `auxfiles/package-smoke/main.cpp` | Add `"MRXS"` to the expected-driver list | The release smoke test would not notice the driver missing from an archive |
| `src/tests/mrxs/`, `src/tests/CMakeLists.txt` | `slideio_mrxs_tests` | — |
| `auxfiles/list-test-images.py` | Map `mrxs` to `slideio_mrxs_tests` | `TEST_IMAGES.md` omits the MRXS images |
| `CLAUDE.md` | Format and driver-library counts (13 formats, 12 libraries), source layout, test list, concurrency list once enabled | Stale project guidance |
| `auxfiles/mrxs-fixtures/`, `auxfiles/mrxs-openslide-reference.py` | Synthetic-fixture generator and OpenSlide reference harness (§9) | Fixtures and references cannot be regenerated or reviewed |
| `slideio-python` repository | Formats table in `README.md`; the wheel must carry `slideio-mrxs` (compare against `docs/superpowers/plans/baseline-wheel-manifest.txt`) | Python users get a wheel without the driver |
| `slideio-doc` repository | Supported-formats page and release news | Users cannot discover the format |

## 11. Follow-on variants

Add logical channels only with explicit group identity and tested slot mapping. For the documented five-channel sample, join by spatial counter, use metadata group references and storing-channel numbers, and keep masks separate; never infer groups from page order or skip arbitrary candidates heuristically. Validate RGB component selection independently from display colors and USE_* flags. Hardware maps need a dedicated coarse-grid model and a declared canvas policy. Compact FF/00 masks require verified polarity and vendor compositing evidence. JPEGXR/JPEG2000 and high depth need real samples and decoder-specific validation. Each capability gets its own fixtures, advertised scope and failure behavior.
