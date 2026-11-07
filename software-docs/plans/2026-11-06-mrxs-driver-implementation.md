# MRXS driver implementation plan

Date: 2026-11-06, revised 2026-11-07 to match the reviewed design. Status: proposed.

Design: [MRXS reader driver](../specs/2026-11-06-mrxs-driver-design.md). Section numbers below (§n) refer to it.
Format reference: [MIRAX/MRXS description](../../../documents/image_formats/mirax/MIRAX_MRXS_Format.md). It lives outside the repository for now and will move under `software-docs/`; update both links when it does.

This plan delivers the primary RGB profile first. Estimates are intentionally omitted until the fractional renderer prototype and the large-index measurements resolve the two main uncertainties. Each step is a reviewable change with its own tests. Do not advertise MRXS in a release until every release gate in §9 passes.

Steps cite code by greppable names, not line numbers, because earlier steps move lines.

## 1. Core: per-channel background hook (§7)

Independent of everything else; lands first as its own commit.

- Add `virtual double getChannelBackground(int channel) const` to `CVScene`, defaulting to 255 for `DT_Byte` and 0 otherwise.
- Factor one fill helper that fills plane `k` with `getChannelBackground(channelIndices[k])`, keeping the single `setTo` when all values are equal. Use it from `initializeSceneBlock` and replace the inline `block.setTo(...)` in `readClampedBlock`.
- Do not forward the hook from `TransformerScene`.
- Tests, next to the existing clamped-read tests in `src/tests/main/test_scenesource.cpp`: a test scene overriding the hook gets its value both for a request entirely outside the scene and for a partially overlapping one; a scene that does not override it gets exactly today's values; channel order follows the caller's `channelIndices`.

Exit: full existing suites pass unchanged; the new tests pass.

## 2. Imagetools: in-memory FreeImage decoding (§6)

Also independent; may proceed in parallel with steps 3–5.

- Add `ImageTools::decodeFreeImageStream(const uint8_t* data, size_t size, cv::OutputArray output)` on `FreeImage_OpenMemory` / `FreeImage_LoadFromMemory`.
- Provide a header-only probe using `FIF_LOAD_NOPIXELS` that reports codec (`FreeImage_GetFileTypeFromMemory`), width, height, bit depth and colour type without allocating pixels.
- Convert FreeImage's BGR(A), bottom-up rows to top-down RGB once in the adapter.
- Tests in `src/tests/main/test_imagetools.cpp`: PNG and BMP round trips from memory buffers, truncated buffers fail, the probe allocates no pixels, a PNG with alpha reports its colour type.

Exit: the new entry point is covered on all CI platforms; no existing imagetools behaviour changes.

## 3. Fixtures, reference harness and renderer feasibility

