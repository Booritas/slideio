# ZVI: concurrent reads, via three changes to pole

**Date:** 2026-09-09
**Branch:** v2.10.0
**Status:** Implemented 2026-09-09 (`extern/pole` at `4b49f49`)
**Baseline:** `slideio` @ `e9094277` (branch `v2.10.0`); `extern/pole` @ `3e64e5a`
(`v1.0.4-3-g3e64e5a`)
**Companion:** `software-docs/specs/2026-09-07-parallel-read-block-design.md`
(the design that built the machinery this one consumes, and whose §3.2 deferred
ZVI); `software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md` (the
previous deferred-driver conversion)
**Corrects:** `TECH_DEBT.md` §14 — the fix it recommends costs three orders of
magnitude more on the mosaic file (1721 ms) than on the ZVIs its "small"
estimate was evidently drawn from (0–3 ms), which makes that fix a net loss

---

## 1. Goal

Make two `read_block` calls on one ZVI `Scene` run at the same time, so
`ZVIScene` can report `supportsConcurrentReads() == true`.

ZVI is one of the three formats still serialised (with DCM and GDAL). The
machinery is in the tree and nine of the twelve formats use it:
`CVScene::supportsConcurrentReads()` and `lockIfSerialised()`, `ContextPool`
handing out `ReadContext` subclasses one borrower at a time, `FileReader` for
cursor-free positional I/O, and `TestTools::concurrentReadIdentityTest*` as the
byte-exactness gate.

What makes ZVI different from the nine is that its mutable read-path state is
not in slideio at all. It is in `extern/pole`, three layers down. So this
design is mostly a design for three changes to pole, and only incidentally a
change to the driver: the ZVI diff at the end is about thirty lines.

Of those three, only the last (§5.3) is what makes reads concurrent — §4
explains why the obvious alternative to it was rejected. The other two are a
latent-UB fix (§5.1) and a performance fix (§5.2); the second of those is worth
landing on its own merits whatever happens to the rest, and §3.3 is the
measurement that says so.

### 1.1 What this is not

Not a re-entrancy audit of the ZVI driver. The audit was done and the surface
is remarkably small; §2.1 records it. In particular the JPEG decode path needs
no work: `ImageTools::decodeJpegStream` goes to `jpeglibDecode`
(`jpeglib_aux.cpp:7`), which holds a per-call `jpeg_decompress_struct` and no
file-scope state, and it is already on VSI's and CZI's concurrent read paths
(`etsfile.cpp:159`, `czithumbnail.cpp:17`).

---

## 2. The blocker

### 2.1 What the read path actually touches

`ZVIScene::readResampledBlockChannelsEx` → `TileComposer::composeRect` →
`ZVIScene::readTile` → `ZVITile::readTile` → `ZVIImageItem::readRaster`.

Along that path, everything in slideio is already clean:

- `m_Tiles`, `m_ImageItems`, `m_TileCountX/Y`, `m_ChannelDataTypes`,
  `m_ChannelNames` and the rest of `ZVIScene`'s members are written in `init()`
  and read-only thereafter.
- `TilerData`, the `userData`, is stack-allocated per read
  (`zviscene.cpp:103`).
- `ZVITile::readTile` and `ZVIImageItem::readRaster` are both `const` and work
  on locals: `channelRasters`, `itemRaster`, `buff`.
- Everything `readRaster` needs about the item — `m_DataPos`, dimensions, pixel
  format, valid bits, item index — was resolved by `readItemInfo` during
  `init()`. Nothing is re-parsed per read.

**`ZVIScene::m_Doc` is the only shared mutable object the read path touches**,
and it is touched from exactly one place: `zviscene.cpp:137`, where `readTile`
passes it by reference into `ZVITile::readTile`. The other three uses
(`zviscene.cpp:291`, `301`, `398`) are all inside `init()`.

That single reference races on three independent layers inside pole:

