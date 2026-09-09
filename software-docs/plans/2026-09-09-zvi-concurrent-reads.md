# ZVI Concurrent Reads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let two `read_block` calls on one ZVI `Scene` run at the same time, so
`ZVIScene` reports `supportsConcurrentReads() == true` — taking slideio from
nine concurrent formats to ten — and make opening a large mosaic ZVI stop
costing 1.7 seconds on the way.

**Architecture:** ZVI's only shared mutable read-path object is
`ZVIScene::m_Doc`, an `ole::compound_document` from the vendored `extern/pole`
submodule. Rather than replicating that document per thread (measured at 1721 ms
and 12 MB each on the 2.0 GB mosaic — a net loss), pole's read path is made
cursor-free: a positional `read_at` on `ole::basic_stream`, positional file I/O
under `StorageIO::loadBigBlocks`, and a `const` stream borrow that does not bump
a refcount. One shared document then serves every thread with one descriptor,
the same shape CZI and VSI already use via `FileReader`. A separate,
independently valuable fix replaces two brute-force searches in pole's directory
tree that account for the 1.7 s.

**Tech Stack:** C++17, CMake 3.10+, Conan v2, GoogleTest, OpenCV. Two git
repositories: `slideio` and the `extern/pole` submodule
(github.com/Booritas/pole). No new dependencies — pole must stay
standard-library-only.

**Spec:** `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md`

## Global Constraints

- C++17. Match each file's surrounding style, comment density and naming. pole
  is vendored third-party code with its own conventions (tabs, `_leading`
  member names, K&R braces) — follow **pole's** style inside `extern/pole`, not
  slideio's.
- **pole may not depend on slideio, Conan, or anything outside the C++ standard
  library plus platform headers.** `CLAUDE.md` records this as the reason pole
  is a submodule rather than a Conan package. This is why Task 4
  reimplements `FileReader::readAt`'s primitive instead of calling it.
- **Two repositories.** Every pole task commits inside `extern/pole` *and* then
  commits the updated submodule pointer in `slideio`, so the slideio tree is
  never left with a dirty submodule. Work on a branch in both, off `v2.10.0`
  in slideio and off `master` in pole.
- **Behaviour preservation on pole's write path is absolute.** `saveBlock`,
  `flush`, `delete_entry`, `DirTree::save` and `StreamImpl::write` have **zero
  test coverage** (see Task 1, Step 4) and slideio never calls them. Do not
  refactor, "improve", or optimise any of them. Where a change could touch
  them, they take the old path unchanged.
- `ZVIScene::m_Doc` stays a plain member. **No `ContextPool`, no
  `ReadContext`** — that is the route the spec rejected in §4. If you find
  yourself adding one, stop and re-read spec §3.2.
- Exactly one descriptor per open ZVI for reading. Task 9 asserts it.
- No new slideio public API beyond the `supportsConcurrentReads()` override.
- Do not touch: `cvscene.*`, `FileReader`, `ContextPool`, any other driver,
  `Tiler`, `TileComposer`, `read_batch`, `TiffConverter`.
- Do not rewrite ZVI's init-time tag parser (`readAllTags`, `readItem`,
  `skipItem`, `bytesLeft`, `skipExactly`) onto the positional API. It is
  sequential by nature, runs once, single-threaded.
- Internal docs go in `software-docs/`, never `docs/` (the published Jekyll
  site).
- Test output must be pristine.
- All AI-generated code is human-reviewed before commit.

## Environment — read this before running anything

- Build: `cmake --build build --config Release --target <target> -- -m`
- Run: `./build/bin/Release/<name>.exe --gtest_filter="..."`
- Targets needed here: `slideio_tests`, and the `pole` target implicitly.
- **Never run `python install.py` in any form.** The build tree is configured
  and warm; `install.py -a conan` aborts partway in this repo and would break
  it. The commands in this plan are the working ones for this machine.
- **Run every test command in the FOREGROUND.** Never `run_in_background`.
- Windows 11, Git Bash, Visual Studio 17 2022 multi-config. `cl.exe` reachable
  via `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`
  (`vswhere.exe` is **not** on PATH here — call the batch file by full path).
- **ThreadSanitizer is unavailable** (MSVC has no TSan, no Linux build here).
  Do not attempt it. Task 9 says what substitutes.
- **After editing ANY file under `extern/pole/includes/`, restage it** before
  building slideio:

  ```bash
  cp -r extern/pole/includes/. build/extern/pole/include/pole/
  ```

  The root `CMakeLists.txt:231` copies that directory into
  `build/extern/pole/include/pole` with `file(COPY)` — a **configure-time**
  snapshot. Without the restage, slideio compiles against the stale header and
  there is no error, just baffling behaviour. (The command also copies
  `includes/CMakeLists.txt` into the staged tree; harmless, nothing globs
  there.) Editing pole `.cpp` files needs no restage — they are compiled from
  their source location.
- Test images resolve through `TestTools::getTestImagePath(subfolder, image)`
  against `SLIDEIO_IMAGES_PATH` (`d:\Projects\slideio\images\images`). Verified
  present: `zvi/Zeiss-1-Merged.zvi` (9.7 MB), `zvi/Zeiss-1-Stacked.zvi`
  (108 MB), `zvi/openslide/Zeiss-3-Mosaic.zvi` (2.0 GB),
  `zvi/mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi` (397 MB).
- `SLIDEIO_SKIP_MISSING_IMAGES=1` makes a missing image skip rather than fail.
  Every new test must be guarded by `SLIDEIO_SKIP_IF_IMAGE_MISSING`.

## File structure

| File | Responsibility after this plan |
|---|---|
| `extern/pole/includes/pole/detail/stream.hpp` | `_state` initialised; `read(pos, …)` declared `const` with an `hit_eof` out-param |
| `extern/pole/sources/pole/detail/stream.cpp` | positional read no longer writes `_state`; cursor read reproduces the old flag behaviour exactly |
| `extern/pole/includes/pole/detail/dirtree.hpp` | `find_siblings` takes a caller-owned `visited` buffer |
| `extern/pole/sources/pole/detail/dirtree.cpp` | `find_siblings` deduplicates in O(1); `children()` owns the buffer |
| `extern/pole/sources/pole/pole.cpp` | `Storage::stream()` computes `path()` only for relative names |
| `extern/pole/includes/pole/pole.h` | `Stream::read_at` |
| `extern/pole/includes/pole/detail/storage.hpp` | `PositionalFile`; `load*Block*` become `const`; `_pread`, `_stream_mutex` |
| `extern/pole/sources/pole/detail/storage.cpp` | `PositionalFile` impl; `loadBigBlocks` reads positionally, mutex fallback for the `iostream*` ctor |
| `extern/pole/includes/stream.hpp` | `basic_stream::read_at`, `basic_stream::size` |
| `extern/pole/includes/path.hpp` | `const` `stream_path::stream()` that does not bump `_ref_count` |
| `extern/pole/tests/test_storage.cpp` | new tests: `_state`, `read_at` equivalence/EOF/threads, `parent` equivalence |
| `src/slideio/drivers/zvi/zviutils.hpp` / `.cpp` | `StreamKeeper` gains a `const`-borrowing form |
| `src/slideio/drivers/zvi/zviimageitem.cpp` | `readRaster` uses `size()` + one `read_at`; no cursor |
| `src/slideio/drivers/zvi/zviscene.hpp` | `supportsConcurrentReads()` override |
| `src/tests/main/test_concurrency_contract.cpp` | `zviIsStillSerialised` replaced by `zviReportsConcurrentReads` |
| `src/tests/main/test_zvi_driver.cpp` | three byte-exactness tests |
| `software-docs/TECH_DEBT.md` | §14 closed; §4 list updated; §15 cross-ref; new entries for what is deliberately left |
| `software-docs/BREAKING_CHANGES.md` | ZVI moved into the concurrent list; pole's API change recorded |
| `CLAUDE.md` | ZVI added to the concurrent-formats list; pole paragraph notes the positional path |

---

## Task 1: Make pole's own test suite runnable

Nothing in this plan can be test-driven until pole's tests build. They do not
today, for two reasons that both need fixing before any code changes. This task
writes no product code and is pure setup — it is its own task because it is the
one thing that blocks every other task, and because what it discovers (Step 4)
changes how Task 3 is written.

**Files:** none modified in either repo. Creates a throwaway build directory.

**Interfaces:** none.

- [ ] **Step 1: Confirm the two blockers are real**

```bash
ls extern/pole/googletest/ | wc -l
grep -n "PACKAGE_TESTS" CMakeLists.txt
```

