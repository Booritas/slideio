# S3 / HTTPS Streaming Support — Design

**Status:** v1 implemented 2026-05-27 on branch `s3` (see §14). Design covers
all 11 drivers across three releases; v1 (foundation + 6 TIFF-family drivers)
is complete.
**Branch:** `s3`
**Scope:** All 11 SlideIO drivers, phased across three releases (v1, v2, v3).

## 1. Goal

Allow `slideio::Slide` to be opened directly from an HTTPS URL — typically a
presigned AWS S3 URL — instead of a local filesystem path. Bytes are streamed
on demand via HTTPS ranged GET requests; no full-file download is performed.
Memory usage is bounded by a fixed in-memory block cache.

The public API surface remains `ImageDriverManager::openSlide(uri, driverName)`.
The argument that was a path may now also be:

- `s3://bucket/key` (translated internally to virtual-hosted-style HTTPS)
- `https://...` or `http://...` (used verbatim — must already be presigned or
  point to a public resource)
- any existing local path form (unchanged behavior)

## 2. Non-goals

- AWS SDK integration, credential providers, or request signing. v1 supports
  **presigned URLs only**. The library never sees access keys.
- Writing slides to S3. SlideIO is read-only.
- A new public stream type exposed to callers. The `RandomAccessStream`
  interface is internal.
- Listing / enumerating bucket contents (precludes auto-discovery of DICOM
  series companions over S3 — see §7).

## 3. Current state

All 11 drivers open files via local-filesystem APIs (libtiff `TIFFOpen`,
`std::ifstream`, `pole::compound_document`, DCMTK `DcmFileFormat::loadFile`,
FreeImage). `ImageDriverManager::openSlide(filePath, driverName)` accepts
only a path string. No abstraction exists for non-file byte sources.

A per-driver byte-reading survey is recorded in §6 — it determines the
phasing in §7.

## 4. Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Public API   ImageDriverManager::openSlide(uri)        │
│               ─ detects s3:// / https:// / path         │
│               ─ constructs the right RandomAccessStream │
│               ─ passes it down to the driver            │
└─────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────┐
│  Drivers      Receive a RandomAccessStream;             │
│               read through it (never fopen the path)    │
└─────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────┐
│  Adapters     ─ libtiff: TIFFClientOpen + read callbacks│
│  (imagetools) ─ std::streambuf adapter (for CZI)        │
│               ─ pole patch (v3, ZVI)                    │
│               ─ DCMTK DcmInputStream (v3, DCM)          │
└─────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────┐
│  RandomAccessStream   abstract interface                │
│  (slideio-base)       ┌───────────────┬────────────────┐│
│                       │ FileStream    │ HttpStream     ││
│                       │ (local files) │ (libcurl +     ││
│                       │               │  block cache)  ││
│                       └───────────────┴────────────────┘│
└─────────────────────────────────────────────────────────┘
```

**Key properties:**

- A single new abstraction — `RandomAccessStream` — is the universal seam.
- Existing local-file behavior is preserved by routing every `openSlide(path)`
  call through `FileStream` internally. Zero behavior change for current
  callers.
- Driver migration is gradual. Each driver advertises `supportsStream()`. The
  dispatcher refuses S3/HTTPS URIs for drivers that have not yet been
  migrated, with a clear error message.

## 5. The `RandomAccessStream` interface

Lives in `slideio-base` so every module (drivers, imagetools, public API) can
depend on it without circular module dependencies.

```cpp
namespace slideio {

class RandomAccessStream {
public:
    virtual ~RandomAccessStream() = default;

    // Total size of the underlying object in bytes.
    virtual uint64_t size() const = 0;

    // Read up to `size` bytes starting at `offset` into `buf`.
    // Returns the number of bytes actually read (0 only at/after EOF).
    // Throws on non-EOF errors (network failure, auth failure, etc.).
    // Thread-safe: multiple threads may call read() concurrently.
    virtual size_t read(uint64_t offset, size_t size, void* buf) = 0;

    // Advisory: caller knows it will soon read [offset, offset+size).
    // Implementations may ignore (FileStream) or warm the cache (HttpStream).
    virtual void prefetch(uint64_t offset, size_t size) {}