- Write the synthetic-fixture generator `auxfiles/mrxs-fixtures/` in Python, sharing no code with the driver. It emits INI/index/data bundles and a JSON of expected dimensions, drawable rectangles and probe-pixel colours computed from the format document's formulas. Regeneration must be byte-identical with the generator's pinned Python environment. Commit the generated bundles under `src/tests/mrxs/data/` (design §9: the one in-repository exception, so the parser tests can run in CI's corpus-free filter).
- Cases: 32/36-byte IDs, nonstandard filenames, multiple pages, a page longer than `PAGEELEMENTCOUNT`, D=1/2/8, exponents `[2,1,2]`, sparse positions, fractional overlaps, negative camera origins, a nonzero non-hierarchical next-page pointer, a missing map with nonzero overlap, no associated images, mismatched `IMAGE_FORMAT` and payload codec, a strict-BMP24 violation, UTF-8 BOM and empty INI keys.
- Write the OpenSlide reference harness `auxfiles/mrxs-openslide-reference.py` (openslide-python, OpenSlide 4.0.1, run by hand). For each integration bundle it writes selected regions at every stored level, flattened onto `IMAGE_FILL_COLOR_BGR`, as lossless PNGs plus a JSON manifest with OpenSlide and codec versions. The same harness confirms GLib `GKeyFile`'s handling of repeated keys and repeated groups (§4) and records the result.
- Prototype the renderer: float premultiplied RGBA accumulator, fractional crop isolation in the `ceil` intermediate, descending row/column order, SATURATE operator, flattening onto the background, banded rendering. Compare against analytically known synthetic images and the OpenSlide references, including subtiles narrower than one pixel.
- Decide and write down the pixel tolerances and edge rules before any comparison test is enabled. If the sampler cannot meet them, revise §6 of the design before building the production path.

Exit: generator and harness committed; references produced for the nine integration bundles; tolerances recorded; GKeyFile duplicate rules confirmed.

## 4. Driver skeleton and build wiring (§10)

- Add `src/slideio/drivers/mrxs/` with `CMakeLists.txt`, `mrxs_api_def.hpp` (`SLIDEIO_MRXS_EXPORTS`), a `conanfile.py` requiring zlib, and driver, slide and scene skeletons. Export the parser, geometry and renderer classes too (§3), so the tests can reach them.
- Wire `add_subdirectory(mrxs)` in `src/slideio/drivers/CMakeLists.txt` and the link in `src/slideio/slideio/CMakeLists.txt`.
- In the root `CMakeLists.txt`: the driver-name variable next to `PKE_DRIVER_NAME`, both `NAME_TOOL_LIB_LIST` entries, and the install list.
- Add `src/tests/mrxs/` with the `slideio_mrxs_tests` executable and register it in `src/tests/CMakeLists.txt`; map it in `auxfiles/list-test-images.py`. Tests that use only `src/tests/mrxs/data/` get a suite-name prefix the CI corpus-free filter can select, and that filter is extended in `build-validation.yml` and `release.yml`.
- Implement entry resolution (§2): `.mrxs` with a same-stem sibling data directory, or `Slidedat.ini` whose parent is the data directory and no `.mrxs` required. Scene name is the data-directory stem.
- `canOpenFile` matches names only (`*.mrxs`, `Slidedat.ini`, case-insensitive) and does no I/O. Do not register the driver with `ImageDriverManager` yet; that is step 9.

Exit: the library builds on all CI configurations, including the macOS rpath rewrite; name-matching tests reject arbitrary INI files.

## 5. Metadata and bounded binary indexing (§4)

- Driver-local INI reader with `GKeyFile` semantics as confirmed in step 3; BOM and empty-key preprocessing; locale-independent numbers; preserved case and unknown entries; section indirection; path containment including symlink escapes.
- Checked integer helpers; explicit little-endian reads through `FileReader`; slide-ID header validation with pointers at `5+S` and `9+S`; hierarchical and non-hierarchical iterators with cycle detection and work budgets.
- Named resource budgets (INI bytes, page visits, records, decoded dimensions and bytes, position-map output, spatial-index bytes, handles, cache bytes).
- Error messages carry the category tags `MRXS corrupt:`, `MRXS unsupported:` and `MRXS limit:` (§8).
- Tests: truncated headers and pages, negative fields, overflow, missing data files, mismatched IDs, null and `(0,0)` heads, nonzero unknown tags, pages exceeding nominal capacity, unsupported versions, duplicate primary counters, `Nx % D != 0` or `Ny % D != 0`, malformed D, and a second image-bearing group (unsupported-profile error).

Exit: synthetic descriptors enumerate exactly; malformed input fails with bounded work and the right category tag. No payload decoding yet.

## 6. Positions and geometry engine (§5)

- Raw and zlib map readers with exact `9*Px*Py` length, flag validation and the raw/compressed/synthesized precedence; a corrupt selected map fails without fallback.
- Cumulative exponents, repeated-truncation canvas sizing, `C_0` normalization, active-camera bitset, position-aware versus fast path, fractional drawable generation.
- Spatial lookup over actual destination rectangles. Benchmark compact drawable arrays against lazy subtile generation on TCIA bundle `003` from the archive (§9) and choose; both must give identical query results. Set the memory and open-time targets from that measurement.
- Tests: D/C alignment rejection, signed origins, `C_0>1` and `C_0>D`, edge blocks, Q>1, a low-resolution first read, query equivalence to a brute-force scan, and every expected value from the generator's JSON.

Exit: geometry matches the generator's expectations and the nine integration bundles' dimensions, downsamples and populated bounds exactly.

## 7. Decoding and production renderer (§6)

- Payload decoder: JPEG through `ImageTools::decodeJpegStream`, PNG and BMP24 through `decodeFreeImageStream`. Before allocating pixels: probe the header, check codec against `IMAGE_FORMAT`, check dimensions against the descriptor and the decoded-bytes budget, apply the strict BMP24 header rules, reject PNG with alpha and non-8-bit depths.
- Renderer: banded float RGBA compositing with the step 3 sampling rules, flattening onto `getChannelBackground` values (MRXS override from `IMAGE_FILL_COLOR_BGR`, default 255 when absent).
- `readResampledLevelBlockChannelsEx` is the only rendering path: render exactly the integer level rectangle, resize once with `Tools::resize` (default `INTER_LINEAR`), extract channels in caller order. Out-of-canvas requests go through `readClampedBlock`.
- `readResampledBlockChannelsEx` selects the level with `Tools::findZoomLevel(std::max(zoomX, zoomY), …)` over level zooms `1/d_l`, converts with `Tools::scaleRect`, and calls the level read — the `SVSTiledScene` pattern.
- Tests: overlapping opaque colours, fractional alpha edges, background byte order, holes inside and area outside the canvas both getting the fill colour, two band sizes giving identical output, a level read equal bit for bit to the equivalent scaled read, 1%-off zooms snapping to the level, codec mismatch, wrong decoded size, truncated adjacent payloads, channel order.

Exit: synthetic regions pass; comparisons against the stored OpenSlide references meet the step 3 tolerances at every level of the integration bundles.

## 8. Scene metadata and associated images (§6)

- Scene properties: nominal rectangle, Red/Green/Blue channels, compression (`Jpeg`/`Png`/`BMP` when all levels agree, else `Unknown`), MPP converted to meters per axis, magnification, `LevelInfo` with stored size, scale `1/d_l` and a 256-square logical tile grid.
- `getRawMetadata()` returns the INI text with `MetadataFormat::Text`; `buildMetadataTree()` adds the parsed groups, rendering profile, nominal and populated bounds, per-level codec, unsupported-layer notices and objective when known.
- Lazy auxiliary scenes `"Macro"`, `"Label"`, `"Thumbnail"` from `SlideThumbnail`, `SlideBarcode`, `SlidePreview`; each calls `markAsAuxImage()`; all share the dataset by `shared_ptr`.
- Tests: a scene retained after slide destruction, auxiliary lifetime, `canReopenByPath()` false for auxiliary scenes, unknown auxiliary name, raw metadata unmodified, anisotropic resolution, `C_0>1` MPP, logical-tile edge reads, mixed-codec compression reported as `Unknown`.

Exit: scene metadata and auxiliary behaviour match §6 with no change to the public scene API.

## 9. Registration and end-to-end integration (§2, §10)

- In `imagedrivermanager.cpp`, construct the driver in `initialize()` and add `"MRXS"` to `driverOrder` before `"GDAL"`. Update the driver expectations in `src/tests/main/test_imagedrivermanager.cpp`.
- Add `"MRXS"` to the expected-driver list in `auxfiles/package-smoke/main.cpp`.
- Tests: explicit and AUTO opening of `.mrxs` and `Slidedat.ini`, case-insensitive matching, Unicode paths, extracted bundles, and a corrupt `.mrxs` under AUTO reporting an `MRXS corrupt:` error rather than a GDAL or "Cannot find driver" one.
- Converter smoke tests on an MRXS scene, including level reads.
- Update `CLAUDE.md`: format and library counts (13 formats, 12 libraries), source layout, test-suite list.

Exit: an installed-package consumer opens and reads a synthetic MRXS through the public API; existing driver-manager tests pass.

## 10. Caching and concurrency certification (§8)

- Decoded-image byte LRU keyed by dataset, level, file, offset, length and normalization profile; bounded handle reuse returning shared ownership; decode counters. No lock held across I/O or decode.
- Tests: one encoded image feeding many subtiles decodes once per request; eviction during active reads; simultaneous identical and disjoint requests; repeated errors; dataset destruction; cache isolation between slides.
- Enable `supportsConcurrentReads()` only after those pass and concurrent output equals serial output; otherwise ship serialised reads with the limitation documented. If enabled, add MRXS to the concurrency list in `CLAUDE.md`.

Exit: cache and handle budgets hold; warm reads show reuse; sanitizers clean where CI supports them.

## 11. Corpus qualification and release

- Run `slideio_mrxs_tests` over the nine integration bundles (§9) with `SLIDEIO_SKIP_MISSING_IMAGES` unset; regenerate `software-docs/TEST_IMAGES.md`. Record each bundle's checksum next to its tests.
- Confirm the Zenodo hardware/mask slide renders primary RGB only, the five-channel mIHC bundle is rejected with `MRXS unsupported:`, and no unsupported profile returns partial pixels.
- Run the performance measurements by hand on TCIA bundle `003` from the archive: open time, peak index memory, cold/warm ROI latency, decode counts, cache usage under budget. Never write, log or compare its label image.
- Cross-platform CI including packaging and the package smoke test.
- Outside this repository: slideio-python (formats table in `README.md`; check the wheel contains `slideio-mrxs` against `docs/superpowers/plans/baseline-wheel-manifest.txt`) and slideio-doc (supported formats page, release news).

Exit: every release gate in §9 is satisfied, known limitations are documented, and the evidence is attached to the PR.

## Dependencies and risk controls

Steps 1 and 2 are independent core changes and land first. Step 3 precedes production rendering. Steps 4–6 precede 7; 8 follows stable decoding; registration (9) follows working scene reads; caching (10) follows correct rendering, so it cannot hide decoding errors. Corpus qualification (11) is the final gate.

The highest risks are fractional sampling compatibility, subtile expansion at very low levels, memory of the blending buffer, and confusing primary RGB with spectral or vendor rendering. They are controlled respectively by the step 3 prototype with tolerances fixed in advance, the step 6 benchmark on a million-descriptor bundle, banded rendering with a test that band size changes nothing, and explicit rejection and metadata notices. Broader channels, masks and hardware stitching need separate designs and evidence; they are not completion criteria for this release.