Expected: `0` (the nested submodule is unpopulated), and
`CMakeLists.txt:204` forcing `PACKAGE_TESTS OFF` for jpegxrcodec and pole
together. That `OFF` is correct and must stay — the slideio build has no
business building pole's tests. pole's tests get their own build tree instead.

- [ ] **Step 2: Initialise pole's nested googletest submodule**

```bash
git -C extern/pole submodule update --init
ls extern/pole/googletest/CMakeLists.txt
```

Expected: the file exists. This is a *nested* submodule; the
`git submodule update --init` that `CLAUDE.md` prescribes for a fresh clone
does not fetch it, which is why it is empty. Nothing in slideio's build needs
it, so this stays a developer-local step and no slideio file changes.

- [ ] **Step 3: Configure and run pole's tests standalone**

```bash
cmake -S extern/pole -B build-pole -G "Visual Studio 17 2022" -A x64 -DPACKAGE_TESTS=ON
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe
```

Expected: 5 tests, all pass — `compound_document.find_storage`,
`storage.find_stream`, `storage.read_stream_int`, `storage.read_stream_double`,
`storage.read_stream_string`.

If the executable lands somewhere else, find it with
`find build-pole -name 'storage_tests.exe'` and use that path consistently for
the rest of the plan. `build-pole/` is a scratch directory: add it to your local
ignore, do not commit it, and do not add it to `.gitignore` in either repo.

- [ ] **Step 4: Record what is NOT covered — this shapes Task 3**

```bash
grep -c "delete_entry" extern/pole/tests/test_storage.cpp
grep -n "^TEST(" extern/pole/tests/test_storage.cpp
```

Expected: `0`, and the five read-only tests above.

**This is the finding that matters:** pole's write and mutation paths —
`delete_entry`, `DirTree::save`, `StreamImpl::write`, `StorageIO::saveBlock` —
have no coverage whatsoever. It is the main reason Task 3 takes the shape it
does: an earlier draft cached each entry's parent index, which would have
needed invalidating across exactly these uncovered paths. It does not any
more. **Do not "improve" any mutation path anywhere in this plan**, and if a
change you are considering would require one to stay correct, that is a signal
to find a different change.

Note also, for Task 3's test: `compound_document::path_exist()` is broken for
nested stream paths. It slices the parent path with
`substr(0, size - ++pos)` and lands mid-name, so it returns `false` for all
fifteen nested streams of pole's own `test1.bin` fixture. Do not build any
assertion on it. Task 10 files it.

- [ ] **Step 5: Report, do not commit**

Nothing to commit. Report the 5/5 pass, the `storage_tests.exe` path you will
use, and confirmation that mutation paths are uncovered.

---

## Task 2: pole — initialise `_state`, and stop the positional read from writing it

Spec §5.1. A UB fix that is also the prerequisite for Task 5: a `const`
positional read is the thing Task 5 exposes.

**Files:**
- Modify: `extern/pole/includes/pole/detail/stream.hpp`
- Modify: `extern/pole/sources/pole/detail/stream.cpp`
- Test: `extern/pole/tests/test_storage.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `std::streamsize StreamImpl::read(size_t pos, unsigned char* data, std::streamsize maxlen, bool* hit_eof = 0) const`
  - `bool StreamImpl::eof() const` / `fail() const` now well-defined on a
    freshly opened stream (both `false`)

- [ ] **Step 1: Write the failing test**

Append to `extern/pole/tests/test_storage.cpp`:

```cpp
TEST(stream, state_is_clean_on_open)
{
	std::string file_path = getTestFilePath("test1.bin");
	ole::compound_document doc(file_path);
	ASSERT_TRUE(doc.good());
	auto image_storage = doc.find_storage("/Image");
	ASSERT_TRUE(image_storage != doc.end());
	auto content_stream = image_storage->find_stream("/Image/Contents");
	ASSERT_TRUE(content_stream != image_storage->end());
	ole::basic_stream& stream = content_stream->stream();
	// A freshly opened stream has read nothing and failed at nothing.
	// StreamImpl::_state used to be left uninitialised by init(), so both
	// of these read indeterminate memory.
	EXPECT_FALSE(stream.eof());
	EXPECT_FALSE(stream.fail());
}
```

- [ ] **Step 2: Run it and expect it to be unreliable, not merely red**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe --gtest_filter="stream.state_is_clean_on_open"
```

Expected: **either** outcome is consistent with the bug — indeterminate memory
is frequently zero. Record what you actually saw. This is the one test in the
plan whose pre-fix red is not guaranteed; its value is as a regression guard
after Step 3, and as documentation of the invariant.

- [ ] **Step 3: Initialise `_state` in both constructors**

In `extern/pole/includes/pole/detail/stream.hpp`, give the member a default so
no constructor can leave it indeterminate:

```cpp
	int _state = 0;
```

In `extern/pole/sources/pole/detail/stream.cpp`, `StreamImpl::init()` — add the
assignment alongside the other resets, so the intent is visible where the rest
of the state is set up:

```cpp
void StreamImpl::init()
{
  _pos = 0;
  _state = 0;
```

and in the copy constructor, which copies `_io`, `_entry`, `_blocks`, `_pos` and
the cache but never `_state`:

```cpp
	_state = stream._state;
```

- [ ] **Step 4: Split the positional read from the cursor read**

Declare in `stream.hpp` — note the `const` and the out-param:

```cpp
	// Positional read: takes its offset as an argument, touches no cursor and
	// no flags, and is therefore safe to call from several threads at once on
	// one StreamImpl. `hit_eof`, when given, reports that the request ran past
	// the end of the stream and was clamped -- which is the only thing the
	// cursor-based overload needs in order to keep setting Eof exactly as it
	// used to.
	std::streamsize read( size_t pos, unsigned char* data, std::streamsize maxlen,
	                      bool* hit_eof = 0 ) const;
	std::streamsize read( unsigned char* data, std::streamsize maxlen );
```

In `stream.cpp`, change the positional definition's signature to match, add
`const`, and replace its opening clamp — the two lines that wrote `_state` —
with a report through the out-param:

```cpp
std::streamsize StreamImpl::read( size_t pos, unsigned char* data,
                                  std::streamsize maxlen, bool* hit_eof ) const
{
  if (hit_eof)
	  *hit_eof = false;
  // sanity checks
  if (!_entry)
	  return 0;
  if( !data )
	  return 0;
  if( maxlen == 0 )
	  return 0;
  if ((maxlen + pos) > _entry->size())
  {
	  maxlen = _entry->size() - pos;
	  if (hit_eof)
		  *hit_eof = true;
  }
```

The rest of the function body is unchanged.

Then the cursor overload carries the flag work that moved out of it:

```cpp
std::streamsize StreamImpl::read( unsigned char* data, std::streamsize maxlen )
{
  bool hit_eof = false;
  std::streamsize bytes = read( (size_t)tell(), data, maxlen, &hit_eof );
  _pos += bytes;

  if (hit_eof)
	  _state |= StreamImpl::Eof;
  else
	  _state &= StreamImpl::Eof;

  if (_pos == _entry->size())
	  _state |= StreamImpl::Eof;

  return bytes;
}
```

**Do not "fix" `_state &= StreamImpl::Eof`.** It keeps the Eof bit and clears
Bad, where the evident intent was `&= ~Eof`. That is a pre-existing bug, it is
carried across verbatim on purpose so this task changes no observable
behaviour, and nothing in slideio consults either flag. Task 10 files it.

`update_cache()` and `getch()` are untouched: both are cursor-based by
definition and only reachable from the cursor API.

- [ ] **Step 5: Run pole's tests**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe
```

Expected: 6/6 pass.

- [ ] **Step 6: Confirm slideio still builds and ZVI still reads**

```bash
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: green. The restage in the first line is mandatory — `stream.hpp`
changed. Skipping it compiles slideio against the pre-change header, silently.

- [ ] **Step 7: Commit in pole, then the pointer in slideio**

```bash
git -C extern/pole add includes/pole/detail/stream.hpp sources/pole/detail/stream.cpp tests/test_storage.cpp
git -C extern/pole commit -m "initialise StreamImpl::_state and make the positional read const

_state was assigned only on the two Bad paths, so fail() and eof() read
indeterminate memory for every successfully opened stream. The positional
read(pos, ...) overload now reports a clamped request through an out-param
instead of writing _state, which makes it const and re-entrant; the
cursor overload reproduces the previous flag behaviour verbatim."
git add extern/pole
git commit -m "bump pole: initialised stream state, const positional read"
```

---

## Task 3: pole — stop the directory walk being quadratic