| Layer | State | Consequence of a race |
|---|---|---|
| `ole::stream_path::stream()` (`includes/path.hpp`) | `_ref_count++` on a shared `unsigned char` | torn increment; only feeds `used()`, which the read path does not consult, so benign in effect but still a data race |
| `POLE::StreamImpl` (`includes/pole/detail/stream.hpp`) | `_pos`, `_state`, `_cache_data` / `_cache_pos` / `_cache_size` | `readRaster` does `seek()` then `read()`; two threads on one item interleave and each gets the other's offset — wrong pixels, silently |
| `POLE::StorageIO::loadBigBlocks` (`sources/pole/detail/storage.cpp:243`) | one `std::fstream`, `seekg(pos)` then `read()` per block | **the binding constraint** — threads reading *different* items still collide here |

The third layer is what makes this unfixable from the slideio side. Note also
that `compound_document::init()` (`sources/storage.cpp:89`) eagerly creates one
`POLE::Stream*` per stream in the file and stores it in the `_storages` tree, so
every caller that resolves the same path gets the *same* `StreamImpl` — there is
no per-borrower stream object to hand out even if slideio wanted one.

### 2.2 A latent bug found on the way

`StreamImpl::_state` is never initialised. `init()`
(`sources/pole/detail/stream.cpp:52`) sets `_pos`, `_cache_pos`, `_cache_size`
and `_cache_data`, and assigns `_state` only on the two `Bad` error paths. So
`fail()` and `eof()` read indeterminate memory for any successfully-opened
stream. slideio does not depend on either for correctness — `ZVIUtils::readExactly`
and `skipExactly` check returned byte counts and remaining length instead
(`zviutils.cpp:37`, `52`), which is exactly why this has never surfaced — but it
is undefined behaviour on a path this design touches, and §5.1 fixes it.

---

## 3. Correcting `TECH_DEBT.md` §14

### 3.1 What §14 recommends

> The work is now bounded: add a `ReadContext` subclass holding an
> `ole::compound_document`. […] A per-thread document costs N x the OLE FAT and
> directory parse, which is small for a ZVI in a way it never is for CZI.

The mechanism is right — a per-thread `compound_document` does remove all three
races, because each one owns its own `fstream`, its own `StreamImpl` objects and
its own `stream_path` tree. The cost claim is what fails.

### 3.2 Measured cost of a per-thread `compound_document`

Measured with a standalone probe linked against the built `pole.lib`, timing
`ole::compound_document` construction alone (not `ZVIScene::init`, which also
parses per-item tags and which a `ReadContext` would *not* repeat):

| File | Size | Streams | Construct | Resident |
|---|---|---|---|---|
| `zvi/TOMMAlexaFluor647.zvi` | 2.8 MB | 10 | 0 ms | 0.2 MB |
| `zvi/Zeiss-1-Merged.zvi` | 9.7 MB | 19 | 1 ms | 0.5 MB |
| `zvi/Zeiss-1-Stacked.zvi` | 108 MB | 105 | 3 ms | 1.0 MB |
| `zvi/mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi` | 397 MB | 315 | 26 ms | 2.7 MB |
| `zvi/openslide/Zeiss-3-Mosaic.zvi` | 2.0 GB | 1543 | **1721 ms** | **12 MB** |

Two things to read off this. The mosaic number is 1.7 s, not "small". And cold
and warm runs are within 5 ms of each other (1721 / 1716), so it is parse-bound,
not I/O-bound — an OS page cache does not rescue it and neither would an SSD.

With `ContextPool::defaultMax()` = `min(8, hardware_concurrency())`, the first
eight concurrent reads of the mosaic would each block ~1.7 s building a context,
for ~96 MB total, in order to then read tiles that cost 45 ms cold and 2 ms
warm. **The fix §14 recommends would leave a mosaic reader slower than the mutex
it removes.** That is the correction: not that the entry is imprecise, but that
following it as written makes the flagship case worse.

Resident cost, by contrast, is fine: 12 MB × 8 is not a reason to avoid
anything.

### 3.3 Where the 1.7 s goes — and why it is pole's, not the format's

The scaling is superlinear: 4.9× the streams (315 → 1543) costs 66× the time
(26 ms → 1721 ms). It is not the FAT parse, which is linear in file size and
identical for the 397 MB and 2.0 GB cases at the sector level.

`compound_document::init()` calls `Storage::stream()` once per directory entry
and `enterDirectory()` once per storage. Both reach
`DirTree::find_siblings()` (`sources/pole/detail/dirtree.cpp:347`), which is
recursive over a sibling chain and runs **three O(`result.size()`) linear dedup
scans per node** — making it O(k²) in the number of siblings k. The mosaic has
~514 entries under `/Image`, so each of those ~3000 resolutions costs ~132k
operations.