    // Human-readable identifier for logs and error messages.
    virtual std::string uri() const = 0;
};

} // namespace slideio
```

**Design choices:**

- **Stateless `read`** — offset is a parameter, not a cursor. Multiple threads
  may issue overlapping reads concurrently; adapters that need a seek-style
  interface implement the cursor themselves on top of the stateless API.
- **No write API.** SlideIO is read-only.
- **Thread safety required.** `FileStream` uses `pread` (POSIX) /
  `ReadFile` with `OVERLAPPED` (Windows). `HttpStream` synchronizes cache
  metadata access via a mutex; block-level fetches reuse libcurl's connection
  pool.

### 5.1 `FileStream`

Wraps a local file. Constructed from a path. Used by every `openSlide(path)`
call after migration. Implementation: `pread` on POSIX, `ReadFile` with
explicit `OVERLAPPED` offsets on Windows. Lives in `slideio-imagetools`.

### 5.2 `HttpStream`

Wraps a presigned/public HTTPS URL via libcurl. Owns the block cache. Lives
in `slideio-imagetools`. Detailed in §8.

## 6. Per-driver I/O survey

This table is what drives the v1→v3 phasing. It documents how each driver
currently reads bytes and the work needed to migrate it.

| Driver | I/O mechanism | Effort | Notes |
|---|---|---|---|
| **svs** | libtiff via `TiffTools::openTiffFile` | Trivial | Inherits TIFF chokepoint |
| **scn** | libtiff via `TiffTools::openTiffFile` | Trivial | Inherits TIFF chokepoint |
| **ndpi** | libtiff via `NDPITiffTools::openTiffFile` (parallel wrapper) | Trivial | Same change applied to NDPI's wrapper |
| **pke** | libtiff via `TiffTools::openTiffFile` | Trivial | Inherits TIFF chokepoint |
| **ometiff** | libtiff + small `ifstream` for companion XML | Trivial+ | TIFF chokepoint plus a small stream swap for companion XML |
| **afi** | `ifstream` for XML index + references to SVS files | Special | Inherits SVS automatically; needs URI-prefix preservation for referenced files |
| **vsi** | `std::ifstream` wrapped in `VSIStream` facade | Easy | Refactor `VSIStream` to back onto a `RandomAccessStream` |
| **czi** | Raw `std::ifstream` with `seekg`/`read` in many sites | Medium | `streambuf` adapter, swap `m_fileStream`'s buffer |
| **gdal** | FreeImage (`FIWrapper`); does **not** currently use GDAL `/vsis3/` | Medium | Route to GDAL `/vsis3/` or `/vsicurl/` virtual filesystem |
| **zvi** | `pole::compound_document` opens path directly; no callback API | Hard | Patch pole to accept a reader interface; upstream the patch |
| **dcm** | DCMTK `DcmFileFormat::loadFile(path)` | Hard | Subclass `DcmInputStream`, switch to lower-level `DcmFileFormat::read(DcmInputStream&)` API |

## 7. Phasing

### 7.1 v1 — Foundation + TIFF family (6 drivers)

Drivers shipping: **svs, scn, ndpi, pke, ometiff, afi.**

Work items:

| Item | Files |
|---|---|
| `RandomAccessStream` interface | new header in `slideio-base` |
| `FileStream` | new files in `slideio-imagetools` |
| `HttpStream` + libcurl + LRU block cache | new files in `slideio-imagetools` |
| Conan: add libcurl | `conanfile.txt`, CMake link rules |
| `TiffTools::openTiffFile(stream)` overload + `TIFFClientOpen` callbacks | `imagetools/tifftools.cpp/hpp` |
| `NDPITiffTools::openTiffFile(stream)` overload | `drivers/ndpi/ndpitifftools.*` |
| `ImageDriverManager::openSlide` URI dispatcher + `createStream` | `slideio/imagedrivermanager.*` |
| `ImageDriver::supportsStream()` virtual (default `false`) | `core/imagedriver.h` |
| Per-driver `openSlide(stream)` overloads + `supportsStream() = true` | one file per driver |
| AFI URI-prefix resolution | `drivers/afi/afislide.cpp` |
| OME-TIFF companion XML via stream | `drivers/ome-tiff/otslide.cpp` |
| `ImageDriverManager::setHttpCacheEnabled(bool)` global toggle | `slideio/imagedrivermanager.*` |
| Tests (see §10) | new test files |

### 7.2 v2 — Native-stream drivers (czi, vsi)

- **vsi:** Refactor `VSIStream` to back onto `RandomAccessStream` instead of
  `std::ifstream`. ~1 file. Easy.
- **czi:** Build a `std::streambuf` adapter that delegates to
  `RandomAccessStream`, and swap `m_fileStream`'s buffer to use this adapter.
  Existing `seekg`/`read` call sites compile and work unchanged. Medium.

### 7.3 v3 — Third-party-library drivers (gdal, zvi, dcm)

- **gdal:** Route S3/HTTPS URIs through GDAL's `/vsis3/` or `/vsicurl/`
  virtual filesystem. Path-translation layer in the gdal driver. Could be
  pulled forward to v1 but kept in v3 for consistency with the
  `RandomAccessStream`-everywhere model.
- **zvi:** Fork pole into the repo, add a reader-interface constructor to
  `pole::compound_document`, upstream the patch. `ZviSlide` constructs the
  compound document over a `RandomAccessStream`.
- **dcm:** Subclass DCMTK's `DcmInputStream` to back onto a
  `RandomAccessStream`, switch from `DcmFileFormat::loadFile` to the
  lower-level `DcmFileFormat::read(DcmInputStream&)` API.

**v3 limitation:** Multi-file DICOM series require directory enumeration,
which presigned URLs do not provide. Over S3, the DCM driver in v3 supports
**single-file DICOM** (including multi-frame DICOM files); multi-file series
require the caller to pass a manifest of presigned URLs (mechanism out of
scope for this design — to be specified separately if needed). This
limitation is accepted by the design.

## 8. `HttpStream` details

### 8.1 Block model

- The URL's byte range is partitioned into fixed-size **1 MB blocks**.
- Each `read(offset, size, buf)` is decomposed into the set of overlapping
  block indices. Each block: served from cache if present, otherwise fetched
  via one ranged GET and inserted into the cache.
- Consecutive missing blocks within a single `read` are coalesced into one
  multi-block GET — at most N HTTP round-trips for a read spanning N
  non-cached, non-contiguous blocks.

### 8.2 Cache

- Per-stream LRU keyed by block index.
- Capacity: **256 MB → 256 blocks per stream.**
- Per-stream (not process-wide) so concurrent slides do not evict each
  other's blocks.
- Memory only; nothing touches disk in v1.
- Can be **disabled at runtime** via
  `ImageDriverManager::setHttpCacheEnabled(false)`. When disabled, every
  `read` triggers a fresh ranged GET. The flag is an atomic bool consulted
  by `HttpStream` on each cache lookup; toggling it does not invalidate
  cached blocks in existing streams.

### 8.3 libcurl usage

- One `curl_multi` handle per `HttpStream`, plus a small pool (e.g., 4) of
  `curl_easy` handles for connection reuse and keep-alive.
- HTTPS only via the system TLS provider (OpenSSL on Linux/macOS, Schannel
  on Windows) used by libcurl. HTTP/2 enabled if the build's libcurl
  supports it.
- HTTP (plaintext) is permitted in addition to HTTPS — useful for in-cluster
  MinIO and the test fixture (§10).
- Range header: `Range: bytes=<from>-<to>` per HTTP/1.1.
- Retries: 5xx and transient network errors retried with exponential
  backoff, bounded (e.g., 3 attempts). 4xx (except 429) fails immediately.
- 429 / `Retry-After` honored within the retry budget.

### 8.4 Size discovery

- On construction, `HttpStream` issues a HEAD request to read
  `Content-Length` and verify `Accept-Ranges: bytes`. If byte-range support
  is not advertised, construction fails fast with a clear error.
- If `Content-Length` is missing (some signed-URL configurations),
  `HttpStream` falls back to a `GET Range: bytes=0-0` and extracts the
  total size from `Content-Range`.

### 8.5 Thread safety

- All cache and `curl_multi` access is serialized by a single per-stream
  mutex. Cache lookup and metadata bookkeeping happen inside the mutex;
  `curl_multi_perform` polling for an outstanding fetch happens outside.
- Concurrent reads do not double-fetch the same block: the first arrival
  marks a block as "in-flight" and subsequent readers wait on the in-flight
  fetch's completion before serving from the now-populated cache.

### 8.6 URI handling

- `s3://bucket/key` → string-translated to
  `https://bucket.s3.amazonaws.com/key` (virtual-hosted style). No AWS
  region lookup; users who need region-pinned URLs pass `https://`
  directly.