Spec §5.2. This is the 1721 ms. It is independent of concurrency and worth
landing on its own merits: it speeds up every ZVI open in the library,
single-threaded, and if the rest of this plan were abandoned it is the piece
worth keeping.

Two changes, about fifteen lines together. Both were measured on a patched copy
of pole before this plan was written, so the numbers below are what you should
actually see, not estimates.

**Files:**
- Modify: `extern/pole/sources/pole/pole.cpp`
- Modify: `extern/pole/includes/pole/detail/dirtree.hpp`
- Modify: `extern/pole/sources/pole/detail/dirtree.cpp`
- Test: `extern/pole/tests/test_storage.cpp`

**Interfaces:**
- Consumes: `StreamImpl` unchanged from Task 2.
- Produces:
  - `void DirTree::find_siblings(std::vector<size_t>& result, ULONG32 index, std::vector<char>& visited) const`
    — private; the two-argument form is gone
  - No public API change, no new class, no new data member on `DirEntry`

- [ ] **Step 1: Write the characterization test**

The change must be *behaviour-preserving*, so the test pins down what the
document reports about itself and Step 6 re-runs it. Append to
`extern/pole/tests/test_storage.cpp`:

```cpp
// Every storage and stream the document enumerates must still be findable by
// the path string the document itself reports -- which is what exercises the
// DirTree path walk that Task 3 makes non-quadratic. Asserting particular
// paths would test the fixture; this asserts the round-trip that makes the
// optimisation safe.
//
// Deliberately does NOT use compound_document::path_exist(): it returns false
// for every nested stream path in this very fixture, because it slices the
// parent path with substr(0, size - ++pos) and lands mid-name. That is a
// pre-existing defect, filed separately, and not something to depend on here.
TEST(dirtree, every_reported_path_resolves)
{
	std::string file_path = getTestFilePath("test1.bin");
	ole::compound_document doc(file_path);
	ASSERT_TRUE(doc.good());

	int storages = 0, streams = 0;
	for (auto it = doc.begin(); it != doc.end(); ++it)
	{
		++storages;
		const std::string storagePath = it->string();
		ASSERT_TRUE(doc.find_storage(storagePath) != doc.end())
			<< "storage does not resolve: " << storagePath;
		for (auto s = it->begin(); s != it->end(); ++s)
		{
			++streams;
			auto owner = doc.find_storage(storagePath);
			ASSERT_TRUE(owner != doc.end());
			EXPECT_TRUE(owner->find_stream(s->string()) != owner->end())
				<< "stream does not resolve: " << s->string();
		}
	}
	EXPECT_EQ(storages, 16);
	EXPECT_EQ(streams, 19);
}
```

- [ ] **Step 2: Run it and confirm it passes BEFORE the change**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe --gtest_filter="dirtree.every_reported_path_resolves"
```

Expected: PASS, with 16 storages and 19 streams. This is the plan's one
deliberate exception to red-first: the test characterises *existing* behaviour
so that Steps 4 and 5 cannot change it. A test that only went green after the
change would not constrain the change at all.

If the counts differ from 16/19, the fixture has changed since this plan was
written; use whatever it reports and say so, but do not delete the assertion —
it is what catches an entry going missing.

- [ ] **Step 3: Commit the test on its own**

```bash
git -C extern/pole add tests/test_storage.cpp
git -C extern/pole commit -m "pin down that every reported storage and stream path resolves"
```

- [ ] **Step 4: Stop computing a path that is thrown away**

`Storage::stream()` (`extern/pole/sources/pole/pole.cpp:78`) calls
`path(path_)` unconditionally, but `path_` is only read in the branch below it.
`compound_document::init()` passes only absolute paths, so on the path that
matters this builds an expensive string and discards it. Move it inside the
branch:

```cpp
  // make absolute if necesary
  std::string fullName = name;
  if( name[0] != '/' )
  {
    std::string path_;
    path(path_);
    fullName.insert( 0, path_ + "/" );
  }
```

`path()` reaches `DirTree::fullName()` → `DirTree::parent()`, which is brute
force by its own comment — it calls `children()` on every entry in the tree.
This one guard is worth **546 ms** of the mosaic's 1721 ms.

Leave `fullName` itself alone. It is dead — nothing after the `insert` reads it,
`io->entry(name)` and the reuse comparison both use `name` — but deleting it is
a separate tidy-up and this change wants to stay small. Note it in the commit
message.

- [ ] **Step 5: Give `find_siblings` an O(1) duplicate check**

`find_siblings` (`extern/pole/sources/pole/detail/dirtree.cpp:347`) runs three
linear scans of `result` per node, making it O(k²) in the sibling count k. The
mosaic has ~514 entries under `/Image` and resolves paths ~3000 times during
`init`. Worth a further **1037 ms**.

In `extern/pole/includes/pole/detail/dirtree.hpp`, change the private
declaration:

```cpp
	void find_siblings( std::vector<size_t>& result, ULONG32 index, std::vector<char>& visited ) const;
```

In `dirtree.cpp`, `children()` — its only non-recursive caller — owns the
buffer:

```cpp
void DirTree::children( size_t index, std::vector<size_t>& result ) const
{ 
  const DirEntry* e = entry( index );
  if( e && ( e->valid() && e->child() < entryCount() ) )
  {
    std::vector<char> visited( entryCount(), 0 );
    find_siblings( result, (ULONG32)e->child(), visited );
  }
}
```

and the function itself:

```cpp
void DirTree::find_siblings( std::vector<size_t>& result, ULONG32 index,
                             std::vector<char>& visited ) const
{
  const DirEntry* e = entry( index );
  if( !e ) return;
  if( !e->valid() ) return;

  // Prevent infinite loop. O(1) against the caller's visited buffer rather
  // than a linear scan of result per node: the three scans this replaces made
  // find_siblings O(k^2) in the number of siblings, and /Image of a mosaic ZVI
  // has ~514 of them. See section 3.3 of
  // software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md in the
  // slideio repository.
  if( index >= visited.size() ) return;
  if( visited[index] ) return;
  visited[index] = 1;

  // add myself
  result.push_back( index );

  // visit previous sibling, don't go infinitely
  ULONG32 prev = e->prev();
  if( ( prev > 0 ) && ( prev < entryCount() ) )
  {
    if( prev >= visited.size() || !visited[prev] ) find_siblings( result, prev, visited );
  }

  // visit next sibling, don't go infinitely
  ULONG32 next = e->next();
  if( ( next > 0 ) && ( next < entryCount() ) )
  {
    if( next >= visited.size() || !visited[next] ) find_siblings( result, next, visited );
  }
}
```

**Take the parameter, not a `mutable` member.** A `mutable std::vector<char>`
on `DirTree`, reset at the top of `children()`, measures 112 ms against this
version's 138 ms on the mosaic — but it makes the `const` method `children()`
unsafe to call from two threads, which is a bad property to introduce in a
change whose whole purpose is concurrency. 26 ms does not buy that back.

Before moving on, check no other caller passes a non-empty `result` to
`children()`, since the old dedup scanned `result` and the new one does not:

```bash
grep -n "children(" extern/pole/sources/pole/detail/dirtree.cpp extern/pole/includes/pole/detail/dirtree.hpp
```

Expected: every call site declares a fresh vector immediately above it. If one
does not, clear it at the call site and say so.

- [ ] **Step 6: Verify equivalence**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe
```

Expected: 7/7 pass, `dirtree.every_reported_path_resolves` still green with
16 storages and 19 streams — that is the point of the task.

- [ ] **Step 7: Measure**

Build the probe from spec §3.2. Write `poleprobe.cpp` to your scratchpad:

```cpp
#include <pole/polepp.hpp>
#include <chrono>
#include <cstdio>
int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: poleprobe <file.zvi>\n"); return 1; }
    auto t0 = std::chrono::steady_clock::now();
    ole::compound_document doc(argv[1]);
    auto t1 = std::chrono::steady_clock::now();
    if (!doc.good()) { std::printf("open failed\n"); return 2; }
    int storages = 0, streams = 0;
    for (auto it = doc.begin(); it != doc.end(); ++it) {
        ++storages;
        for (auto s = it->begin(); s != it->end(); ++s) ++streams;
    }
    std::printf("construct %.0f ms | storages %d | streams %d\n",
        std::chrono::duration<double, std::milli>(t1 - t0).count(), storages, streams);
    return 0;
}
```

A `.bat` is needed because `vcvars64.bat` must run in the same shell as `cl`,
and `vswhere.exe` is not on PATH on this machine. Write this to your scratchpad
as `pb.bat`:

```
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /std:c++17 /EHsc /O2 /MD /I "D:\Projects\slideio\slideio\build\extern\pole\include" poleprobe.cpp /Fe:poleprobe.exe /link "D:\Projects\slideio\slideio\build\extern\pole\lib\Release\pole.lib"
```

Then:

```bash
SP="<your scratchpad dir>"
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target pole -- -m
cmd //c "$(cygpath -w "$SP/pb.bat")"
for f in "zvi/TOMMAlexaFluor647.zvi" "zvi/Zeiss-1-Merged.zvi" "zvi/Zeiss-1-Stacked.zvi" "zvi/mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi" "zvi/openslide/Zeiss-3-Mosaic.zvi"; do
  printf "%-64s " "$f"; "$SP/poleprobe.exe" "d:/Projects/slideio/images/images/$f"
done
```

Run the loop twice and use the second run's numbers — the first is polluted by
the file cache. Expected, against the pre-change column:

| File | Streams | Before | Expected after |
|---|---|---|---|
| `TOMMAlexaFluor647.zvi` | 10 | 0 ms | 0 ms |
| `Zeiss-1-Merged.zvi` | 19 | 1 ms | 1 ms |
| `Zeiss-1-Stacked.zvi` | 105 | 3 ms | 2 ms |
| `20140505_mouse_…_005.zvi` | 315 | 26 ms | ~9 ms |
| `Zeiss-3-Mosaic.zvi` | 1543 | 1721 ms | ~138 ms |

If the mosaic is still in the hundreds of ms, one of the two changes is not in
effect — check that the `path(path_)` call really is inside the branch, and that
`find_siblings` is testing `visited` rather than scanning `result`. **Do not
assert these timings in a test**; a wall-clock assertion flakes in CI. Report
them.

- [ ] **Step 8: Confirm slideio, then commit twice**

```bash
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: green, and `openSlideMosaic` visibly faster — it was 6690 ms, of which
~1.6 s was this.

```bash
git -C extern/pole add includes/pole/detail/dirtree.hpp sources/pole/detail/dirtree.cpp sources/pole/pole.cpp
git -C extern/pole commit -m "resolve directory paths in linear time"
git add extern/pole
git commit -m "bump pole: linear-time directory path resolution"
```

Use a fuller body on the pole commit: the two causes (unconditional `path()`
reaching the brute-force `parent()`; `find_siblings` deduplicating with three
linear scans per node), the numbers (1721 ms → 138 ms on the mosaic, 26 ms →
9 ms at 315 streams), that every reported path still resolves, and that the
dead `fullName` local and the 2 ms reuse scan were left alone on purpose.

---

## Task 4: pole — positional file I/O under the block loaders

Spec §5.3, bottom half. This removes the binding constraint: the single
`std::fstream` cursor that two threads collide on even when reading different
streams.

**Files:**
- Modify: `extern/pole/includes/pole/detail/storage.hpp`
- Modify: `extern/pole/sources/pole/detail/storage.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `class POLE::PositionalFile` with `bool good() const`,
    `ULONG32 read_at(ULONG32 offset, unsigned char* dst, ULONG32 n) const`
  - `StorageIO::loadBigBlock(s)` and `loadSmallBlock(s)` become `const`

- [ ] **Step 1: Note the pre-existing open mode, and leave it alone**

```bash
grep -n "std::ios::in | std::ios::out" extern/pole/sources/pole/detail/storage.cpp
```

Expected: two hits, in the `const char*` and `const wchar_t*` constructors.
pole opens every compound document **read/write**, which means slideio cannot
open a read-only ZVI at all today. That is a real defect, it is **not** fixed
here, and Task 10 files it: fixing it changes when opens succeed, which is a
behaviour change deserving its own commit and its own test.

The consequence for this task: the `std::fstream` stays exactly as it is, and
the positional handle is opened **in addition**, read-only. One extra descriptor
per open document — one, not one per thread — and open semantics, `_result`
values and the write path are all untouched.

- [ ] **Step 2: Declare the positional reader**

In `extern/pole/includes/pole/detail/storage.hpp`, above `class StorageIO`:

```cpp
// A read-only file handle whose reads carry their own offset, so any number of
// threads may read one file through one descriptor with no shared cursor.
//
// This duplicates slideio::FileReader (src/slideio/core/tools/filereader.hpp in
// the slideio repository) on purpose: pole is vendored as a submodule precisely
// because it depends on nothing but the standard library, so it cannot use it.
// Keep the two in step -- in particular the short-read retry loop, which exists
// because pread is permitted to return fewer bytes than requested.
class PositionalFile
{
public:
	PositionalFile( const char* filename );
#if defined(WIN32)
	PositionalFile( const wchar_t* filename );
#endif
	~PositionalFile();

	bool good() const;
	// Reads up to n bytes from offset. Returns bytes actually read.
	ULONG32 read_at( ULONG32 offset, unsigned char* dst, ULONG32 n ) const;

private:
	void open_handle( const void* name, bool wide );
#if defined(WIN32)
	void* _handle;
#else
	int _fd;
#endif

	PositionalFile( const PositionalFile& );
	PositionalFile& operator=( const PositionalFile& );
};
```

In `StorageIO`, mark the loaders `const`, and add the two members:

```cpp
	ULONG32 loadSmallBlock(ULONG32 block, unsigned char* buffer, ULONG32 maxlen) const;
    ULONG32 loadBigBlock(ULONG32 block, unsigned char* buffer, ULONG32 maxlen) const;
```

```cpp
	ULONG32 loadSmallBlocks( const std::vector<ULONG32>& blocks, unsigned char* buffer, ULONG32 maxlen ) const;
	ULONG32 loadBigBlocks( const std::vector<ULONG32>& blocks, unsigned char* buffer, ULONG32 maxlen ) const;
```

```cpp
	PositionalFile* _pread;        // read path; NULL for the iostream* ctor
	mutable std::mutex _stream_mutex; // guards _stream when _pread is NULL
```

and `#include <mutex>` at the top of that header.

- [ ] **Step 3: Implement it**

In `extern/pole/sources/pole/detail/storage.cpp`, above `StorageIO`:

```cpp
#if defined(WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

PositionalFile::PositionalFile( const char* filename )
{
#if defined(WIN32)
	_handle = CreateFileA( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#else
	_fd = ::open( filename, O_RDONLY );
#endif
}

#if defined(WIN32)
PositionalFile::PositionalFile( const wchar_t* filename )
{
	_handle = CreateFileW( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
}
#endif

PositionalFile::~PositionalFile()
{
#if defined(WIN32)
	if( _handle != INVALID_HANDLE_VALUE ) CloseHandle( _handle );
#else
	if( _fd >= 0 ) ::close( _fd );
#endif
}

bool PositionalFile::good() const
{
#if defined(WIN32)
	return _handle != INVALID_HANDLE_VALUE;
#else
	return _fd >= 0;
#endif
}

ULONG32 PositionalFile::read_at( ULONG32 offset, unsigned char* dst, ULONG32 n ) const
{
	if( !good() || !dst ) return 0;

	ULONG32 done = 0;
	while( done < n )
	{
#if defined(WIN32)
		OVERLAPPED ov;
		memset( &ov, 0, sizeof(ov) );
		ov.Offset = (DWORD)((offset + done) & 0xffffffffu);
		ov.OffsetHigh = 0;
		DWORD got = 0;
		if( !ReadFile( _handle, dst + done, (DWORD)(n - done), &got, &ov ) )
		{
			// A read that reaches end-of-file through OVERLAPPED reports
			// ERROR_HANDLE_EOF rather than a zero-byte success.
			if( GetLastError() == ERROR_HANDLE_EOF ) break;
			break;
		}
		if( got == 0 ) break;
		done += got;
#else
		ssize_t got = ::pread( _fd, dst + done, (size_t)(n - done), (off_t)(offset + done) );
		if( got < 0 )
		{
			if( errno == EINTR ) continue;
			break;
		}
		if( got == 0 ) break;
		done += (ULONG32)got;
#endif
	}
	return done;
}
```

The loop is the point: a short read is retried rather than treated as the whole
answer, and `EINTR` is retried rather than reported as failure.

- [ ] **Step 4: Open it alongside the fstream**

Set `_pread = NULL` in `StorageIO::init()`, delete it in `close()`, and in each
of the two filename constructors add the positional open after the fstream one
succeeds — the `const char*` version:

```cpp
	_file = file;
	_stream = file;
	_pread = new PositionalFile( filename );
	if( !_pread->good() ) { delete _pread; _pread = NULL; }
	load();
```