Two call chains reach it:

- `DirTree::_entry(const std::string&)` (`dirtree.cpp:124`) walks the path
  component by component, calling `children()` → `find_siblings()` for each.
  Reached from `Storage::stream()`'s `io->entry(name)` and from
  `enterDirectory()`.
- `DirTree::parent()` (`dirtree.cpp:73`) is brute force by its own comment —
  it iterates every entry calling `children()` on each — and `fullName()`
  (`dirtree.cpp:92`) calls it once per level of nesting. `Storage::stream()`
  (`sources/pole/pole.cpp:78`) invokes that through `path(path_)`.

Each contribution was measured by patching a copy of pole and re-running the
probe on the mosaic:

| pole as it stands | 1721 ms |
|---|---|
| + guard `path(path_)` behind the relative-name test | 1175 ms |
| + O(1) dedup in `find_siblings` | **138 ms** |
| + `Storage::stream`'s reuse scan as a map (measured separately) | 136 ms |

So `parent()`-via-`fullName` is about a third of it and `find_siblings`' dedup
is the rest. Two details make the first line trivially safe to fix:
`Storage::stream()` calls `path(path_)` **unconditionally** even though the
result is only used when `name[0] != '/'`, and `compound_document::init()`
only ever passes absolute paths — so on this path the expensive string is
computed and then discarded. Its local `fullName` is in fact dead code
altogether: nothing after the `insert` reads it.

The reuse scan (`pole.cpp:91`) is a genuine O(n²) — a linear
`std::list<Stream*>` walk per entry — but it is worth **2 ms** of the 1721 and
§5.2 leaves it alone. Note for anyone tempted: it compares
`(*it)->path()`, which is the entry's *short* name, against `name`, which
callers pass as a full path, so the reuse lookup never matches at all today.
"Fixing" it either changes semantics (keying on the full path makes reuse start
working) or collides (keying on the short name makes every item's `Contents`
the same key).

So the 1.7 s is not the price of the OLE container format. It is two searches in
pole that nothing has needed to fix because nothing had opened a ZVI with 1543
streams and cared. Fixing them is §5.2, it is about fifteen lines, and it
delivers 12.5× on the mosaic and ~3× at 315 streams with no regression on small
files.

---

## 4. Design decision: shared document, not per-thread documents

Two routes remove the races. Both are viable after §5.2 lands.

**Route A — per-thread `compound_document` in a `ZVIReadContext`.** The §14
route, and structurally identical to what NDPI, SVS, PKE, SCN and OME-TIFF each
did. Costs, after §5.2: one `fstream` and ~12 MB per context on the mosaic, and
whatever the reduced construction time turns out to be. No pole API change.

**Route B — make pole's read path cursor-free, and share one document.** This is
the CZI and VSI shape: `FileReader` in the tree exists precisely because "a
single `FileReader` serves any number of threads with one descriptor — no pool,
no per-thread handle" (`core/tools/filereader.hpp:15`). CZI needs no
`ReadContext` at all for this reason.

**This design takes Route B**, for four reasons:

1. **The state being replicated is not state.** A `compound_document` is, apart
   from cursors, an immutable index: the directory tree, per-stream sector
   chains, the FAT. Route A replicates a read-only index N times to escape a
   cursor. Route B removes the cursor and shares the index — which is what
   `FileReader`'s existence in this tree already argues for.
2. **The positional read is already written.** `StreamImpl::read(pos, data,
   maxlen)` (`sources/pole/detail/stream.cpp:100`) is a *positional* overload
   that takes its offset as an argument and touches no cursor. It mutates one
   field, `_state`, and is otherwise re-entrant today. Route B is largely a
   matter of exposing it and fixing what is under it.
3. **Route A's cost never fully goes away.** Even at 10 ms per document, eight
   contexts is 96 MB of duplicated index on the mosaic, and the arithmetic gets
   worse for the larger mosaics users have that the corpus does not.
4. **Route B is faster single-threaded too.** §5.3.

Route A is not wrong, and if Route B's scope is judged too large for this
branch, Route A *after* §5.2 is a legitimate smaller version of this work. It is
recorded in §8 rather than deleted.

