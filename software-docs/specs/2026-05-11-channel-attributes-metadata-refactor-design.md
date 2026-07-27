# Channel Attributes via `Metadata` — Design

Date: 2026-05-11
Scope: full sweep (public `Scene` read API + `CVScene` read/write API + internal storage + driver call sites + tests).

## Goal

Replace the four `Scene` channel-attribute methods (`getNumChannelAttributes`,
`getChannelAttributeIndex`, `getChannelAttributeName`, `getChannelAttributeValue`)
and their five `CVScene` counterparts with a single `Metadata`-tree accessor.
Drop the parallel name↔index machinery, and allow typed values (Bool/Int/Double/String)
instead of strings only.

## Public read API

On both `CVScene` (core) and `Scene` (public wrapper):

```cpp
const Metadata& getChannelAttributes() const;
```

Returns a lazily-built `Metadata` tree. Shape: **Array of length
`getNumChannels()`**; each element is an Object keyed by attribute name.

- Channels with no attributes get an empty Object — `result[ch]` is always
  valid for any valid channel index in `[0, getNumChannels())`.
- `result[ch].contains(name)` distinguishes "attribute absent" cleanly.
- Values may be `String`, `Int`, `Double`, or `Bool` per the `Metadata::Type` enum.

### Replaced methods

| Old | New |
|---|---|
| `scene.getChannelAttributeValue(ch, "wavelength")` | `scene.getChannelAttributes()[ch]["wavelength"].asString()` |
| `scene.getChannelAttributeName(i)` | `scene.getChannelAttributes()[ch].keys()[i]` |
| `scene.getNumChannelAttributes()` | (gone — per-channel `keys().size()` if needed) |
| `scene.getChannelAttributeIndex(name)` | (gone — names are keys, no indices) |

On `CVScene` the same five methods removed: `defineChannelAttribute`,
`getChannelAttributeIndex`, `getChannelAttributeName`,
`getChannelAttributeValue(int, const std::string&)`,
`getChannelAttributeValue(int, int)`, `getNumChannelAttributes`.

## Producer API (driver-facing, on `CVScene`, `protected`)

Type-safe overloads, same call shape as today so CZI/OME-TIFF driver code
barely changes:

```cpp
protected:
void setChannelAttribute(int channelIndex, const std::string& name, const std::string& value);
void setChannelAttribute(int channelIndex, const std::string& name, const char* value);
void setChannelAttribute(int channelIndex, const std::string& name, bool value);
void setChannelAttribute(int channelIndex, const std::string& name, int64_t value);
void setChannelAttribute(int channelIndex, const std::string& name, double value);
```

The `const char*` overload is required: without it,
`setChannelAttribute(0, "k", "488nm")` would pick `bool` over `std::string`
via the standard conversion rank for string literals.

### Validation
- `channelIndex < 0` or `channelIndex >= getNumChannels()` → throw `RuntimeError`
  (matches the current `setChannelAttribute` contract in `cvscene.cpp:262`).

### Lifecycle contract
Same as `getMetadata()`: setters must be called before the first
`getChannelAttributes()` call. Drivers populate during scene construction;
no current call site reads attributes during init. The first read freezes
the tree.

## Storage on `CVScene`

```cpp
protected:
nlohmann::json m_channelAttributes;            // shape: array of objects

private:
mutable std::once_flag m_channelAttrsOnce;
mutable Metadata       m_channelAttributesMeta;
```

- Setters write into `m_channelAttributes`: if the outer array has fewer
  than `channelIndex + 1` slots, grow it and initialize each new slot as
  an empty object (`json::object()`), not the default `null`, so that
  `m_channelAttributes[channelIndex][name] = value` works directly.
- `getChannelAttributes()` follows the existing `getMetadata()` pattern
  (`cvscene.cpp:323-332`):
  1. `std::call_once` on `m_channelAttrsOnce`
  2. Pad `m_channelAttributes` to `getNumChannels()` with empty objects
  3. Wrap in `Metadata` via `detail::makeMetadataFromJson(std::move(json))`
  4. Cache in `m_channelAttributesMeta`
- Removes the two old members `m_channelAttributeNames` (`vector<string>`)
  and `m_channelAttributes` (`vector<vector<string>>`).

## Relationship to `getMetadata()`

Channel attributes stay a **dedicated method**, not folded into the
`getMetadata()` tree. Rationale:
- Channel attributes are structured, driver-populated per-channel data with
  a fixed shape (Array of Objects).
- `getMetadata()` exposes free-form, format-specific scene metadata.
- Folding would mix two namespaces and risks key collision if a driver
  emits its own `"channels"` node under the main metadata tree.

## Affected files

- `src/slideio/core/cvscene.hpp` — replace 6 member functions + the two
  `m_channelAttributeNames` / `m_channelAttributes` vectors with the new
  API + `nlohmann::json` storage + lazy `Metadata` cache.
- `src/slideio/core/cvscene.cpp` — replace ~70 lines of attribute logic
  (`defineChannelAttribute`, `getChannelAttributeIndex`,
  `setChannelAttribute`, two `getChannelAttributeValue` overloads,
  `getChannelAttributeName`) with the new setter overloads and lazy build.
- `src/slideio/slideio/scene.hpp` — replace the four channel-attribute
  methods (lines 279–282) with one `getChannelAttributes()`.
- `src/slideio/slideio/scene.cpp` — same on the implementation side
  (lines 329–343).
- `src/slideio/drivers/czi/cziscene.cpp:708` — producer call site of
  `setChannelAttribute`; signature unchanged for the `(int, string, string)`
  overload, no driver edit required.
- `src/slideio/drivers/ome-tiff/otscene.cpp:267` — same.
- `src/slideio/converter/tiffconverter.cpp:186-188` — **production consumer**.
  Currently iterates `getNumChannelAttributes()` and reads via index using
  `getChannelAttributeName(idx)` + `getChannelAttributeValue(channel, idx)`.
  Migrates to walking `getChannelAttributes()[channel].keys()` and indexing
  by name.
- `src/tests/main/test_channel_attributes.cpp` — rewrite around the new API;
  covers same ground (set, get by name, invalid channel index, non-existent
  attribute, overwrite, multiple channels) plus new typed-value cases.
- `src/tests/main/test_czi_driver.cpp:675-684` — migrate channel-attribute
  assertions.
- `src/tests/converter/test_converter.cpp:230-238` — same.
- `src/tests/ometiff/test_ometiff_driver.cpp:690-733` — `channelAttributes`
  test currently uses both name- and index-based lookups; rewrite to use
  the new tree API directly.

## Out of scope

- Any change to `getMetadata()` itself.
- Any change to driver behavior beyond the API shift (no new attributes
  surfaced, no semantic interpretation of values).
- Any change to other `CVScene` storage members.
