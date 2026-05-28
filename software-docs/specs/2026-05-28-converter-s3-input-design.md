# Converter S3 Input — Design

**Status:** Draft, ready for implementation
**Branch:** `s3`
**Scope:** Make the converter CLI and library accept `s3://`, `http://`, and
`https://` URIs as inputs, using the HTTP/S3 streaming foundation shipped in
v1 of the S3 streaming work (see
[`2026-05-25-s3-streaming-design.md`](./2026-05-25-s3-streaming-design.md)).

## 1. Goal

Allow `slideio_converter` (the CLI in `src/tools/converter/`) and
`slideio::converter::convertScene(...)` (the library entry point) to convert
an input slide that lives at an HTTPS URL — typically a presigned AWS S3 URL
— instead of a local filesystem path. Bytes are streamed on demand via the
existing `HttpStream`; no full-file download is performed.

The user-visible CLI command stays the same:

```
slideio_converter --input <URI> --output <local-path> ...
```

`<URI>` may now be any of:

- `s3://bucket/key`
- `https://...` or `http://...` (must be presigned or public)
- any existing local path form (unchanged behavior)

## 2. Non-goals

- **Output to S3.** Inherited from the S3 streaming design (§1 non-goal).
  Output remains a local file path. The `<local-path>` argument is unchanged.