---

## 5. Design

Three pole changes (§5.1–§5.3), then the driver (§5.4). §5.1 and §5.2 are
independent of concurrency and of each other; §5.3 depends on §5.1.

All three are upstream changes to `github.com/Booritas/pole`, so
`extern/pole`'s submodule pin moves. pole builds against nothing but the
standard library and must stay that way — `CLAUDE.md` records that as the reason
it is a submodule rather than a Conan package — so §5.3 cannot use
`slideio::FileReader` itself and reimplements its primitive. That duplication is
deliberate and is called out in §5.3.

### 5.1 Fix `_state`, and stop the positional read from writing it

`sources/pole/detail/stream.cpp`, `includes/pole/detail/stream.hpp`.

- Initialise `_state = 0` in `StreamImpl::init()`, and give it a default
  initialiser in the header so the copy constructor (which does not copy
  `_state` today) cannot leave it indeterminate either.
- Split the positional read in two. `read(size_t pos, unsigned char* data,
  std::streamsize maxlen)` becomes `const` and returns the byte count without
  touching `_state`; the cursor-based `read(unsigned char*, std::streamsize)`
  keeps setting `Eof` from the count its `const` sibling returns, so existing
  behaviour through `Stream::read` is unchanged.
- `update_cache()` and `getch()` stay exactly as they are. They are cursor-based
  by definition, they are only reached from the cursor API, and §5.4 keeps the
  read path off them.

This is a prerequisite for §5.3 — a `const` positional read is the thing being
exposed — and a UB fix regardless.

### 5.2 Make the directory-tree searches not brute force

`sources/pole/detail/dirtree.cpp`, `includes/pole/detail/dirtree.hpp`,
`sources/pole/pole.cpp`.

Two changes, about fifteen lines together, both local and both
behaviour-preserving. §3.3 has the measurement that picked them.

- **Guard the discarded `path()` call.** In `Storage::stream()`
  (`sources/pole/pole.cpp:78`), move `std::string path_; path(path_);` inside
  the `if( name[0] != '/' )` branch that is the only consumer of its result.
  `compound_document::init()` passes only absolute paths, so today this
  computes an expensive string and throws it away — 546 ms of the mosaic's
  1721. The local `fullName` it feeds is dead code: nothing after the `insert`
  reads it. Leave `fullName` in place anyway; deleting it is a separate
  tidy-up and this change wants to stay two lines.
- **Give `find_siblings` an O(1) duplicate check.** It currently runs three
  linear scans of `result` per node. Add a `std::vector<char>& visited`
  parameter, have `children()` — its only non-recursive caller — create the
  buffer sized to `entryCount()`, and test and set `visited[index]` instead of
  scanning. This is worth a further 1037 ms.

  A `mutable std::vector<char>` member on `DirTree`, reset at the top of
  `children()`, measures ~26 ms faster than allocating per call (112 ms against
  138 ms on the mosaic). **Take the parameter, not the member.** `children()`
  is `const`, and a `const` method that writes a member is not safely callable
  from two threads — a property worth preserving on its own terms in a change
  whose whole purpose is concurrency, even though §2.1 establishes that
  `DirTree` is not on the ZVI read path today. 26 ms does not buy that back.

Two things deliberately **not** done, both of which an earlier draft of this
section prescribed and the measurement then ruled out:

- **No parent map.** Caching each `DirEntry`'s parent index to replace
  `DirTree::parent()`'s brute-force search would be dead code: `parent()` is
  reached only through `fullName()`, and the first change above stops
  `fullName()` being called at all on this path. It also would have needed
  invalidation across `delete_entry`, `DirTree::save` and the `create` branch
  of `_entry` — mutation paths with **zero** test coverage (§6.4), so no safety
  net under exactly the code most likely to break.
- **No map for `Storage::streams`.** The linear reuse scan is a real O(n²) and
  is worth 2 ms of 1721. §3.3 records why touching it is worse than leaving it:
  the comparison never matches today, so any correct-looking replacement
  changes behaviour.

Measured effect of the two changes, warm, mosaic verified to still resolve
`/Image/Item(0)/Contents` afterwards:

| File | Streams | Before | After |
|---|---|---|---|
| `TOMMAlexaFluor647.zvi` | 10 | 0 ms | 0 ms |
| `Zeiss-1-Merged.zvi` | 19 | 1 ms | 1 ms |
| `Zeiss-1-Stacked.zvi` | 105 | 3 ms | 2 ms |
| `20140505_mouse_…_005.zvi` | 315 | 26 ms | 9 ms |
| `Zeiss-3-Mosaic.zvi` | 1543 | 1721 ms | 138 ms |

12.5× on the mosaic, ~3× at 315 streams, nothing lost on the small files. The
plan gates on the equivalence of the paths the document reports rather than on a
wall-clock assertion, which would flake in CI; the timings are reported, not
asserted.

**Measured again after the whole change landed**, warm, two consecutive runs
per file: the mosaic's `ole::compound_document` construction takes 139 ms and
140 ms, the 315-stream mouse file 10 ms and 10 ms, `Zeiss-1-Stacked.zvi` 2 ms,
and the two smallest 0–1 ms. Storage and stream counts matched the table's
column exactly on every file. So the "After" column above held, and §5.3's
positional read path did not regress the open — 1721 ms → ~138 ms is the
defensible before/after for this work.

**This change stands alone.** It speeds up every ZVI open in the library,
single-threaded, on the largest files, and it should land and be releasable
independently of anything else here.

### 5.3 Give pole a cursor-free read

`includes/stream.hpp`, `includes/path.hpp`, `includes/pole/pole.h`,
`sources/pole/detail/storage.cpp`, `includes/pole/detail/storage.hpp`.

**At the bottom — positional file I/O.** `StorageIO` holds
`std::iostream* _stream` and `std::fstream* _file`, and `loadBigBlocks()` does
`seekg(pos)` then `read()` per block. Replace the read path with a positional
primitive: `ReadFile` with an `OVERLAPPED` offset on Windows, `pread` elsewhere,
wrapped in a small `StorageIO`-private helper with a retry loop for short reads.
This is `slideio::FileReader::readAt` and its `fillFrom` retry loop reimplemented
inside pole, ~40 lines, because pole may not depend on slideio-core. The
duplication is the price of pole staying standard-library-only, and both copies
should carry a comment naming the other.

Constraints on that replacement:

- `loadBigBlocks`, `loadBigBlock`, `loadSmallBlocks` and `loadSmallBlock` all
  become `const`. They are already logically const — they read the FAT
  (`AllocTable::follow` is already `const`) and copy bytes out.
- `StorageIO(std::iostream*)` has no positional equivalent for a caller-supplied
  stream. slideio never uses it. Keep it, and have it fall back to a mutexed
  `seekg`+`read` so that constructor's behaviour is preserved and only the
  filename constructors get the concurrent path. Do not delete it in this change.
- `saveBlock()` and `flush()` keep `_file` and `seekp`. Writes stay serialised
  and out of scope.
- `_size`, `_header`, `_dirtree`, `_bbat`, `_sbat`, `_sb_blocks` are all built
  in `load()` and read-only after. `_streams`, the `std::list<StreamImpl*>`
  member of `StorageIO`, is unused on the read path.

**At the top — a positional stream API.** On `ole::basic_stream`:

```cpp
std::streamsize read_at(std::streamoff offset, char* buf, std::streamsize n) const;
std::streamoff  size() const;
```

`read_at` forwards to a new `POLE::Stream::read_at`, which forwards to the
`const` positional `StreamImpl::read(pos, ...)` from §5.1. `size()` exposes
`StreamImpl::size()`, which reads `_entry->size()` and is already const — this
is what lets a caller stop using `seek(0, std::ios::end)` to learn a length.
Neither touches `_pos`, `_state` or the cache.

The existing cursor API (`seek`, `read`, `pos`, `eof`, `fail`) is untouched.
ZVI's init-time tag parser is built on it — `readAllTags`, `readItem`,
`skipItem`, `bytesLeft` all walk a stream sequentially — and rewriting that is
neither necessary nor wanted, because it runs once, single-threaded, before any
read.

**The `_ref_count` race.** `stream_path::stream()` increments `_ref_count` and
returns a non-const reference. Add:

```cpp
const ole::basic_stream& stream() const;   // no ref-count mutation
```

