# `MetadataBuilder` — Design

Date: 2026-05-11
Scope: add a mutable companion to `Metadata`; use it as the channel-attribute
storage on `CVScene`; replace the `void* rootHandle` hack in
`CVScene::buildMetadataTree` and `CVSlide::buildMetadataTree` with a typed
return-style hook.

## Goal

Eliminate two leaks of `nlohmann::json` into the public-header API of the
core library:

1. `nlohmann::json m_channelAttributesJson` is currently a value member of
   `CVScene` (introduced in the channel-attributes refactor). It forced
   `nlohmann_json` to be linked `PUBLIC` so transitive consumers of
   `cvscene.hpp` can see the complete type.
2. `virtual void buildMetadataTree(void* rootHandle) const` on both
   `CVScene` and `CVSlide` is a type-erased out-parameter that drivers cast
   back to `nlohmann::json*` via `detail::asJson()`. The hack exists for
   the same reason — to keep `nlohmann/json.hpp` out of the public header.

`MetadataBuilder` is the mutable companion to the existing read-only
`Metadata` view. Once it exists, both leaks go away.

## Non-goals

- Mutable `Metadata`. The read API stays immutable.
- Public `nlohmann::json` anywhere outside `metadata_internal.hpp`.
- JSON-pointer path setters on the builder. Navigation via `operator[]`
  is the only mutation entry point; a path-style helper can be added
  later if a need surfaces.

## API surface

### `MetadataBuilder` (in `slideio/core/metadata.hpp`)

```cpp
class SLIDEIO_CORE_EXPORTS MetadataBuilder
{
public:
    MetadataBuilder();                              // empty (Null root)
    ~MetadataBuilder();
    MetadataBuilder(const MetadataBuilder&);        // deep copy at top level
    MetadataBuilder(MetadataBuilder&&) noexcept;
    MetadataBuilder& operator=(const MetadataBuilder&);
    MetadataBuilder& operator=(MetadataBuilder&&) noexcept;

    // Navigation. Returns a sub-builder sharing root storage with this one.
    // Auto-creates and coerces: operator[](key) on a Null node turns it into
    // an Object; operator[](index) on a Null node turns it into an Array and
    // grows the array up to index+1 (new slots default to empty Objects).
    // Throws slideio::RuntimeError on type mismatch (e.g. operator[](key)
    // on an Array node, or operator[](size_t) on an Object node).
    MetadataBuilder operator[](const std::string& key);
    MetadataBuilder operator[](size_t index);

    // Leaf assignment. Replaces the current node with a scalar value.
    void set(bool   value);
    void set(int64_t value);
    void set(double value);
    void set(const std::string& value);
    void set(const char* value);                    // null pointer throws

    // Explicit container coercion. Ensures the current node is an empty
    // Object/Array. Idempotent if the node is already of that type;
    // replaces a scalar/Null otherwise.
    void makeObject();
    void makeArray();

    // Inspection. Mirrors Metadata for sanity checks.
    bool   isNull()   const;
    bool   isObject() const;
    bool   isArray()  const;
    size_t size()     const;

    // Snapshot the current state into an immutable Metadata.
    // The builder may continue to be mutated; the returned Metadata is
    // independent of subsequent mutations.
    Metadata freeze() const;

    struct Impl;
private:
    explicit MetadataBuilder(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> m_impl;
};
```

### Internal bridge (in `slideio/core/metadata_internal.hpp`)

```cpp
namespace slideio::detail
{
    // Existing — unchanged:
    Metadata makeMetadataFromJson(nlohmann::json root);
    nlohmann::json xmlStringToJson(const std::string& xml);

    // New:
    MetadataBuilder builderFromJson(nlohmann::json root);
    MetadataBuilder makeDefaultMetadataBuilder(const std::string& rawMetadata,
                                               MetadataFormat fmt);

    // Removed (no callers after this refactor):
    //   void buildDefaultMetadataTree(nlohmann::json& root,
    //                                 const std::string& rawMetadata,
    //                                 MetadataFormat fmt);
    //   inline nlohmann::json& asJson(void* handle);
}
```

`builderFromJson(json)` is the path for drivers that already have a fully
parsed `nlohmann::json` and don't want to traverse it field-by-field
(SVSSlide is the only such driver today). `metadata_internal.hpp` is the
existing convention for "include if you need nlohmann interop"; this
function joins the existing `makeMetadataFromJson` there.