and the same three lines in the `const wchar_t*` version. A failed positional
open falls back to the mutex path rather than failing the whole open, so this
cannot make a file that opens today stop opening.

- [ ] **Step 5: Read positionally in `loadBigBlocks`**

Replace the seek-and-read pair in `loadBigBlocks` — everything else in the
function, including the `pos + p > _size` clamp, stays:

```cpp
    if( pos + p > _size ) 
		p = _size - pos;
    if( _pread )
    {
      bytes += _pread->read_at( pos, data + bytes, p );
    }
    else
    {
      std::lock_guard<std::mutex> lock( _stream_mutex );
      _stream->seekg( pos );
      _stream->read( (char*)data + bytes, p );
      bytes += p;
    }
```

and change the four loader definitions to `const` to match Step 2. The
`if( !_stream->good() ) return 0;` sentinels at the top of `loadBigBlocks` and
`loadSmallBlocks` must become `if( !_pread && (!_stream || !_stream->good()) ) return 0;`
— otherwise a document opened positionally is rejected on the state of an
fstream nothing is reading.

`loadSmallBlocks` needs no change of its own: it reads through `loadBigBlock`.

`saveBlock()` and `flush()` keep `_file` and `seekp`, untouched.

- [ ] **Step 6: Run both suites**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: 7/7 in pole, green in ZVI. The existing pole read tests are the
evidence here — they read ints, doubles and strings out of a real compound
document, which now travels through `read_at`.

- [ ] **Step 7: Commit twice**

```bash
git -C extern/pole add includes/pole/detail/storage.hpp sources/pole/detail/storage.cpp
git -C extern/pole commit -m "read blocks positionally instead of through a shared cursor

loadBigBlocks did seekg+read on one std::fstream, so two threads reading
different streams of one document collided on the file cursor. A read-only
PositionalFile (ReadFile with an OVERLAPPED offset, or pread) is opened
alongside the existing fstream and carries the read path; the block loaders
become const. The iostream* constructor has no positional equivalent and
falls back to the previous code under a mutex. The write path is untouched."
git add extern/pole
git commit -m "bump pole: positional block reads"
```

---

## Task 5: pole — expose a cursor-free read to callers

Spec §5.3, top half. Task 4 made the bottom of the stack re-entrant; this makes
it reachable without touching a cursor, and removes the last race
(`_ref_count`).

**Files:**
- Modify: `extern/pole/includes/pole/pole.h`
- Modify: `extern/pole/sources/pole/pole.cpp`
- Modify: `extern/pole/includes/stream.hpp`
- Modify: `extern/pole/includes/path.hpp`
- Test: `extern/pole/tests/test_storage.cpp`

**Interfaces:**
- Consumes: `StreamImpl::read(pos, data, maxlen, hit_eof) const` from Task 2;
  `PositionalFile` from Task 4.
- Produces:
  - `unsigned long POLE::Stream::read_at(unsigned long offset, unsigned char* data, unsigned long maxlen) const`
  - `std::streamsize ole::basic_stream::read_at(std::streamoff offset, char* buf, std::streamsize n) const`
  - `std::streamoff ole::basic_stream::size() const`
  - `const ole::basic_stream& ole::stream_path::stream() const`

- [ ] **Step 1: Write the failing tests**

Append to `extern/pole/tests/test_storage.cpp`:

```cpp
#include <thread>
#include <vector>

// read_at must return exactly what a cursor read returns, and must leave the
// cursor where it found it -- that is what lets one document serve several
// threads without each needing its own copy.
TEST(stream, read_at_matches_cursor_read_and_does_not_move_it)
{
	std::string file_path = getTestFilePath("test1.bin");
	ole::compound_document doc(file_path);
	ASSERT_TRUE(doc.good());
	auto storage = doc.find_storage("/Image");
	ASSERT_TRUE(storage != doc.end());
	auto sp = storage->find_stream("/Image/Contents");
	ASSERT_TRUE(sp != storage->end());
	ole::basic_stream& stream = sp->stream();

	const std::streamoff size = stream.size();
	ASSERT_GT(size, 16);

	std::vector<char> viaCursor(16), viaPositional(16);
	stream.seek(8, std::ios::beg);
	ASSERT_EQ(stream.read(viaCursor.data(), 16), 16);

	stream.seek(0, std::ios::beg);
	ASSERT_EQ(stream.read_at(8, viaPositional.data(), 16), 16);
	EXPECT_EQ(stream.pos(), 0) << "read_at moved the cursor";
	EXPECT_EQ(viaCursor, viaPositional);
}

TEST(stream, read_at_past_end_is_clamped)
{
	std::string file_path = getTestFilePath("test1.bin");
	ole::compound_document doc(file_path);
	ASSERT_TRUE(doc.good());
	auto storage = doc.find_storage("/Image");
	ASSERT_TRUE(storage != doc.end());
	auto sp = storage->find_stream("/Image/Contents");
	ASSERT_TRUE(sp != storage->end());
	ole::basic_stream& stream = sp->stream();

	const std::streamoff size = stream.size();
	std::vector<char> buf(32);
	// Straddling the end returns only what exists, and reports it by count --
	// the destination is otherwise left untouched, so callers must check.
	const std::streamsize got = stream.read_at(size - 4, buf.data(), 32);
	EXPECT_EQ(got, 4);
	EXPECT_EQ(stream.read_at(size, buf.data(), 32), 0);
}

// The race this whole exercise is about: many threads reading one document.
TEST(stream, concurrent_read_at_on_one_document)
{
	std::string file_path = getTestFilePath("test1.bin");
	ole::compound_document doc(file_path);
	ASSERT_TRUE(doc.good());
	auto storage = doc.find_storage("/Image");
	ASSERT_TRUE(storage != doc.end());
	auto sp = storage->find_stream("/Image/Contents");
	ASSERT_TRUE(sp != storage->end());
	const ole::basic_stream& stream = sp->stream();

	const std::streamoff size = stream.size();
	const std::streamsize chunk = (size < 64) ? size : 64;
	ASSERT_GT(chunk, 0);

	std::vector<char> expected((size_t)chunk);
	ASSERT_EQ(stream.read_at(0, expected.data(), chunk), chunk);

	std::vector<std::thread> threads;
	std::vector<int> mismatches(8, 0);
	for (int t = 0; t < 8; ++t)
	{
		threads.emplace_back([&, t]() {
			std::vector<char> got((size_t)chunk);
			for (int i = 0; i < 200; ++i)
			{
				if (stream.read_at(0, got.data(), chunk) != chunk || got != expected)
					++mismatches[t];
			}
		});
	}
	for (auto& th : threads) th.join();
	for (int t = 0; t < 8; ++t)
		EXPECT_EQ(mismatches[t], 0) << "thread " << t << " read torn data";
}
```

- [ ] **Step 2: Run them and watch them fail to compile**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
```

Expected: FAIL — `read_at` and `size` are not members of `ole::basic_stream`,
and `stream()` on a `const` iterator does not compile.

- [ ] **Step 3: Add `read_at` to `POLE::Stream`**

In `extern/pole/includes/pole/pole.h`, in `Stream`'s public operations:

```cpp
  // Reads a block of data from an explicit offset. Touches no cursor and no
  // flags, so several threads may call it on one Stream at once.
  unsigned long read_at( unsigned long offset, unsigned char* data, unsigned long maxlen ) const;
```

In `extern/pole/sources/pole/pole.cpp`, beside the existing `Stream::read`:

```cpp
unsigned long Stream::read_at( unsigned long offset, unsigned char* data, unsigned long maxlen ) const
{
  return impl ? (unsigned long)impl->read( (size_t)offset, data, (std::streamsize)maxlen ) : 0;
}
```

`impl` is a `StreamImpl*`; the positional overload has been `const` since
Task 2, so no cast is needed and none should be added.

- [ ] **Step 4: Add `read_at` and `size` to `ole::basic_stream`**

In `extern/pole/includes/stream.hpp`, in `basic_stream`'s operations:

```cpp
		// Positional read: carries its own offset, leaves the cursor and the
		// flags alone, and is safe from several threads at once. Returns the
		// number of bytes actually read -- a short return is the ONLY report of
		// a truncated read, since the destination is left untouched.
		std::streamsize read_at( std::streamoff offset, char* buf, std::streamsize n ) const
		{
			return _stream ? (std::streamsize)_stream->read_at( (unsigned long)offset,
			                     (unsigned char*)buf, (unsigned long)n ) : 0;
		}
		// Size in bytes. Reads the directory entry, not the cursor, so callers
		// no longer need seek(0, std::ios::end) to learn a length.
		std::streamoff size() const
		{
			return _stream ? (std::streamoff)_stream->size() : 0;
		}