`_ref_count` exists only to answer `used()`, which only `entry_can_be_deleted`
consults. A `const` accessor that does not bump it is correct for read-only
borrowing and removes the third race outright. `StreamKeeper` (§5.4) uses it.

**What is deliberately still shared and safe after this:** the `_storages` tree,
`DirTree` (its `_current` is written only by `enterDirectory`/`leaveDirectory`,
both init-time), every `StreamImpl::_blocks` sector chain (built at construction,
read-only after), and the `AllocTable`s.

**Throughput — and, as it turned out, a cost.** `loadBigBlocks` issues one
`seekg`+`read` pair per block and `loadBigBlock` heap-allocates a one-element
`std::vector<ULONG32>` per block, so the 2.9 MB tile in the mosaic costs ~5600
syscalls and ~5600 vector allocations. Measured cold read of that tile is
61 MB/s; warm it is 1437 MB/s, which is the same code with the page cache
absorbing the syscalls. Coalescing
runs of contiguous sectors into one positional read is a natural thing to do
while rewriting this function and would move the cold number materially. It is
**optional** for this design and should be a separate commit with its own
before/after, so that a regression in it is separable from the concurrency work.

That separability was worth having, but "as a bonus" was the wrong
expectation and is corrected here. Leaving the coalescing out cost about
**20%** on a warm single-threaded read of that tile — 1.80 ms before against
2.16 ms after, n=15 each side with non-overlapping ranges. The positional path
is a concurrency win paid for single-threaded until the coalescing lands;
`software-docs/TECH_DEBT.md` §21 carries the numbers and the argument for why
that trade is acceptable.

### 5.4 The driver

`src/slideio/drivers/zvi/`. Small, and entirely a consequence of §5.3.

**`ZVIImageItem::readRaster`** (`zviimageitem.cpp:204`) is the whole read-path
change. Today:

```cpp
stream->seek(getDataOffset(), std::ios::beg);
stream->seek(0, std::ios::end);
std::streampos endPos = stream->pos();
std::streamsize bytesToRead = endPos - getDataOffset();
stream->seek(getDataOffset(), std::ios::beg);
stream->read(reinterpret_cast<char*>(buff.data()), bytesToRead);
```

becomes, in both branches, `size()` to learn the length and a single `read_at`
from `getDataOffset()`. The redundant first `seek` on line 218 goes away with
it. The short-read check on the uncompressed branch (`readBytes != rasterSize`,
line 237) stays and stays load-bearing: it is how a truncated file is caught
now that nothing consults `eof()`.

**`ZVIUtils::StreamKeeper`** (`zviutils.hpp`, `zviutils.cpp:306`) gains a
`const`-borrowing form returning `const ole::basic_stream&`, used by
`readRaster`. Its existing non-const form stays for the init-time parsers.
`find_storage` and `storage_path::find_stream` are linear scans over the
`_storages` tree — `find_storage` walks every storage to match a string — but
they mutate nothing, so they are safe to share. They are, however, now on the
per-item read path: ~1543 string comparisons per `readRaster`. Not a correctness
issue, and not addressed here; §8 records it.

**`ZVIScene`** gains

```cpp
bool supportsConcurrentReads() const override { return true; }
```

and nothing else. `m_Doc` stays a plain member, shared by all readers, with no
`ContextPool` and no extra descriptor. `ZVITile::readTile` keeps its
`ole::compound_document&` parameter; only what `readRaster` does with it
changes. `m_Doc` is declared before `m_Tiles` and `m_ImageItems` in
`zviscene.hpp` and must stay that way — a reader in flight holds pointers into
`m_ImageItems`.

---

## 6. Verification

### 6.1 Byte-exactness

`src/tests/main/test_zvi_driver.cpp`, using the existing harness:

- `concurrentReadsAreByteIdentical` on `zvi/Zeiss-1-Merged.zvi` via
  `TestTools::concurrentReadIdentityTestAllPaths` — covers all-channels, single
  channel, out-of-order channel subset, and the level-addressed path.
- The same on `zvi/Zeiss-1-Stacked.zvi`, which is the multi-Z case. ZVI resolves
  a z-slice inside `readTile` through `TilerData::zSliceIndex`, so a Z-stack
  exercises `ZVITile::getImageItem`'s per-slice item lookup under concurrency;
  `Zeiss-1-Merged` does not.