`makeDefaultMetadataBuilder(raw, fmt)` is the return-style replacement
for the deleted `buildDefaultMetadataTree`. It is used by the base-class
default `buildMetadataTree()` implementations on `CVScene` and `CVSlide`.

## Implementation notes (not visible in header)

`MetadataBuilder::Impl` (in `metadata.cpp`) holds:

```cpp
struct MetadataBuilder::Impl
{
    std::shared_ptr<nlohmann::json> root;   // shared owner of the tree
    nlohmann::json*                 view;   // non-const pointer into *root
};
```

Top-level copy (`MetadataBuilder(const MetadataBuilder&)`) deep-copies
`*root` into a fresh `shared_ptr<nlohmann::json>` and points `view` at
the new root. Sub-views from `operator[]` create a fresh `Impl` that
shares the same `root` shared_ptr and sets `view` to the child node
inside that tree — so mutation through a sub-view is visible in the
parent.

`freeze()` deep-copies `*view` (not the whole root — just the subtree
addressed by the current view) into a `std::make_shared<const
nlohmann::json>` and constructs a `Metadata` via the existing
`detail::makeMetadataFromJson` path (or an equivalent internal helper
that takes a `shared_ptr<const json>` directly). The original builder
is unaffected.

### Coercion / error semantics

- `operator[](key)` on a Null node: coerces to Object, then auto-creates
  the missing key with a Null value, returns sub-view.
- `operator[](key)` on an Object node: returns sub-view (auto-creates
  key as Null if absent).
- `operator[](key)` on an Array/scalar node: throws
  `slideio::RuntimeError` with "operator[](key) on non-object node".
- `operator[](index)` on a Null node: coerces to Array, grows to
  `index+1` slots (new slots default to empty Objects, not Null), returns
  sub-view at `index`.
- `operator[](index)` on an Array node: grows the array if needed (new
  slots default to empty Objects), returns sub-view at `index`.
- `operator[](index)` on an Object/scalar node: throws.

The "new slots default to empty Objects, not Null" choice matches the
existing channel-attribute padding behavior and means the common pattern
`b[i][key].set(v)` works without intermediate coercion calls.

### Thread safety

Same as `Metadata`: not thread-safe. Mutation through one builder while
another sub-view of the same root is being read is a race. The
CVScene/CVSlide use sites mutate during scene construction and freeze
once via `std::call_once`; that pattern is safe.

## `buildMetadataTree` signature change

On both `CVScene` (`cvscene.hpp:233-236`) and `CVSlide` (`cvslide.hpp:83`):

```cpp
// Before:
virtual void buildMetadataTree(void* rootHandle) const;

// After:
virtual MetadataBuilder buildMetadataTree() const;
```

Default base implementation:

```cpp
MetadataBuilder CVScene::buildMetadataTree() const
{
    return detail::makeDefaultMetadataBuilder(m_rawMetadata, m_metadataFormat);
}
```

Caller site (`getMetadata()`):

```cpp
const Metadata& CVScene::getMetadata() const
{
    std::call_once(m_metadataOnce, [this] {
        m_metadata = buildMetadataTree().freeze();
    });
    return m_metadata;
}
```

SVSSlide override:

```cpp
MetadataBuilder SVSSlide::buildMetadataTree() const
{
    return detail::builderFromJson(
        SVSTools::parseAperioMetadata(m_rawMetadata));
}
```

## Channel-attribute storage migration

On `CVScene`, replace:

```cpp
nlohmann::json m_channelAttributesJson;        // protected member
```

with:

```cpp
MetadataBuilder m_channelAttrs;                // protected member
```

The five `setChannelAttribute` overloads change shape from
`growToChannel(m_channelAttributesJson, channelIndex)[name] = value;` to:

```cpp
m_channelAttrs[channelIndex][name].set(value);
```

The `growToChannel` anon-namespace helper in `cvscene.cpp` is deleted —
`MetadataBuilder::operator[](size_t)` handles auto-growth.

`getChannelAttributes()` becomes:

```cpp
const Metadata& CVScene::getChannelAttributes() const
{
    std::call_once(m_channelAttrsOnce, [this] {
        const int numChannels = getNumChannels();
        for (int i = 0; i < numChannels; ++i) {
            m_channelAttrs[i].makeObject();      // pad to numChannels
        }
        m_channelAttributesMeta = m_channelAttrs.freeze();
    });
    return m_channelAttributesMeta;
}
```

`makeObject()` is idempotent on an Object node, so this loop is safe
to run even when some channels already have attributes set.

## Header / build impact