```

`POLE::Stream::size()` is already `const`, so this needs no other change.

- [ ] **Step 5: Add the `const` stream borrow**

In `extern/pole/includes/path.hpp`, in `stream_path`, beside the existing
`stream()`:

```cpp
		// Borrow the stream without claiming it. The non-const overload bumps
		// _ref_count, which only feeds used() and only entry_can_be_deleted()
		// consults -- but that increment is a write, and a write on a shared
		// object is a data race for concurrent readers. Read-only callers take
		// this one.
		const ole::basic_stream& stream() const { return _stream; }
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build-pole --config Release --target storage_tests -- -m
./build-pole/Release/storage_tests.exe
```

Expected: 10/10 pass. If `concurrent_read_at_on_one_document` reports torn
data, the positional path is still touching shared state — check that
`StreamImpl::read(pos, ...)` is genuinely `const` and that Task 4's
`loadBigBlocks` is not falling into the mutex branch *and* mutating `bytes`
inconsistently.

- [ ] **Step 7: Confirm slideio, then commit twice**

```bash
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: green — nothing in slideio uses the new API yet.

```bash
git -C extern/pole add includes/pole/pole.h sources/pole/pole.cpp includes/stream.hpp includes/path.hpp tests/test_storage.cpp
git -C extern/pole commit -m "expose a cursor-free read and a const stream borrow

basic_stream::read_at and size() let a caller read a stream without touching
_pos, the flags or the getch cache, and stream_path::stream() const borrows
without bumping _ref_count. Together with the positional block loaders this
makes one compound_document safe to read from several threads at once."
git add extern/pole
git commit -m "bump pole: cursor-free read API"
```

---

## Task 6: ZVI — read positionally, contract still `false`

Spec §5.4, minus the flip. Landing the read-path change with the contract
unchanged means the existing suite is the evidence that it reads the same bytes,
before concurrency is added as a second variable.

**Files:**
- Modify: `src/slideio/drivers/zvi/zviutils.hpp`
- Modify: `src/slideio/drivers/zvi/zviutils.cpp`
- Modify: `src/slideio/drivers/zvi/zviimageitem.cpp`

**Interfaces:**
- Consumes: `ole::basic_stream::read_at`, `::size`,
  `ole::stream_path::stream() const` from Task 5.
- Produces:
  - `class ZVIUtils::ConstStreamKeeper` with
    `operator const ole::basic_stream&()` and
    `const ole::basic_stream* operator->() const`

- [ ] **Step 1: Add a `const`-borrowing keeper**

In `src/slideio/drivers/zvi/zviutils.hpp`, beside `StreamKeeper`:

```cpp
        // StreamKeeper's read-only sibling. It borrows the stream through
        // stream_path::stream() const, which does not bump _ref_count -- so
        // two threads resolving the same path do not race. Use this on the read
        // path; StreamKeeper stays for the init-time parsers, which walk a
        // stream sequentially with the cursor API.
        class SLIDEIO_ZVI_EXPORTS ConstStreamKeeper
        {
        public:
            ConstStreamKeeper(ole::compound_document& doc, const std::string& path);
            operator const ole::basic_stream& () const {
                return m_StreamPos->stream();
            }
            const ole::basic_stream* operator ->() const {
                return &(m_StreamPos->stream());
            }
        private:
            std::vector<ole::stream_path>::const_iterator m_StreamPos;
        };
```

In `src/slideio/drivers/zvi/zviutils.cpp`, immediately after
`StreamKeeper::StreamKeeper`, add the constructor. It resolves the path the same
way — `find_storage` then `find_stream`, both non-mutating scans — and differs
only in the iterator's constness:

```cpp
ZVIUtils::ConstStreamKeeper::ConstStreamKeeper(ole::compound_document& doc, const std::string& path)
{
    const size_t pos = path.find_last_of('/');
    std::string storagePath = path.substr(0, pos);
    auto storagePos = doc.find_storage(storagePath);

    if(storagePos==0)
    {
        storagePath = "/";
    }

    if(storagePos == doc.end())
    {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid storage path: " << storagePath;
    }

    m_StreamPos = storagePos->find_stream(path);
    if(m_StreamPos == storagePos->end())
    {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid stream path: " << path;
    }
}
```

If `storage_path::find_stream` and `end()` have no `const` overloads returning
`const_iterator`, add them in pole (`includes/path.hpp`) as a fourth commit on
that branch rather than casting constness away here.

- [ ] **Step 2: Rewrite `readRaster` onto the positional API**

In `src/slideio/drivers/zvi/zviimageitem.cpp`, replace the body of
`readRaster` from the `StreamKeeper` line to the end:

```cpp
    const std::string streamPath = std::string("/Image/Item(") + std::to_string(getItemIndex()) + ")/Contents";
    ZVIUtils::ConstStreamKeeper stream(doc, streamPath);

    if (validBites==0 || validBites==1)
    {
        const std::streamoff bytesToRead = stream->size() - getDataOffset();
        std::vector<uint8_t> buff(bytesToRead);
        const std::streamsize readBytes =
            stream->read_at(getDataOffset(), reinterpret_cast<char*>(buff.data()), bytesToRead);
        if (readBytes != bytesToRead) {
            RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected end of stream reading item "
                << getItemIndex() << ": " << static_cast<long long>(bytesToRead)
                << " bytes requested, " << static_cast<long long>(readBytes) << " available";
        }
        ImageTools::decodeJpegStream(buff.data(), buff.size(), raster);
    }
    else
    {
        raster.create(getHeight(), getWidth(), CV_MAKETYPE(CVTools::toOpencvType(dt), channels));
        cv::Mat& mat = raster.getMatRef();

        const auto readBytes =
            stream->read_at(getDataOffset(), reinterpret_cast<char*>(mat.data), rasterSize);
        if (readBytes != rasterSize) {
            RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected end of stream reading item "
                << getItemIndex() << ": " << static_cast<long long>(rasterSize)
                << " bytes requested, " << static_cast<long long>(readBytes) << " available";
        }
        Endian::fromLittleEndianToNative(dt, mat.data, readBytes);
    }
```

Three things changed and each matters:

- The four `seek` calls are gone, including the redundant one on the old line
  218 that positioned the stream and was then immediately overridden by
  `seek(0, std::ios::end)`.
- The JPEG branch gains a short-read check it never had. It previously trusted
  `read` and handed whatever it got to `decodeJpegStream`; a truncated file
  reached the decoder as a short buffer.
- The uncompressed branch's `std::runtime_error` becomes
  `RAISE_RUNTIME_ERROR` so both branches report the same way and name the item.

- [ ] **Step 3: Build and run the whole ZVI suite**

```bash
cp -r extern/pole/includes/. build/extern/pole/include/pole/
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: green, every test. These tests compare read pixels against reference
rasters, so they are exactly the equivalence evidence this task needs. Note
`openSlideMosaic`'s time for Task 9's report.

- [ ] **Step 4: Confirm the contract has NOT moved**

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter="ConcurrencyContract.*"
```

Expected: green, `zviIsStillSerialised` included. If it fails here, something
flipped `supportsConcurrentReads()` early — Task 7 does that, not this one.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/drivers/zvi/zviutils.hpp src/slideio/drivers/zvi/zviutils.cpp src/slideio/drivers/zvi/zviimageitem.cpp
git commit -m "read ZVI item rasters positionally