- `concurrentReadsAreByteIdenticalMosaic` on
  `zvi/openslide/Zeiss-3-Mosaic.zvi` — the multi-tile case, and the only file
  where one block read spans many items and therefore many `StreamImpl`
  objects. This is the test that would have caught the `StorageIO::_stream`
  race, and the reason it must be in the set even at 2.0 GB.
Each guarded by `SLIDEIO_SKIP_IF_IMAGE_MISSING`, as the suite requires.

### 6.1.1 The contract test that must be replaced, not added to

`src/tests/main/test_concurrency_contract.cpp` already contains
`ConcurrencyContract.zviIsStillSerialised`, which opens
`zvi/mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi` through
`expectSerialised()` and asserts `supportsConcurrentReads() == false`. It will
fail the moment §5.4 lands, and its failure message states the procedure this
design is following:

> this driver now reports concurrent reads -- update TECH_DEBT,
> BREAKING_CHANGES.md and CLAUDE.md, and give it a byte-exactness test, before
> changing this expectation

So the change is: delete `zviIsStillSerialised`, and add a positive
`ConcurrencyContract.zviReportsConcurrentReads` in its place, on the same image.
`ZVIImageDriver` is linked into `slideio_tests`, so unlike the OME-TIFF case
this assertion belongs in that shared contract file rather than in the driver
test file. `expectSerialised()` itself stays — DCM and GDAL still use it — and
its comment should keep naming only the two drivers that remain.

### 6.2 Sanitisers

The mosaic byte-exactness test under TSan is the real gate on §5.3, because the
`_ref_count` and `_state` races are the kind that a byte-comparison test will
not reliably fail on. If TSan is not currently wired up for this tree, the plan
should say so and say what is being run instead, rather than implying coverage
that does not exist.

### 6.3 Descriptors and memory

One open ZVI must hold exactly one file descriptor after this change, and
`ZVIScene`'s resident cost must not grow with thread count. Both are the point
of Route B and both should be asserted, the descriptor count by the same means
the OME-TIFF work used.

### 6.4 pole's own tests

Two prerequisites the plan must handle before a single pole test can run:

- **`extern/pole/googletest/` is empty.** It is a *nested* submodule, and the
  `git submodule update --init` that `CLAUDE.md` prescribes for a fresh clone
  does not fetch it — that is deliberate, since the slideio build forces
  `PACKAGE_TESTS OFF` and never needs it. Adding pole tests means initialising
  it inside `extern/pole` first.
- **pole's tests are not part of the slideio build tree.** The slideio root sets
  `PACKAGE_TESTS OFF CACHE BOOL … FORCE` (`CMakeLists.txt:204`) for both
  jpegxrcodec and pole, so `storage_tests` is never configured. Running it
  needs a separate standalone configure of `extern/pole`.
- **Editing a pole header does not reach the slideio build.** The root
  `CMakeLists.txt:231` stages `includes/` into
  `build/extern/pole/include/pole` with `file(COPY)` — a *configure-time*
  snapshot. After editing any pole header, that staged copy must be refreshed
  or slideio compiles against the stale one, with no error.

Existing coverage is five read-only tests and must keep passing. New pole tests
for: `read_at` against the cursor `read` on the same stream (identical bytes,
cursor unmoved), `read_at` past end-of-stream, `read_at` from many threads on
one document, `_state` being `0` on a freshly opened stream, and — for §5.2 —
every storage and stream path the document reports still resolving after the
change, which is the assertion that makes that optimisation behaviour-preserving
rather than merely fast.

### 6.5 Regression on the whole suite

Full `slideio_tests` run, plus a before/after on ZVI open time for each file in
§3.2's table to confirm §5.2 delivered and §5.3 did not regress it.

---

## 7. Documentation updates

- **`TECH_DEBT.md` §14** — rewritten. Per §3, the entry's mechanism is right and
  its cost estimate is wrong: "small for a ZVI in a way it never is for CZI"
  must go, replaced by §3.2's table and the conclusion that the per-thread
  document it recommends would leave a mosaic reader slower than the mutex.
  Marked resolved if §5.3 and §5.4 land; if only §5.2 lands, the entry stays
  open but records the corrected cost and that Route A has become affordable.
  The entry's stated alternative ("resolve every stream's `(offset, length)`
  once at `init()` and read via `FileReader`") should be updated too: it
  describes moving mini-FAT handling into slideio, and §5.3 is the same idea
  done where that logic already lives.
