# TIFFKeeper Ownership — Design

**Status:** Implemented 2026-08-15
**File:** `src/slideio/imagetools/tiffkeeper.hpp` (+ `tiffkeeper.cpp`)
**Addresses:** section 1 of `software-docs/TECH_DEBT.md`, *`TIFFKeeper` unsafe value semantics*
**Scope option chosen:** 1, the full RAII fix. `NDPITIFFKeeper` is out of scope.

## 1. Goal

Make `TIFFKeeper` a handle that owns what it claims to own.

- Copying one is a compile error rather than a double close.
- Ownership can be moved.
- No operation silently drops a handle on the floor.
- The implicit conversion to a raw owned pointer and the `TIFFKeeperPtr`
  macro are gone.

## 2. Current state

`TIFFKeeper` wraps a `libtiff::TIFF*`, closes it in the destructor, and
forwards a dozen operations to free functions in `TiffTools`. Six classes hold
one by value — `PKEScene`, `SCNScene`, `SCNSlide`, `SVSScene`, `VSIFileScene`
and `SmallTiffWrapper` — the converter holds one through a `shared_ptr`, and
several drivers and tests construct one on the stack.

The problems recorded in the debt log, re-verified against the tree at
`50dd77c0`. The numbering below is this document's own — the log combines
copying and the missing move into its problem 1, and its problem 3 is the
already-fixed constructor inconsistency:

1. **Copy is implicitly generated and public.** Copying duplicates the raw
   pointer, so both copies close it: double close, and use-after-free for
   whichever object outlives the first destructor. Any class holding one by
   value silently inherits this.
2. **No move.** Ownership cannot be handed over, which is part of why the
   converter reaches for `shared_ptr<TIFFKeeper>`.
3. **`operator=(libtiff::TIFF*)` leaks.** It overwrites `m_hFile` without
   closing the previous handle. Eight call sites use it.
4. **`operator libtiff::TIFF*()`** converts an owning wrapper to a raw owned
   pointer implicitly — an accidental `TIFFClose`, a pointer arithmetic
   mistake, or an overload ambiguity with the raw-pointer `operator=` all
   become possible, and most call sites already prefer `getHandle()`.
5. **`#define TIFFKeeperPtr std::shared_ptr<slideio::TIFFKeeper>`** where a
   namespace-scoped alias belongs: it ignores scope, leaks into every including
   translation unit, and cannot be qualified.
6. **Header hygiene.** `<memory>` and `<cstdint>` arrive transitively.

The entry's problem 3, inconsistent construction of `m_messageHandler`, is
already fixed and struck through in the log; both constructors create it.

### Two problems the debt entry does not record

Found while reading the file for this design, and in scope because a fix that
leaves either in place does not achieve the goal:

- **`openTiffFile()` leaks exactly as `operator=` does.** `tiffkeeper.cpp:31`
  assigns `TiffTools::openTiffFile(...)` to `m_hFile` without closing whatever
  was there. Removing the leak from `operator=` while leaving it here would
  close the headline door and leave the side door open. One external caller:
  `smalltiffwrapper.cpp:84`.
- **`m_hFile` has no default member initializer.** Harmless today, because a
  constructor that throws means no destructor runs, but it stops being harmless
  as soon as a member whose construction can throw is declared after it.

One item in the entry turned out to be a non-issue: `closeTiffFile()` on a null
handle is safe, because `TiffTools::closeTiffFile` null-checks.

## 3. Scope

In scope: the class itself and every call site the change breaks.

Out of scope, and deliberately:

- **`NDPITIFFKeeper`** (`ndpitifftools.hpp`), a near-twin with the same
  copyable/leaking shape. Unifying the two is the debt entry's own follow-up
  note and stays open. It also has no message handler, so unifying would first
  require deciding whether the ndpi driver should gain that behaviour — a
  question this change does not need to answer.
- The converter's `shared_ptr<TIFFKeeper>`. Once the class is movable that
  indirection is probably unnecessary, but removing it is a separate change
  with its own reasoning, and this one should not grow into it.

## 4. Approach

### 4.1 The class

```cpp
    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFKeeper
    {
    public:
        explicit TIFFKeeper(libtiff::TIFF* hFile = nullptr);
        explicit TIFFKeeper(const std::string& filePath, bool readOnly = true);
        ~TIFFKeeper();

        // An owning handle must not be copied: two owners means two closes.
        TIFFKeeper(const TIFFKeeper&)            = delete;
        TIFFKeeper& operator=(const TIFFKeeper&) = delete;
        TIFFKeeper(TIFFKeeper&& other) noexcept;
        TIFFKeeper& operator=(TIFFKeeper&& other) noexcept;

        libtiff::TIFF* getHandle() const { return m_hFile; }
        bool isValid() const { return m_hFile != nullptr; }

        // Takes ownership of a raw handle, closing any handle already held.
        void reset(libtiff::TIFF* hFile = nullptr);
        // Gives up ownership without closing.
        libtiff::TIFF* release();

        void openTiffFile(const std::string& filePath, bool readOnly = true);
        void closeTiffFile();
        // ... the forwarding methods, unchanged ...

    private:
        libtiff::TIFF* m_hFile = nullptr;
        std::shared_ptr<TIFFMessageHandler> m_messageHandler;
    };

    using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;
```

