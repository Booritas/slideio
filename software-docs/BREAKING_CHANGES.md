# Breaking Changes

There is no changelog in this repository yet; this file is a short, factual
record of exported-API changes that break out-of-tree callers, so a consumer
upgrading past a given branch/commit knows what to check. Entries are grouped
by branch.

---

## v2.9.0

### `TIFFKeeper` is now move-only

**Module:** `slideio-imagetools` (exported: `SLIDEIO_IMAGETOOLS_EXPORTS`)
**File:** `src/slideio/imagetools/tiffkeeper.hpp`

Removed:

- The copy constructor and copy assignment operator (`= delete`).
- `operator libtiff::TIFF*()` (implicit conversion to the raw handle).
- `operator=(libtiff::TIFF*)`.
- The `TIFFKeeperPtr` macro. It is now a namespace-scoped
  `using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;` inside `namespace slideio`.

Changed:

- Both constructors (`TIFFKeeper(libtiff::TIFF*)` and
  `TIFFKeeper(const std::string&, bool)`) are now `explicit`.
- A move constructor and move assignment operator were added.

Migration for callers:

- Anywhere a `libtiff::TIFF*` was expected and a `TIFFKeeper` was passed
  implicitly, call `.getHandle()` explicitly.
- Anywhere a raw handle was assigned into a keeper with `operator=`, call
  `.reset(handle)` instead. `reset()` closes the handle the keeper already
  holds before taking the new one; the old `operator=` did not, and leaked.
- Anywhere a `TIFFKeeper` was copied, restructure to move it (`std::move`) or
  hold it through a `shared_ptr`/`unique_ptr` instead.

This is a source-breaking change to an exported class. The SlideIO Python
bindings live in a separate repository and were **not** checked as part of
this work — anyone maintaining that repository, or any other out-of-tree
consumer of `slideio-imagetools`, needs to check for direct use of the
removed members before building against this branch.

See `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md` and
`software-docs/TECH_DEBT.md` §1 for the full rationale.

### `Tools::isXml` was removed

**Module:** `slideio-core` (exported)

`Tools::isXml` was added during Philips TIFF format-detection work to test
whether a string looked like XML. It was never adopted: the intended caller
would have had to parse the document to decide whether to call it, and could
not afford a second parse of an 844 KB description just to answer that
question. It had no caller anywhere in this repository at the time of
removal. Any out-of-tree code calling `Tools::isXml` will need to inline an
equivalent check or drop the dependency.