- `cvscene.hpp`: drop `#include <nlohmann/json.hpp>` (added in the
  channel-attributes refactor for the json storage member; that member
  goes away in this refactor).
- `cvscene.hpp`: also drop the nlohmann reference in the docstring of
  `buildMetadataTree` (the `void* rootHandle` paragraph). The signature
  change makes the docstring obsolete.
- `cvslide.hpp`: does not currently include `nlohmann/json.hpp`
  directly — no change.
- `src/slideio/core/CMakeLists.txt`: `nlohmann_json` link returns from
  `PUBLIC` to `PRIVATE`.
- Downstream drivers and the converter no longer transitively see
  nlohmann via `cvscene.hpp`. Drivers that need nlohmann directly
  (e.g. SVSSlide) include it themselves, via `metadata_internal.hpp` or
  directly.

## Affected files

Core:
- `src/slideio/core/metadata.hpp` — add `class MetadataBuilder`.
- `src/slideio/core/metadata.cpp` — implement `MetadataBuilder`, `Impl`,
  and the navigation/coercion semantics.
- `src/slideio/core/metadata_internal.hpp` — add
  `detail::builderFromJson` and `detail::makeDefaultMetadataBuilder`;
  remove `detail::buildDefaultMetadataTree(json&, ...)` and `asJson()`.
- `src/slideio/core/cvscene.hpp` — change `buildMetadataTree` signature;
  replace `m_channelAttributesJson` with `MetadataBuilder m_channelAttrs;`;
  drop the `nlohmann/json.hpp` include.
- `src/slideio/core/cvscene.cpp` — update `getMetadata()`,
  `buildMetadataTree()` default, all five `setChannelAttribute`
  overloads, `getChannelAttributes()`; remove the `growToChannel`
  anon-namespace helper.
- `src/slideio/core/cvslide.hpp` — change `buildMetadataTree` signature.
- `src/slideio/core/cvslide.cpp` — update `getMetadata()`,
  `buildMetadataTree()` default.
- `src/slideio/core/CMakeLists.txt` — `nlohmann_json` to `PRIVATE`.

Drivers:
- `src/slideio/drivers/svs/svsslide.hpp` — update override signature.
- `src/slideio/drivers/svs/svsslide.cpp` — replace one-line body with
  the `detail::builderFromJson(...)` call.

Tests:
- `src/tests/main/test_metadata_builder.cpp` (new) — unit tests for
  `MetadataBuilder`. Coverage:
  - default construction is Null.
  - `operator[](key)` auto-creates Object and missing key.
  - `operator[](size_t)` auto-creates Array and grows; new slots are
    empty Objects.
  - Each of the five `set()` overloads round-trips through `freeze()`
    with the correct `Metadata::Type`.
  - Type-mismatch on `operator[]` throws.
  - Top-level copy is independent (mutation through one builder doesn't
    affect the copy).
  - Sub-view from `operator[]` shares storage with the parent
    (mutation through the sub-view IS visible in the parent's
    `freeze()`).
  - `freeze()` is independent of subsequent mutation (the snapshot
    doesn't change when the builder is mutated after `freeze`).
  - `makeObject()` / `makeArray()` are idempotent on matching types and
    replace scalar/Null nodes.

Existing tests that must continue to pass without changes (we are not
breaking any consumer of `Metadata` or `getChannelAttributes()`):
- `src/tests/main/test_channel_attributes.cpp` (all 10 tests).
- All driver tests that exercise `getMetadata()` (the public read API
  shape is unchanged).
- `src/tests/converter/test_converter.cpp`.
- `src/tests/main/test_czi_driver.cpp` (channel-attributes assertions).
- `src/tests/ometiff/test_ometiff_driver.cpp` (channelAttributes test).

## Migration sequencing

The two phases (channel-attribute storage swap; `buildMetadataTree`
signature change) are independent of each other but both depend on
`MetadataBuilder` existing first. The natural ordering:

1. Add `MetadataBuilder` (header + implementation + unit tests).
2. Migrate channel-attribute storage on `CVScene`.
3. Migrate `buildMetadataTree` on `CVScene`, `CVSlide`, and SVSSlide
   together (signature change is base-class + override; cannot be split).
4. Drop `#include <nlohmann/json.hpp>` from `cvscene.hpp` and flip
   `nlohmann_json` to `PRIVATE` in CMake. Verify the full build still
   compiles (downstream drivers and the converter must not silently
   rely on transitive nlohmann visibility).

Each step keeps the build green. The implementation plan derived from
this spec will lay out these as separate, individually committable tasks.