`reset()` replaces `operator=(libtiff::TIFF*)` and closes first.
`openTiffFile()` routes through `reset()`, which closes the previously held
handle and fixes the unrecorded leak. Move assignment closes its own handle
before taking the source's. Both constructors share one private initialiser for
`m_messageHandler`. `<memory>` and `<cstdint>` are included directly.

Both constructors become `explicit`. This goes beyond the minimum: an implicit
conversion *into* an owning wrapper from a raw owned pointer is the same
category of footgun as the implicit conversion out, and this change removes the
latter. It costs two call sites, both in tests, where
`TIFFKeeper tiff = TiffTools::openTiffFile(...)` becomes direct initialisation.

### 4.2 Call sites

Three mechanical groups. All of them are compile errors if missed, which is the
point of making the change this way round.

- **Five raw-pointer assignments become `reset()`**: `pkescene.cpp:40`,
  `scnscene.cpp:232`, `scnslide.cpp:25`, `svsscene.cpp:40`,
  `vsifilescene.cpp:73`. A grep for `= TiffTools::openTiffFile` finds eight
  hits, but three of them — `otslide.cpp:92`, `pkeslide.cpp:61` and
  `svsslide.cpp:153` — assign to a local `libtiff::TIFF*`, not to a keeper, and
  are unaffected.
- **Two test sites take direct initialisation** once the `TIFF*` constructor is
  `explicit`: `test_fiwrapper.cpp:106` and `:171`, where
  `TIFFKeeper tiff = TiffTools::openTiffFile(...)` becomes
  `TIFFKeeper tiff(TiffTools::openTiffFile(...));`.
- **Implicit conversions become `.getHandle()`**: `scnscene.cpp:211`,
  `scnslide.cpp:29`, `vsifilescene.cpp:29`, and the `return m_tiffKeeper;` in
  `pkescene.cpp:50` and `svsscene.cpp:50`. Note that `ome-tiff/tiffdata.hpp`'s
  `m_tiff` is a *raw* `TIFF*`, not a keeper, so its three uses are not
  conversion sites. The compiler is the authority on the final list.
- **One macro user**: `tiffconverter.hpp:161` picks up the alias instead.

### 4.3 What the build proves

Six classes hold a `TIFFKeeper` by value. Making the class move-only turns any
accidental copy of those into a compile error. The debt entry predicts there are
none, because those members live in slide and scene classes that are handled
through `shared_ptr`. If the build disagrees, that is a live double close this
change has just found, and the right response is to stop and report it rather
than to reinstate a copy constructor to make the build pass.

## 5. Testing

`TIFFKeeper` has no unit tests today. Add a suite covering the ownership
contract, since that is the whole subject of the change:

- `release()` returns the handle and leaves the keeper empty.
- `reset()` closes the handle previously held.
- `reset()` on an empty keeper takes ownership without incident.
- The move constructor transfers the handle and leaves the source empty.
- Move assignment closes the destination's own handle before taking the
  source's.
- The destructor closes.
- `openTiffFile()` on a keeper that already holds a handle closes the old one.

Ownership is observable rather than inferred: on Windows libtiff opens without
`FILE_SHARE_DELETE`, so "the handle was closed" can be asserted by copying a
TIFF to a temp path and deleting the copy afterwards. This is the same technique
that covers the Philips open-failure leak in `test_phtiff_driver.cpp`.

Copy-rejection is a compile-time property and gets a comment in the test file
rather than a test — a test that must fail to compile is not worth the build
machinery here.

Gate: `slideio_tests`, `slideio_ometiff_tests`, `slideio_ndpi_tests`,
`slideio_converter_tests`, `slideio_pke_tests`, `slideio_vsi_tests`,
`slideio_phtiff_tests`, `slideio_transformer_tests` — every suite, because
keepers are held across the drivers and the converter.

## 6. Risks

| Risk | Handling |
|---|---|
| An accidental copy exists and the build breaks | That is the change working. Report it as a found defect; do not reinstate copying. |
| A conversion site is missed | Impossible to miss silently: removing `operator TIFF*()` makes each one a compile error. |
| `reset()` closes a handle another object still uses | Only where the old code leaked it instead. Any such site was already wrong; the tests above and the per-driver suites are the check. |
| The converter's `shared_ptr<TIFFKeeper>` stops compiling | `shared_ptr` needs no copyable element type, so it continues to work. Only the macro-to-alias change touches it. |
| Public API change for out-of-tree consumers | Real: `TIFFKeeper` is `SLIDEIO_IMAGETOOLS_EXPORTS`, and removing copy, `operator TIFF*()` and the macro breaks any external caller relying on them. The Python bindings live in a separate repository and cannot be checked from here. Needs a release note. |