- **`TECH_DEBT.md` §4** — the list of formats that still serialise loses ZVI;
  the "Kept open because ZVI, DCM and GDAL" sentence becomes DCM and GDAL.
- **`TECH_DEBT.md` §15 (DCM)** — one cross-reference: §15 offers the same
  per-thread-replica route, and §3.2 here is evidence that the cost of a replica
  needs measuring before it is recommended, not after. §15 already says "N x
  parse is expensive here, so choose the pool's cap accordingly", which is the
  right instinct; a pointer to §3.2's method is worth adding.
- **New `TECH_DEBT.md` entries** for the five things this work found and
  deliberately did not fix: pole opening every document `in | out` (so a
  read-only ZVI cannot be opened at all, and an open one holds two
  descriptors); `StreamImpl::_state &= Eof` where `&= ~Eof` was meant;
  `find_storage`'s linear scan, now on the read path (§8); sector coalescing in
  `loadBigBlocks`, scoped out of §5.3; and two defects in code §5.2 touches but
  leaves — `compound_document::path_exist()` returning `false` for every nested
  stream path, and `Storage::stream()`'s reuse lookup comparing a short name
  against a full path so that it never matches.
- **`BREAKING_CHANGES.md`**, under `v2.10.0` — the existing "`Scene` block reads
  may now overlap" entry lists the converted formats and says "ZVI, DCM and GDAL
  are unchanged". Move ZVI across; nine of twelve becomes ten of twelve. No
  slideio signature changes. pole's exported API does change and should be
  recorded: `basic_stream` gains `read_at`/`size`, `stream_path` gains a `const`
  `stream()`, and several `StorageIO` members become `const`.
- **`CLAUDE.md`** — the concurrency-contract bullet's "Concurrent today:" list
  gains ZVI. The dependencies section's pole paragraph should note that pole now
  carries a positional-read path and why it duplicates `FileReader`'s primitive
  rather than depending on it.
- **`extern/pole`** — its own README should say the read path is
  positional and safe for concurrent readers on one document, since that is now
  a property external consumers may rely on.

---

## 8. Out of scope

- **Converting DCM or GDAL.** Only `TECH_DEBT.md` §15 and §16 are touched, and
  §16 not at all. DCM's blocker is DCMTK's `CIF_UsePartialAccessToPixelData`
  state inside a shared `DcmDataset`, which has no positional-read escape and
  genuinely needs replicas.
- **Route A, per-thread `compound_document`s.** Rejected in §4, but recorded as
  the legitimate smaller version of this work if Route B's scope is judged too
  large — provided §5.2 lands first, without which it is a regression.
- **`find_storage`'s linear scan.** `compound_document::find_storage`
  (`sources/storage.cpp:133`) walks the whole `_storages` tree comparing
  strings, and after §5.4 it runs once per item per read — ~1543 comparisons per
  `readRaster` on the mosaic. It mutates nothing, so it is a throughput matter,
  not a correctness one, and it wants its own measurement before anyone
  restructures the tree into a map. New `TECH_DEBT.md` entry.
- **Sector coalescing in `loadBigBlocks`.** Described in §5.3 as optional and
  wanted; explicitly separable, with its own before/after, so a regression in it
  does not implicate the concurrency change.
- **Rewriting ZVI's tag parser onto the positional API.** `readAllTags`,
  `readItem`, `skipItem` and `bytesLeft` are sequential by nature, run once at
  `init()`, and are single-threaded. The cursor API stays for them.
- **pole's write path.** `saveBlock`, `flush`, `delete_entry` and
  `StreamImpl::write` keep the shared `_file` and stay serialised. slideio opens
  ZVI read-only.
- **ZVI read granularity.** `readRaster` reads a whole item — a full camera tile
  for every requested channel — regardless of how small the requested rect is,
  and `ZVITile::readTile` then `cv::extractChannel`s a copy per channel out of
  multi-channel items. Real, on the read path, and untouched: it shapes how much
  work one "tile" is but not whether two reads can overlap.
- **`read_batch`.** This removes the last blocker in this driver for a batched
  reader built on top; it does not build one.