readRaster resolved a stream and then seeked it four times, which is what made
two reads of one scene unsafe. It now borrows the stream const and issues one
read_at from the item's data offset. The JPEG branch gains the short-read check
it never had -- a truncated file previously reached decodeJpegStream as a short
buffer. supportsConcurrentReads() is unchanged; that is the next commit."
```

---

## Task 7: ZVI — flip the contract

**Files:**
- Modify: `src/slideio/drivers/zvi/zviscene.hpp`
- Modify: `src/tests/main/test_concurrency_contract.cpp`

**Interfaces:**
- Consumes: `CVScene::supportsConcurrentReads()` (`core/cvscene.hpp:140`).
- Produces: `bool ZVIScene::supportsConcurrentReads() const override` → `true`.

- [ ] **Step 1: Replace the deferred-driver assertion with a positive one**

`src/tests/main/test_concurrency_contract.cpp` already holds
`ConcurrencyContract.zviIsStillSerialised`, which asserts `false` and whose own
failure message prescribes this procedure. Delete that test and put this in its
place, on the same image:

```cpp
// ZVI reports concurrent reads as of the 2026-09-09 conversion. Its mutable
// read-path state lived in the vendored pole submodule; pole's read path is now
// positional, so one ole::compound_document serves every thread. See
// software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md.
TEST(ConcurrencyContract, zviReportsConcurrentReads) {
    const std::string filePath = TestTools::getTestImagePath(
        "zvi", "mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}
```

Leave `expectSerialised()` in place — DCM and GDAL still use it — and update
the comment above the deferred-driver tests so it names only those two.

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ConcurrencyContract.*"
```

Expected: `zviReportsConcurrentReads` FAILS — `supportsConcurrentReads()` is
still the base class's `false`. The other two pass.

- [ ] **Step 3: Add the override**

In `src/slideio/drivers/zvi/zviscene.hpp`, in `ZVIScene`'s public section beside
the other overrides:

```cpp
        // Safe because nothing on the read path holds a cursor: readRaster
        // borrows its stream const and issues one positional read_at, and
        // pole's block loaders read by offset rather than through a shared
        // file cursor. m_Doc is therefore shared by every reader, with one
        // descriptor and no ContextPool -- the CZI and VSI shape, not the
        // NDPI one. See the 2026-09-09 design, section 4.
        bool supportsConcurrentReads() const override { return true; }
```

- [ ] **Step 4: Run it and watch it pass**

```bash
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ConcurrencyContract.*"
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.*"
```

Expected: both green.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/drivers/zvi/zviscene.hpp src/tests/main/test_concurrency_contract.cpp
git commit -m "ZVI scenes report concurrent reads

Tenth of the twelve formats. m_Doc is shared rather than replicated: pole's
read path no longer holds a cursor, so no ContextPool is needed and an open
ZVI still costs one descriptor."
```

---

## Task 8: Byte-exactness under concurrency

Spec §6.1. The gate. Three files, chosen because each exercises a different
part of the read path.

**Files:**
- Modify: `src/tests/main/test_zvi_driver.cpp`

**Interfaces:**
- Consumes: `TestTools::concurrentReadIdentityTestAllPaths(filePath, driver,
  sceneIndex = 0, numRois = 4, numThreads = 16, readsPerThread = 4)`
  (`src/tests/testlib/testtools.hpp:86`).
- Produces: nothing later tasks consume.

- [ ] **Step 1: Add the three tests**

Append to `src/tests/main/test_zvi_driver.cpp`:

```cpp
// Spec 6 requires the channel-subset and level-addressed entry points as well
// as the plain 2D all-channels read; concurrentReadIdentityTestAllPaths covers
// all four shapes in one call.
TEST(ZVIImageDriver, concurrentReadsAreByteIdentical)
{
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    TestTools::concurrentReadIdentityTestAllPaths(filePath, driver);
}

// A Z-stack, which Zeiss-1-Merged is not. ZVI resolves the slice inside
// readTile through TilerData::zSliceIndex, so this is what exercises
// ZVITile::getImageItem's per-slice item lookup under concurrency.
TEST(ZVIImageDriver, concurrentReadsAreByteIdenticalStacked)
{
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    TestTools::concurrentReadIdentityTestAllPaths(filePath, driver);
}

// The mosaic, and the only file here where one block read spans many items and
// therefore many POLE::StreamImpl objects. This is the test that would have
// caught the shared std::fstream cursor in StorageIO::loadBigBlocks -- the
// races on a single item's cursor are invisible to the two tests above, which
// read one tile per scene.
TEST(ZVIImageDriver, concurrentReadsAreByteIdenticalMosaic)
{
    std::string filePath = TestTools::getTestImagePath("zvi", "openslide/Zeiss-3-Mosaic.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    TestTools::concurrentReadIdentityTestAllPaths(filePath, driver);
}
```

- [ ] **Step 2: Run them**

```bash
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.concurrentReads*"
```

Expected: all three green. Record each runtime — the mosaic one will be the
slowest in the suite and Task 9 reports it.

- [ ] **Step 3: Prove the tests can actually fail**

A concurrency test that passes against broken code is worthless. Temporarily
revert the mechanism and confirm the mosaic test goes red:

```bash
git stash push src/slideio/drivers/zvi/zviimageitem.cpp
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.concurrentReadsAreByteIdenticalMosaic"
```

Expected: FAIL, or a crash — the cursor-based `readRaster` is back while the
contract still says reads may overlap. Then:

```bash
git stash pop
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.concurrentReadsAreByteIdenticalMosaic"
```

Expected: green again. Report both outcomes. **If the reverted build passes,
say so plainly and stop** — it means the test is not exercising what this plan
claims, and Task 9's substitute for TSan rests on it.

- [ ] **Step 4: Commit**

```bash
git add src/tests/main/test_zvi_driver.cpp
git commit -m "byte-exactness gates for concurrent ZVI reads

Three files, three shapes: a single-tile scene, a Z-stack, and the 2.0 GB
mosaic whose block reads span many items and so many pole streams. The mosaic
case is the one that fails against a cursor-based readRaster."
```

---

## Task 9: Full suites, timings, descriptor sanity

**Files:** none — verification only.

**Interfaces:** none.

- [ ] **Step 1: Run everything affected, in the foreground**

```bash
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_tests.exe
./build-pole/Release/storage_tests.exe
```

Expected: both green. If anything fails, establish whether it pre-exists by
`git stash`ing and re-running the same filter, and report that comparison. Do
not "fix" unrelated pre-existing failures.

- [ ] **Step 2: Before/after on open time**

Run `ZVIImageDriver.openSlideMosaic` and compare against the 6690 ms baseline
recorded in the spec's investigation:

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.openSlideMosaic"
```

Expected: materially faster — Task 3 removed 1721 ms of it. Report the number.
Then re-run the §3.2 probe over all five ZVI files and report the table
alongside the spec's, so a reviewer can see nothing regressed on the small
files.

- [ ] **Step 3: Count descriptors**

The whole point of the shared-document route is one descriptor per open ZVI,
not one per thread. Verify it rather than trusting it. With the mosaic
byte-exactness test running, check the process's open handles on that file:

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter="ZVIImageDriver.concurrentReadsAreByteIdenticalMosaic" &
# then, in another shell, with Sysinternals handle.exe if available:
handle.exe -p slideio_tests.exe Zeiss-3-Mosaic
```

Expected: exactly two handles on the file — the `std::fstream` pole has always
opened `in|out`, plus the read-only `PositionalFile` Task 4 added. **Not**
sixteen. If `handle.exe` is unavailable, substitute a temporary counter: a
`static std::atomic<int>` incremented in `PositionalFile`'s constructor and
decremented in its destructor, logged at scene close, and removed before
finishing — confirm `git diff --stat` is empty afterwards.

Two, not one, is the expected and documented answer here; Task 10 files the
`in|out` open mode that makes it two.

- [ ] **Step 4: Say plainly what was not run**

ThreadSanitizer is unavailable on this machine (MSVC has no TSan, no Linux
build). The substitute is Task 8 Step 3: the demonstration that the mosaic test
fails against the pre-change read path. Report that explicitly rather than
implying race coverage that does not exist. The `_ref_count` and `_state` races
in particular are the kind a byte-comparison test may not catch, and the honest
statement is that they were removed by construction and are unverified by a
race detector.

- [ ] **Step 5: Report, do not commit**

Nothing to commit. Report: both suite totals, the open-time table, the
descriptor count and how it was obtained, the three byte-exactness runtimes,
the Step 3-of-Task-8 revert outcome, and the TSan gap.

---

## Task 10: Documentation

The contract change is invisible at compile time, so these documents are the
only place a caller learns it moved. This task also closes one tech-debt entry
and files three findings this work turned up but deliberately did not fix.

**Files:**
- Modify: `software-docs/TECH_DEBT.md`
- Modify: `software-docs/BREAKING_CHANGES.md`
- Modify: `CLAUDE.md`
- Modify: `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md`

**Interfaces:** none.

- [ ] **Step 1: Close `TECH_DEBT.md` §14 and correct its cost claim**

Rewrite §14 as resolved. It must (a) say `ZVIScene` now reports
`supportsConcurrentReads() == true`, (b) state that the fix the entry
*recommended* — a per-thread `ole::compound_document` — was measured and
rejected, with the numbers: 1721 ms and 12 MB per document on
`Zeiss-3-Mosaic.zvi`, against 0–3 ms on the ZVIs the "small for a ZVI" estimate
was evidently drawn from, and (c) name the route actually taken and why. Cite
the spec.

- [ ] **Step 2: Update §4's list**

§4 says the serialisation is removed "for the scenes of SVS, PHTIFF, AFI, PKE,
SCN, NDPI, CZI, VSI and OME-TIFF" and is "Kept open because ZVI, DCM and GDAL
still serialise". Add ZVI to the first list; reduce the second to DCM and GDAL;
drop the §14 cross-reference from that sentence.

- [ ] **Step 3: Add a cross-reference to §15 (DCM)**

§15 recommends per-thread `DCMFile` replicas and already says "N x parse is
expensive here, so choose the pool's cap accordingly". Add one sentence pointing
at the 2026-09-09 spec §3.2 as the method for measuring a replica's cost before
committing to it — that instinct was right, and ZVI is the evidence for why it
matters.

- [ ] **Step 4: File the five things deliberately not fixed**

Five new `TECH_DEBT.md` entries, each short and each naming the reason it was
left. All five were found during this work; none is a regression it introduced.

1. **pole opens every compound document read/write.**
   `StorageIO(const char*)` and `StorageIO(const wchar_t*)` open with
   `std::ios::in | std::ios::out`, so slideio cannot open a read-only ZVI or one
   on read-only media at all. Fixing it changes when opens succeed and needs its
   own test; it is also why an open ZVI holds two descriptors rather than one.
2. **`StreamImpl::_state &= StreamImpl::Eof`** keeps the Eof bit and clears
   Bad where `&= ~Eof` was evidently meant. Carried across verbatim by the
   2026-09-09 work so that change stayed behaviour-preserving. Nothing in
   slideio consults either flag.
3. **`compound_document::find_storage` is a linear scan now on the read path.**
   It walks the whole `_storages` tree comparing strings — ~1543 comparisons per
   `readRaster` on the mosaic. Mutates nothing, so a throughput matter, not
   correctness. Wants measurement before anyone restructures the tree into a
   map. Also record that sector coalescing in `loadBigBlocks` was scoped out:
   one `read_at` per block means the mosaic's 2.9 MB tile still costs ~5600
   calls, measured at 61 MB/s cold against 1437 MB/s warm.
4. **`compound_document::path_exist()` is wrong for nested stream paths.** It
   derives the parent storage with `substr(0, path.size() - ++pos)`, which for
   `/Image/Contents` yields `/Image/C` and finds nothing. Measured against
   pole's own `test1.bin`: `false` for all fifteen nested streams,
   while `find_storage` + `find_stream` resolve every one of them. slideio does
   not call it, which is why nothing noticed. Anything that starts calling it
   must fix it first.
5. **`Storage::stream()`'s reuse lookup never matches.** It compares
   `(*it)->path()`, the entry's *short* name, against `name`, which every
   caller passes as a full path, so `reuse = true` always misses and the
   `streams` list grows one entry per stream and is scanned in full each time.
   Worth 2 ms of the mosaic's original 1721, which is why Task 3 left it. It is
   also not safely fixable in isolation: keying on the full path makes reuse
   start working where it never did, and keying on the short name makes every
   item's `Contents` collide.

- [ ] **Step 5: `BREAKING_CHANGES.md`**

Under `v2.10.0`, the existing "`Scene` block reads may now overlap" entry lists
the converted formats and says "ZVI, DCM and GDAL are unchanged". Move ZVI
across — nine of twelve becomes ten of twelve — and leave the behavioural-break
paragraph as it stands. Then add a short paragraph recording pole's exported API
change, since out-of-tree consumers of the submodule are affected:
`ole::basic_stream` gains `read_at` and `size`; `ole::stream_path` gains a
`const` `stream()`; `POLE::Stream` gains `read_at`; `POLE::StorageIO`'s block
loaders became `const` and it gains two data members, which is a layout change
for anything embedding it. No slideio signatures changed. Also worth a line for
consumers: opening a large compound document is now much faster —
1721 ms → 138 ms on a 1543-stream file.

- [ ] **Step 6: `CLAUDE.md`**

Two edits. In the concurrency-contract bullet, add ZVI to "Concurrent today:".
In the dependencies section's pole paragraph, note that pole now carries a
positional read path used by the ZVI driver, that it duplicates
`FileReader`'s primitive because pole must stay standard-library-only, and that
**editing a pole header requires re-staging `includes/` into
`build/extern/pole/include/pole`** — the `file(COPY)` at
`CMakeLists.txt:231` is configure-time, so a stale header compiles silently.
That last point cost time during this work and belongs where the next person
will read it.

- [ ] **Step 7: Mark the spec implemented**

In `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md`, change
**Status:** from `Design proposed, pending approval` to
`Implemented`, and replace §5.2's "Expected effect" sentence with the number
Task 9 Step 2 actually measured.

- [ ] **Step 8: Commit**

```bash
git add software-docs/TECH_DEBT.md software-docs/BREAKING_CHANGES.md CLAUDE.md software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md
git commit -m "document ZVI concurrent reads

TECH_DEBT 14 closed, with the correction that the per-thread compound_document
it recommended costs 1721 ms on the mosaic and was rejected on that evidence.
Three findings filed rather than fixed: pole's read/write open mode, the
_state mask bug, and find_storage's linear scan now sitting on the read path."
```

---

## Dependencies between tasks

```
1 (pole tests runnable)
      |
      +--> 2 (_state + const positional read)
      |         |
      |         +--> 4 (positional file I/O) --> 5 (read_at / size / const borrow)
      |                                                   |
      +--> 3 (linear-time directory walk)                 |
                (independent of 2, 4, 5)                   |
                                                           v
                                                    6 (ZVI positional, contract false)
                                                           |
                                                           v
                                                    7 (flip the contract)
                                                           |
                                                           v
                                                    8 (byte-exactness)
                                                           |
                                                           v
                                                    9 (suites, timings, descriptors)
                                                           |
                                                           v
                                                   10 (documentation)
```

Task 1 gates everything: no pole test can run until its googletest submodule is
initialised and a standalone build tree exists.

Task 3 is fully independent of Tasks 2, 4 and 5 and could ship on its own — it
is a performance fix with no bearing on concurrency. It is placed early because
it is what makes the 1.7 s open cost go away, and because if the rest of the
plan is abandoned it is the piece worth keeping. Its two changes were prototyped
and measured before this plan was written, so it carries the least risk of any
task here and is the natural first thing to merge.

Tasks 2 → 4 → 5 are a chain: Task 5 exposes the `const` positional read Task 2
creates, and it is only *safe* to expose once Task 4 has removed the shared
`fstream` cursor beneath it. Do not reorder them — exposing `read_at` before
Task 4 would publish an API that races.

Task 6 must land before Task 7 and be verified with the contract still `false`,
so that "reads the same bytes" and "reads may overlap" are separate commits with
separate evidence. Task 8 requires Task 7 or its tests are meaningless. Task 10
requires Task 9, because it states measured numbers.

## Self-review notes

- **Spec coverage:** §5.1 → Task 2. §5.2 → Task 3. §5.3 bottom → Task 4; §5.3
  top → Task 5; §5.3's optional sector coalescing → deliberately out of scope,
  filed in Task 10 Step 4. §5.4 → Tasks 6 and 7. §6.1 → Task 8. §6.1.1 →
  Task 7 Step 1. §6.2 → Task 9 Step 4. §6.3 → Task 9 Step 3. §6.4 → Task 1
  plus the tests in Tasks 2, 3 and 5. §6.5 → Task 9 Steps 1–2. §7 → Task 10.
- **Not in the spec, added here:** Task 1 exists because the spec's §6.4
  prerequisites turned out to be blocking rather than incidental; Task 8 Step 3
  (prove the test can fail) exists because TSan is unavailable and something has
  to stand in for it; Task 10 Step 4 grew from three filed findings to five
  (`path_exist`, and the reuse lookup that never matches).
- **Task 3 was rewritten after prototyping.** Its first draft followed the
  spec's original §5.2 — a cached parent index plus a map for
  `Storage::streams`. Measuring a patched copy of pole showed the parent map
  would be dead code once `path()` is guarded (that guard alone removes the
  only caller of `parent()` on this path) and the stream map is worth 2 ms of
  1721. Both were dropped and the spec's §3.3 and §5.2 were corrected. If you
  are reading an older copy of the spec that prescribes a `_parent` field,
  this plan supersedes it.
- **Naming consistency:** `read_at` and `size` on `ole::basic_stream`;
  `read_at` on `POLE::Stream`; `read(pos, data, maxlen, hit_eof)` on
  `StreamImpl`; `ConstStreamKeeper` in `ZVIUtils`; `PositionalFile` in `POLE`;
  `find_siblings(result, index, visited)` on `DirTree`. These spellings are used
  identically in every task that mentions them.