- `https://...` and `http://...` are used verbatim.

## 9. Public-API integration

`ImageDriverManager::openSlide(uri, driverName)` keeps its current signature.
Dispatch logic:

```cpp
std::shared_ptr<CVSlide> ImageDriverManager::openSlide(
    const std::string& uri, const std::string& driverName)
{
    auto stream = createStream(uri);                    // dispatcher
    auto driver = selectDriver(uri, driverName);
    if (!driver->supportsStream())
        throw NotSupportedError(driver->name(), uri);
    return driver->openSlide(stream);                   // new overload
}
```

### 9.1 URI prefix detection (`createStream`)

| URI prefix | Stream constructed |
|---|---|
| `file://...` or absolute path or relative path | `FileStream(localPath)` |
| `s3://bucket/key` | `HttpStream("https://bucket.s3.amazonaws.com/key")` |
| `https://...` or `http://...` | `HttpStream(uri)` |

Detection is by prefix only. Anything that is not `s3://`/`http(s)://` is
treated as a local path, preserving current behavior including UNC paths on
Windows.

### 9.2 Driver pattern matching

Current `canOpenFile(filePath)` does a glob pattern match (e.g., `*.svs`).
After v1, the matcher strips any HTTP query string and matches on the URI's
path component (the part after the last `/` and before any `?`). For
`s3://bucket/path/slide.svs?X-Amz-...` the matcher sees `slide.svs`. No
driver-level changes are required.