- **AWS SDK, credentials, signing.** Presigned URLs only — inherited.
- **Migrating v2/v3 drivers (czi, vsi, gdal, zvi, dcm).** Those gain S3 input
  automatically when the broader S3 phasing migrates them. This work adds
  nothing driver-specific. If a user points the converter at an S3 URI for an
  un-migrated driver, the existing dispatcher message ("driver does not yet
  support remote URIs") surfaces unchanged.
- **Stream sharing across reader threads.** Each reader thread continues to
  open its own slide handle, and therefore its own `HttpStream` and 256 MB
  block cache. A future refactor could share one stream across readers (see
  §6); explicitly deferred here.

## 3. Current state

After the S3 v1 work, the converter is **already implicitly capable** of S3
input through the library API — but two seams block the path end-to-end:

1. **CLI gate.** `src/tools/converter/sceneconverter.cpp:213` rejects any
   input that fails `std::filesystem::exists`, which is false for every
   `s3://` and `http(s)://` URI.

2. **MT reader reopen.** `TiffConverter::cloneScene()` in
   `src/slideio/converter/tiffconverter.cpp:530` reopens the slide once per
   reader thread:

   ```cpp
   std::string filePath = m_scene->getFilePath();
   int sceneIndex = m_scene->getSceneIndex();
   std::string driverId = m_scene->getDriverId();
   std::shared_ptr<Slide> slide = openSlide(filePath, driverId);
   ```

   For this to work over HTTP, `getFilePath()` must return the original URI
   passed to `openSlide` (so it can be re-dispatched through `createStream`).

   Per grep of v1 stream-capable drivers (`src/slideio/drivers/{svs,scn,pke,
   ome-tiff,ndpi,afi}/`), every one assigns `m_filePath = stream->uri()` (or
   the equivalent `identifier` variable that holds it) for stream opens, and
   that string is propagated to the `CVScene` on construction. So MT reopen
   already works end-to-end; this design captures the invariant and pins it
   with an integration test rather than introducing new code in the reopen
   path.

## 4. Architecture

```
                                 (LOCAL path or URI)
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────┐
│  CLI: src/tools/converter/sceneconverter.cpp             │
│       URI-aware existence gate (this design)             │
│       openSlide(inputPath, inputDriver)                  │
└──────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────┐
│  ImageDriverManager::openSlide  (already routes URIs)    │
│       detectUriScheme + createStream + driver dispatch   │
└──────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────┐
│  Driver (SVS / NDPI / OME-TIFF / …)                      │
│       openFile(stream) overload, sets                    │
│       slide->m_filePath = stream->uri()                  │
│       scene->m_filePath = stream->uri()                  │
└──────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────┐
│  TiffConverter::convertScene / createTiff                │
│       MT reader path calls openSlide(getFilePath(), …)   │
│       → fresh HttpStream + cache per reader thread       │
└──────────────────────────────────────────────────────────┘
```

The new code lives only in the top box (CLI gate) and the integration test.
Boxes 2–4 are unchanged.

## 5. CLI input-gate change

### 5.1 What changes

`src/tools/converter/sceneconverter.cpp:213`, current:

```cpp
if (!std::filesystem::exists(inputPath)) {
    throw std::runtime_error("Input file does not exist: " + inputPath);
}
```

New:

```cpp
if (detectUriScheme(inputPath) == UriScheme::LocalFile) {
    if (!std::filesystem::exists(inputPath)) {
        throw std::runtime_error("Input file does not exist: " + inputPath);
    }
}
// Remote URIs (s3 / http / https): existence is validated by openSlide
// (HEAD probe / driver-support gate), and any failure surfaces with a
// clear network or driver-level error.
```

`detectUriScheme` and `UriScheme` are already exported from
`slideio/imagetools/uridispatcher.hpp` — the same routine
`ImageDriverManager::openSlide` uses. We reuse it to keep one source of
truth for "is this a local path or a remote URI?".

### 5.2 What does not change

- `<output-path>` is still validated as a local path (existence, optional
  delete, etc.) — output to S3 is out of scope.
- The `--input-driver` flag, `--scene`, ranges, encoding, tile size, thread
  counts: all unchanged.
- The progress bar, `--info-only`, `--silent`: unchanged.
- Behavior on local input is byte-identical to before.

### 5.3 Help / error text

No new flags. Help text is updated only to mention that `--input` accepts
`s3://`, `http://`, or `https://` URIs in addition to a local path.

## 6. Memory and throughput notes (no code change)

The MT reader path opens `numReadingThreads` independent slides, each with
its own `HttpStream` and 256 MB block cache. For HTTP input this means:

- **Peak memory ≈ numReadingThreads × 256 MB** of stream cache (plus the
  encoder/writer pipelines).
- **N HEAD probes** at conversion start (one per reader open).
- Tiles touched by multiple readers can be fetched **N times**, since caches
  are independent.

This is the same model the v1 streaming work shipped with. Users with bounded
memory can already mitigate via:

- `--num-reading-threads <N>` — lower N, lower peak cache memory.
- `ImageDriverManager::setHttpCacheEnabled(false)` (library) — disables the
  per-stream cache; every read triggers a fresh ranged GET. Trades cache
  memory for redundant GETs. Useful for very large slides over fast links.

A future refactor could share a single `HttpStream` across all reader-thread
slides (each reader opens its own libtiff `TIFF*` over the same stream).
That requires plumbing the shared stream into each driver's
`cloneScene`-equivalent reopen path. Deferred — out of scope here.

The CLI's `printInfo` output gains one line when the input is a remote URI,
documenting the per-reader cache footprint so the user can tune
`--num-reading-threads`. (Pure UX; no behavior change.)

## 7. Testing strategy

### 7.1 Integration test (new)

`src/tests/converter/test_converter_s3.cpp`, one file, two test cases:

1. **`Converter_S3.SingleThreaded_HttpInput_MatchesLocal`**
   - Start the in-process Python HTTP fixture used by
     `src/tests/main/test_s3_streaming_integration.cpp` (range-aware
     `http.server` on `127.0.0.1`).
   - Serve a small SVS test slide already present in the repo's test-data
     set (the same one used for the streaming integration test).
   - Call `convertFile(http://127.0.0.1:<port>/test.svs, <tmp>/out_http.svs,
     ..., numReadingThreads=1, numEncodingThreads=1)`.
   - Call `convertFile(<local-path-to-same-svs>, <tmp>/out_local.svs, ...,
     same parameters)`.
   - Compare the two outputs: open both with `openSlide`, verify same
     dimensions, tile counts, magnification, and a sampled tile read matches
     byte-for-byte. (Full byte-equal comparison of the SVS itself is fragile
     because the SVS image-description tag embeds a UUID and timestamp.)

2. **`Converter_S3.MultiThreaded_HttpInput_MatchesLocal`**
   - Same as (1) but with `numReadingThreads=4, numEncodingThreads=4`.
   - Exercises the `cloneScene()` reopen path over HTTP — each reader thread
     opens its own `HttpStream`.

Both tests use the existing test-fixture helper, set the
`SLIDEIO_TEST_PYTHON` env var if required by the fixture, and clean up the
temp output directory.

### 7.2 Not adding

- Unit test for the input-gate function: trivial enough that the integration
  test covers it (any URI scheme path through is exercised; nonexistent
  local path is already covered by existing converter tests).
- Live AWS S3 test: covered by the v1 design (§10.4) and not specific to the
  converter.
- Tests for v2/v3 driver inputs: those drivers don't yet stream; the existing
  dispatcher message is the contract until they're migrated.

### 7.3 CMake

Add `test_converter_s3.cpp` to `src/tests/converter/CMakeLists.txt` alongside
the existing `test_converter.cpp`. The HTTP fixture dependency
(`SLIDEIO_TEST_PYTHON`) is already wired into `src/tests/main/CMakeLists.txt`
for the streaming integration test; the converter test suite will need the
same wiring — copy that pattern.

## 8. Error handling

| Case | Behavior | Source |
|---|---|---|
| Local file missing | `"Input file does not exist: <path>"` thrown by CLI pre-flight | `sceneconverter.cpp` (unchanged) |
| Output file already exists, `--delete-if-exists` not set | `"Output file already exists: <path>"` | `sceneconverter.cpp` (unchanged) |
| Remote 404 | HttpStream surfaces `"HEAD failed: 404"` (or equivalent) | `HttpStream::probeSize` (existing) |
| Remote 403 (expired presigned URL) | HttpStream surfaces `"HEAD failed: 403"` | `HttpStream::probeSize` (existing) |
| Driver does not support streams (czi/vsi/zvi/dcm/gdal/qptiff) | `"Driver <X> does not yet support remote URIs: <uri>"` | `ImageDriverManager::openSlide` (existing) |
| Driver detection fails for URI | `"Cannot find driver for file <uri>. Try to define driver manually."` | `ImageDriverManager::openSlide` (existing) |

All error messages are pre-existing — this work doesn't add new error paths.

## 9. Backwards compatibility

- CLI flags and behavior are unchanged for local-path inputs.
- Library API (`converter::convertScene`) is unchanged at source and ABI
  level.
- No new public symbols, no header changes.
- Existing converter test suites continue to pass without modification.

## 10. File touchpoints

| File | Change | Approx LOC |
|---|---|---|
| `src/tools/converter/sceneconverter.cpp` | URI-aware existence gate; help-text update; optional info-line for remote inputs | ~15 |
| `src/tests/converter/test_converter_s3.cpp` | New file, two tests | ~120 |
| `src/tests/converter/CMakeLists.txt` | Add new test source; wire `SLIDEIO_TEST_PYTHON` if needed | ~5 |

**No library/header changes.** No driver changes. No public-API changes.

The converter CLI already links `${IMAGETOOLS_LIB_NAME}` and has
`${INCLUDE_ROOT}` on its include path (see
`src/tools/converter/CMakeLists.txt`), so `slideio/imagetools/uridispatcher.hpp`
is reachable without a CMake change.

## 11. Implementation order

1. Audit `cloneScene()` reopen on a stream-opened SVS — add an
   integration-test smoke first (single-threaded) to confirm the
   no-code-change assumption before touching anything.
2. CLI input-gate change in `sceneconverter.cpp`.
3. Optional info-line for remote inputs.
4. Multi-threaded integration test.
5. Update `software-docs/specs/2026-05-25-s3-streaming-design.md` if the
   converter integration surfaces any deviations from §6 of that spec.

Each step is independently runnable and verifiable.