### 9.3 Driver-base evolution

`ImageDriver` and `CVSlide` keep their existing path-based entry points and
gain new stream-based overloads (preserves all existing internal callers
and external bindings):

```cpp
class ImageDriver {
public:
    virtual bool supportsStream() const { return false; }
    // existing path-based open APIs unchanged
    // new stream-based open APIs added per migrated driver
};
```

The path-based overload's default implementation wraps the path in a
`FileStream` and forwards to the stream-based overload. Path-only callers
keep working.

## 10. Testing strategy

Tests sit at four layers. Most volume is at layers 1-3 where we control the
implementation, not at the live-network layer.

### 10.1 Layer 1 — `RandomAccessStream` contract tests

A shared GTest fixture exercises a uniform suite of read patterns against
any `RandomAccessStream` implementation. Backends exercised:

- `FileStream` against a temp file.
- `MemoryStream` — a test-only `RandomAccessStream` backed by a
  `std::vector<uint8_t>`. Used here and in §10.2 adapter tests.
- `HttpStream` (against the §10.3 fixture).

Cases: read at 0; read past EOF (returns short / 0); read of size 0; full
file in one call; full file in 4 KB chunks; random offsets/sizes (seeded
RNG); concurrent reads from N threads; `prefetch` callable as no-op.

### 10.2 Layer 2 — Adapter tests

Independent of any real S3:

- **`TIFFClientOpen` adapter:** Feed a `MemoryStream` containing a small
  known TIFF into `TiffTools::openTiffFile(stream)`. Walk directories, read
  tiles, verify pixel data matches the same TIFF opened by path. Guarantees
  v1's TIFF drivers will work over any `RandomAccessStream` backend.
- **`streambuf` adapter (v2 prep):** Feed a `MemoryStream` into the new
  streambuf wrapper around a `std::istream`, verify `seekg`/`read` patterns
  match a real `ifstream` on the same bytes.

### 10.3 Layer 3 — `HttpStream` against a local HTTP fixture

A small Python `http.server` (or equivalent in-process socket server)
running on `127.0.0.1` with range-request support. No real-AWS access
needed in CI. Tests:

- Block size and coalescing: assert exact GET count for a given read
  pattern.
- LRU eviction: read enough distinct blocks to overflow the 256 MB cache,
  verify oldest blocks are re-fetched.
- `setHttpCacheEnabled(false)`: every `read` triggers a GET.
- HEAD-then-GET size-probe sequence; `Content-Range` fallback when
  `Content-Length` missing.
- Retry on 5xx (server programmed to fail twice then succeed).
- Fast-fail on 404.

### 10.4 Layer 4 — Integration

End-to-end smoke for v1:

- Local HTTP fixture serves an existing small SVS test slide from the
  repo's test-data set.
- `openSlide("http://127.0.0.1:<port>/test.svs")` returns a working
  `Slide`.
- Render a scene region; bytes match the result of opening the same file
  via local path.
- Repeated for one driver per group: svs, ndpi, ome-tiff, afi.

A separate CI job (not gating the unit suite) can additionally smoke-test
against a real public-data S3 bucket or a CI-only test bucket with a
long-lived presigned URL stored as a secret.

### 10.5 Out of scope for v1 tests

- Real AWS credentials in CI (not needed — presigned URLs only).
- Cross-region behavior (handled below libcurl + DNS).
- Performance benchmarking — lives in `single_tests/`, added separately.

## 11. Build / dependencies

- **libcurl** added to `conanfile.txt`. Linked into `slideio-imagetools`.
- No AWS SDK dependency.
- TLS is provided by the system TLS stack via libcurl (OpenSSL on
  Linux/macOS, Schannel on Windows) — already present on supported build
  platforms.

## 12. Backwards compatibility

- `openSlide(path, driverName)` signature unchanged.
- All existing local-path workflows continue to work; they internally route
  through `FileStream`.
- Driver `canOpenFile` and pattern-matching semantics unchanged for path
  inputs; URI inputs are normalized before pattern matching.
- C++ ABI: new virtual methods are added to `ImageDriver` / `CVSlide` with
  default implementations. Out-of-tree consumers of these classes (if any)
  recompile cleanly without source changes.

## 13. Out of scope (recap)

- AWS SDK, credential providers, request signing.
- Bucket listing / directory enumeration.
- Auto-discovery of DICOM series companions over S3 (v3 limitation
  documented in §7.3).
- Disk-backed cache layer (memory only in v1).
- Public exposure of `RandomAccessStream` as part of the SDK surface.
- Async / coroutine-based public API.

## 14. v1 implementation status (2026-05-27)

Shipped on branch `s3` (commits `b9f78c0`..`e76d029`). Full test suite: 527
passing, 0 failing. Local-path behavior is byte-identical to before; every
driver migration is an additive stream branch verified by a byte-exact
path-vs-stream parity read.

**Delivered (matches design):** `RandomAccessStream` (§5), `FileStream`,
`BlockCache` (1 MB blocks / 256-block LRU, §8.2), `HttpStream` (libcurl,
HEAD→Content-Range size probe, ranged-GET block fetch with run coalescing,
3-attempt exponential-backoff retry, runtime cache toggle, §8), the
`TIFFClientOpen` adapter (§F), URI dispatcher + `matchPattern` query-strip
(§9), public-API URI gating + `setHttpCacheEnabled` (§9.3), and the six
TIFF-family drivers (svs, scn, ndpi, pke, ome-tiff, afi). NDPI additionally
required an `NDPIDataSource` + libjpeg source manager because its JPEG/MCU
path bypasses libtiff. AFI resolves referenced SVS files via `siblingUri` +
`createStream` and was validated end-to-end over the HTTP fixture.

The converter CLI (`slideio_converter`) accepts `s3://` and `http(s)://`
URIs as input as of 2026-05-28; output remains a local file path (per §1
non-goal). See
[`2026-05-28-converter-s3-input-design.md`](./2026-05-28-converter-s3-input-design.md).

**v1 deviations / known limitations (carry into v2+):**
- **Multi-file OME-TIFF over a stream is not supported** — a stream open that
  references sibling TIFF files raises a clear error
  (`TIFFFiles::getOrOpen`). Single-file OME-TIFF and all local multi-file
  OME-TIFF work. (Analogous to the §7.3 DICOM-series limitation: needs
  per-sibling stream resolution.)
- **OME-TIFF companion-XML over a stream is implemented but untested** — no
  single-file `BinaryOnly` test image was available. The path uses the same
  `siblingUri`/`createStream` helpers exercised elsewhere.
- **`Accept-Ranges: bytes` is not verified at construction** (TODO in
  `probeSize`, spec §8.4). A server that ignores `Range` and returns `200` is
  instead caught at fetch time by the "reject 200 at non-zero offset" guard,
  so there is no correctness hole — just a deferred, less-specific error.
- **429 / `Retry-After` is not specially handled** (§8.3) — 429 is treated as
  non-transient and fails immediately rather than retrying.

**Follow-up hardening (non-blocking, deferred):**
- Bound the companion-file/index allocation in the AFI and OME-TIFF readers by
  the server-reported `Content-Length` (defense-in-depth; low risk under the
  presigned-URL trust model).
- Remove or port the now-dead `FILE*`-only NDPI JPEG helpers
  (`readJpegScanlines`, `readJpegDirectoryRegion`, `readUncompressedScanlines`)
  so they cannot be reintroduced into the stream read path.
- The committed env-var Python mechanism in `src/tests/main/CMakeLists.txt`
  (`SLIDEIO_TEST_PYTHON`) was verified against a warm CMake cache, not a fully
  clean configure; confirm on a fresh build dir.
