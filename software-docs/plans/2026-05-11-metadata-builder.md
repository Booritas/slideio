# `MetadataBuilder` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `MetadataBuilder` — the mutable companion to `Metadata` — and use it to (1) replace `nlohmann::json m_channelAttributesJson` on `CVScene` and (2) replace the `void* rootHandle` parameter in `CVScene::buildMetadataTree` / `CVSlide::buildMetadataTree` with a typed return-style hook. After this plan, `nlohmann/json.hpp` no longer appears in any public header of `slideio-core`.

**Architecture:** Build `MetadataBuilder` from the inside out (skeleton → leaf setters → navigation → coercion → errors → copy semantics → internal helpers), then perform two bridge-style migrations (channel-attribute storage; `buildMetadataTree` signature), then drop the nlohmann header include and flip the CMake link to `PRIVATE`. Each task is independently committable and keeps the build green.

**Tech Stack:** C++17, Google Test, `nlohmann::json` (kept internal), `slideio-core` shared library.

**Reference spec:** `docs/superpowers/specs/2026-05-11-metadata-builder-design.md`

---

## Pre-flight

Branch: `metadata-builder` (already exists, stacked on `channel-metadata`).

Sanity-check the workspace before starting. All build commands assume `release` config unless noted. Prefix with `PATH="$HOME/miniconda3/envs/conan2/bin:$PATH"` if `conan` is not on the default `PATH`.

```bash
git rev-parse --abbrev-ref HEAD          # expect: metadata-builder
git log --oneline -1                     # expect: 3aba3aa docs: MetadataBuilder design spec
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*:Metadata.*'
```
Expected: build succeeds; all `ChannelAttributesTest.*` (10 tests) and `Metadata.*` cases pass against the current implementation. This is the baseline.

---

## Task 1: `MetadataBuilder` skeleton — default constructor, `set(string)`, `freeze()`

The minimum slice that puts the class into the codebase and exercises a round-trip through `freeze()`. Pimpl-style: header declares `struct Impl` as an incomplete type; the actual storage and methods are in the `.cpp`.

**Files:**
- Modify: `src/slideio/core/metadata.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Create: `src/tests/main/test_metadata_builder.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Create the test file with a failing test**

Create `src/tests/main/test_metadata_builder.cpp` with:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"

using namespace slideio;

TEST(MetadataBuilder, DefaultIsNull)
{
    MetadataBuilder b;
    EXPECT_TRUE(b.isNull());

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Null);
}

TEST(MetadataBuilder, SetStringRoundtrip)
{
    MetadataBuilder b;
    b.set(std::string("hello"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::String);
    EXPECT_EQ(m.asString(), "hello");
}
```

- [ ] **Step 2: Register the test file in CMakeLists**

In `src/tests/main/CMakeLists.txt`, find the `TEST_SOURCES` list (around lines 23–39 with entries like `test_metadata.cpp`, `test_channel_attributes.cpp`). Add a new line for `test_metadata_builder.cpp` next to `test_metadata.cpp`:

```cmake
  test_metadata.cpp
  test_metadata_builder.cpp
  test_tifffiles.cpp
```

- [ ] **Step 3: Build to verify the test fails**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error referencing `MetadataBuilder` — undeclared identifier (no header symbol yet).

- [ ] **Step 4: Declare `MetadataBuilder` in `metadata.hpp`**

In `src/slideio/core/metadata.hpp`, just before the closing `}` of the `namespace slideio` block (after the `Metadata` class definition, around line 63), add:

```cpp
    class SLIDEIO_CORE_EXPORTS MetadataBuilder
    {
    public:
        MetadataBuilder();
        ~MetadataBuilder();
        MetadataBuilder(const MetadataBuilder&);
        MetadataBuilder(MetadataBuilder&&) noexcept;
        MetadataBuilder& operator=(const MetadataBuilder&);
        MetadataBuilder& operator=(MetadataBuilder&&) noexcept;

        // Leaf assignment — replaces the current node with a scalar value.
        void set(const std::string& value);

        // Inspection.
        bool isNull() const;

        // Snapshot the current state into an immutable Metadata.
        Metadata freeze() const;

        struct Impl;
    private:
        explicit MetadataBuilder(std::shared_ptr<Impl> impl);
        std::shared_ptr<Impl> m_impl;
    };
```

- [ ] **Step 5: Implement `MetadataBuilder` in `metadata.cpp`**

In `src/slideio/core/metadata.cpp`, append before the closing `}` of the `namespace slideio` block (after the existing `detail::buildDefaultMetadataTree` body, near the end of the file):

```cpp
    struct MetadataBuilder::Impl
    {
        std::shared_ptr<nlohmann::json> root;   // shared owner of the tree
        nlohmann::json*                 view = nullptr;   // points into *root
    };

    MetadataBuilder::MetadataBuilder()
        : m_impl(std::make_shared<Impl>())
    {
        m_impl->root = std::make_shared<nlohmann::json>();   // default = Null
        m_impl->view = m_impl->root.get();
    }
    MetadataBuilder::~MetadataBuilder() = default;
    MetadataBuilder::MetadataBuilder(const MetadataBuilder&) = default;
    MetadataBuilder::MetadataBuilder(MetadataBuilder&&) noexcept = default;
    MetadataBuilder& MetadataBuilder::operator=(const MetadataBuilder&) = default;
    MetadataBuilder& MetadataBuilder::operator=(MetadataBuilder&&) noexcept = default;
    MetadataBuilder::MetadataBuilder(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

    void MetadataBuilder::set(const std::string& value)
    {
        *m_impl->view = value;
    }

    bool MetadataBuilder::isNull() const
    {
        return m_impl->view->is_null();
    }

    Metadata MetadataBuilder::freeze() const
    {
        return detail::makeMetadataFromJson(nlohmann::json(*m_impl->view));
    }
```

Note: the default copy/move semantics here are temporary — they share `m_impl` (shallow). Task 6 reworks them to the spec-defined behavior (deep copy at top level, shared storage on sub-views). For now, no test depends on copy independence.

- [ ] **Step 6: Build and verify both tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 2/2 pass — `MetadataBuilder.DefaultIsNull`, `MetadataBuilder.SetStringRoundtrip`.

- [ ] **Step 7: Run the full `Metadata.*` and `ChannelAttributesTest.*` suites to confirm no regression**

```bash
./build/release/bin/slideio_tests --gtest_filter='Metadata.*:ChannelAttributesTest.*'
```
Expected: all pre-existing tests pass (the new type was additive only).

- [ ] **Step 8: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/tests/main/test_metadata_builder.cpp src/tests/main/CMakeLists.txt
git commit -m "core: add MetadataBuilder skeleton with set(string) and freeze()"
```

---

## Task 2: Remaining `set()` overloads (`bool`, `int64_t`, `double`, `const char*`)

**Files:**
- Modify: `src/slideio/core/metadata.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the failing test**

Append to `src/tests/main/test_metadata_builder.cpp`:

```cpp
TEST(MetadataBuilder, SetTypedLeafRoundtrip)
{
    {
        MetadataBuilder b;
        b.set(true);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Bool);
        EXPECT_TRUE(m.asBool());
    }
    {
        MetadataBuilder b;
        b.set(static_cast<int64_t>(488));
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Int);
        EXPECT_EQ(m.asInt(), 488);
    }
    {
        MetadataBuilder b;
        b.set(0.1);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Double);
        EXPECT_DOUBLE_EQ(m.asDouble(), 0.1);
    }
    {
        MetadataBuilder b;
        b.set("literal");
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::String);
        EXPECT_EQ(m.asString(), "literal");
    }
}

TEST(MetadataBuilder, SetConstCharNullThrows)
{
    MetadataBuilder b;
    EXPECT_THROW(b.set(static_cast<const char*>(nullptr)), slideio::RuntimeError);
}
```

The `RuntimeError` type is declared in `slideio/base/exceptions.hpp`; the test file does not currently include that header — add at the top of the test file:

```cpp
#include "slideio/base/exceptions.hpp"
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error — no overloads for `set(bool)`, `set(int64_t)`, `set(double)`, `set(const char*)`. The `set(int64_t)` call may also produce an ambiguous-overload diagnostic with `set(const std::string&)` via integral-to-string promotion — that's expected.

- [ ] **Step 3: Add the four overload declarations to `metadata.hpp`**

In `src/slideio/core/metadata.hpp`, in the `MetadataBuilder` class, right after the existing `void set(const std::string& value);` line, insert:

```cpp
        void set(bool value);
        void set(int64_t value);
        void set(double value);
        void set(const char* value);
```

- [ ] **Step 4: Implement the four overload bodies in `metadata.cpp`**

In `src/slideio/core/metadata.cpp`, immediately after the existing `MetadataBuilder::set(const std::string&)` body, add:

```cpp
    void MetadataBuilder::set(bool value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(int64_t value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(double value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(const char* value)
    {
        if (value == nullptr) {
            RAISE_RUNTIME_ERROR << "MetadataBuilder::set: const char* value must not be null";
        }
        *m_impl->view = std::string(value);
    }
```

The `RAISE_RUNTIME_ERROR` macro is declared in `slideio/base/exceptions.hpp`. Check the top of `metadata.cpp` for an existing include; if it's not present, add:

```cpp
#include "slideio/base/exceptions.hpp"
```

(near the top alongside the other slideio includes).

- [ ] **Step 5: Build and verify the new tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 4/4 tests pass — the 2 from Task 1 plus `SetTypedLeafRoundtrip` and `SetConstCharNullThrows`.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/tests/main/test_metadata_builder.cpp
git commit -m "core: add typed MetadataBuilder set() overloads"
```

---

## Task 3: `operator[](key)` navigation + Object auto-create + `makeObject()` + `isObject()`

**Files:**
- Modify: `src/slideio/core/metadata.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_metadata_builder.cpp`:

```cpp
TEST(MetadataBuilder, ObjectKeyAutoCreate)
{
    MetadataBuilder b;
    b["wavelength"].set(std::string("488nm"));
    b["exposure"].set(std::string("100ms"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_TRUE(m.contains("wavelength"));
    EXPECT_TRUE(m.contains("exposure"));
    EXPECT_EQ(m["wavelength"].asString(), "488nm");
    EXPECT_EQ(m["exposure"].asString(),   "100ms");
}

TEST(MetadataBuilder, MakeObjectIdempotent)
{
    MetadataBuilder b;
    b.makeObject();
    EXPECT_TRUE(b.isObject());

    b["a"].set(std::string("1"));
    b.makeObject();                                  // idempotent — must not clobber "a"
    EXPECT_TRUE(b.isObject());

    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("a"));
    EXPECT_EQ(m["a"].asString(), "1");
}

TEST(MetadataBuilder, MakeObjectOnScalarReplaces)
{
    MetadataBuilder b;
    b.set(std::string("scalar"));
    b.makeObject();                                  // replaces the scalar

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_EQ(m.size(), 0u);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error — `operator[]`, `makeObject`, `isObject` undeclared.

- [ ] **Step 3: Add declarations to `metadata.hpp`**

In the `MetadataBuilder` class, after the existing `set(...)` overloads, insert:

```cpp
        // Navigation. Returns a sub-builder sharing root storage with this one.
        // Auto-creates and coerces: operator[](key) on a Null node turns it into
        // an Object; on an Object node it returns a sub-view (auto-creating the
        // key with Null if absent). Throws slideio::RuntimeError on type mismatch
        // (e.g. operator[](key) on an Array or scalar node).
        MetadataBuilder operator[](const std::string& key);

        // Ensures the current node is an empty Object. Idempotent if already
        // an Object; replaces a scalar/Null/Array otherwise.
        void makeObject();

        // Inspection.
        bool isObject() const;
```

- [ ] **Step 4: Implement in `metadata.cpp`**

Append after the existing `set()` bodies:

```cpp
    MetadataBuilder MetadataBuilder::operator[](const std::string& key)
    {
        if (m_impl->view->is_null()) {
            *m_impl->view = nlohmann::json::object();
        }
        if (!m_impl->view->is_object()) {
            RAISE_RUNTIME_ERROR << "MetadataBuilder::operator[](key): current node is not an object";
        }
        nlohmann::json& child = (*m_impl->view)[key];
        auto childImpl = std::make_shared<Impl>();
        childImpl->root = m_impl->root;
        childImpl->view = &child;
        return MetadataBuilder(std::move(childImpl));
    }

    void MetadataBuilder::makeObject()
    {
        if (!m_impl->view->is_object()) {
            *m_impl->view = nlohmann::json::object();
        }
    }

    bool MetadataBuilder::isObject() const
    {
        return m_impl->view->is_object();
    }
```

- [ ] **Step 5: Build and verify the new tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 7/7 pass — the 4 from before plus the 3 new ones.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/tests/main/test_metadata_builder.cpp
git commit -m "core: add MetadataBuilder Object navigation + makeObject"
```

---

## Task 4: `operator[](size_t)` navigation + Array auto-grow + `makeArray()` + `isArray()` + `size()`

**Files:**
- Modify: `src/slideio/core/metadata.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_metadata_builder.cpp`:

```cpp
TEST(MetadataBuilder, ArrayIndexAutoGrow)
{
    MetadataBuilder b;
    b[2]["wavelength"].set(std::string("640nm"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Array);
    EXPECT_EQ(m.size(), 3u);                          // grown to index+1
    EXPECT_EQ(m[0].type(), Metadata::Type::Object);   // new slots default to empty Object
    EXPECT_EQ(m[0].size(), 0u);
    EXPECT_EQ(m[1].type(), Metadata::Type::Object);
    EXPECT_EQ(m[1].size(), 0u);
    EXPECT_EQ(m[2]["wavelength"].asString(), "640nm");
}

TEST(MetadataBuilder, MakeArrayIdempotent)
{
    MetadataBuilder b;
    b.makeArray();
    EXPECT_TRUE(b.isArray());

    b[0].set(std::string("first"));
    b.makeArray();                                    // idempotent — must not clobber [0]
    EXPECT_TRUE(b.isArray());

    Metadata m = b.freeze();
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].asString(), "first");
}

TEST(MetadataBuilder, SizeReflectsContainer)
{
    MetadataBuilder b;
    EXPECT_EQ(b.size(), 0u);                          // Null reports 0

    b.makeObject();
    EXPECT_EQ(b.size(), 0u);
    b["k"].set(std::string("v"));
    EXPECT_EQ(b.size(), 1u);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error — `operator[](size_t)`, `makeArray`, `isArray`, `size` undeclared.

- [ ] **Step 3: Add declarations to `metadata.hpp`**

In the `MetadataBuilder` class, just after the existing `operator[](const std::string&)` declaration:

```cpp
        // operator[](index) on a Null node coerces to Array and grows to
        // index+1 (new slots default to empty Objects, not Null, so the
        // common pattern b[i][key].set(v) works without intermediate
        // makeObject() on the new slot). Throws on type mismatch
        // (e.g. operator[](index) on an Object or scalar node).
        MetadataBuilder operator[](size_t index);
```

And after `makeObject()` / `isObject()`:

```cpp
        // Ensures the current node is an empty Array.
        void makeArray();

        bool isArray() const;
        size_t size() const;
```

- [ ] **Step 4: Implement in `metadata.cpp`**

Append after the existing implementations:

```cpp
    MetadataBuilder MetadataBuilder::operator[](size_t index)
    {
        if (m_impl->view->is_null()) {
            *m_impl->view = nlohmann::json::array();
        }
        if (!m_impl->view->is_array()) {
            RAISE_RUNTIME_ERROR << "MetadataBuilder::operator[](index): current node is not an array";
        }
        while (m_impl->view->size() <= index) {
            m_impl->view->push_back(nlohmann::json::object());
        }
        nlohmann::json& child = (*m_impl->view)[index];
        auto childImpl = std::make_shared<Impl>();
        childImpl->root = m_impl->root;
        childImpl->view = &child;
        return MetadataBuilder(std::move(childImpl));
    }

    void MetadataBuilder::makeArray()
    {
        if (!m_impl->view->is_array()) {
            *m_impl->view = nlohmann::json::array();
        }
    }

    bool MetadataBuilder::isArray() const
    {
        return m_impl->view->is_array();
    }

    size_t MetadataBuilder::size() const
    {
        const auto& v = *m_impl->view;
        return (v.is_object() || v.is_array()) ? v.size() : 0u;
    }
```

- [ ] **Step 5: Build and verify the tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 10/10 pass.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/tests/main/test_metadata_builder.cpp
git commit -m "core: add MetadataBuilder Array navigation + makeArray + size"
```

---

## Task 5: Type-mismatch error semantics on `operator[]`

The Task 3/4 implementations already throw on type mismatch via `RAISE_RUNTIME_ERROR`. This task pins that contract down with explicit tests so regressions surface immediately.

**Files:**
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the tests**

Append to `src/tests/main/test_metadata_builder.cpp`:

```cpp
TEST(MetadataBuilder, OperatorKeyOnArrayThrows)
{
    MetadataBuilder b;
    b.makeArray();
    EXPECT_THROW(b["k"], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorKeyOnScalarThrows)
{
    MetadataBuilder b;
    b.set(static_cast<int64_t>(42));
    EXPECT_THROW(b["k"], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorIndexOnObjectThrows)
{
    MetadataBuilder b;
    b.makeObject();
    EXPECT_THROW(b[size_t{0}], slideio::RuntimeError);
}

TEST(MetadataBuilder, OperatorIndexOnScalarThrows)
{
    MetadataBuilder b;
    b.set(true);
    EXPECT_THROW(b[size_t{0}], slideio::RuntimeError);
}
```

- [ ] **Step 2: Build and run the tests**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 14/14 pass — Task 3/4 implementations already throw correctly, so no implementation change is needed.

If any of the four new tests fails, the cause is in the Task 3/4 implementations (`operator[]` should validate type before mutating). Fix there.

- [ ] **Step 3: Commit**

```bash
git add src/tests/main/test_metadata_builder.cpp
git commit -m "core/tests: pin MetadataBuilder operator[] type-mismatch errors"
```

---

## Task 6: Deep-copy at top level + sub-view shared storage + `freeze()` snapshot independence

Spec requires: top-level copies of `MetadataBuilder` are independent; sub-views from `operator[]` share storage with the parent; `freeze()` snapshots the current state and survives subsequent mutation.

The Task 1 default copy/move (`= default`) shares `m_impl` shallowly — wrong for the top-level case. This task fixes that.

**Files:**
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_metadata_builder.cpp`:

```cpp
TEST(MetadataBuilder, TopLevelCopyIsIndependent)
{
    MetadataBuilder a;
    a["k"].set(std::string("a-value"));

    MetadataBuilder b = a;                            // top-level copy
    b["k"].set(std::string("b-value"));

    EXPECT_EQ(a.freeze()["k"].asString(), "a-value"); // a unchanged
    EXPECT_EQ(b.freeze()["k"].asString(), "b-value");
}

TEST(MetadataBuilder, SubViewSharesStorageWithParent)
{
    MetadataBuilder parent;
    MetadataBuilder child = parent["channels"];
    child[0]["wavelength"].set(std::string("488nm"));

    Metadata m = parent.freeze();                     // parent sees the child's writes
    ASSERT_TRUE(m.contains("channels"));
    EXPECT_EQ(m["channels"][0]["wavelength"].asString(), "488nm");
}

TEST(MetadataBuilder, FreezeIsIndependentOfLaterMutation)
{
    MetadataBuilder b;
    b["k"].set(std::string("before"));

    Metadata snapshot = b.freeze();
    b["k"].set(std::string("after"));                 // mutate after freeze

    EXPECT_EQ(snapshot["k"].asString(), "before");    // snapshot unchanged
    EXPECT_EQ(b.freeze()["k"].asString(), "after");
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.TopLevelCopyIsIndependent:MetadataBuilder.SubViewSharesStorageWithParent:MetadataBuilder.FreezeIsIndependentOfLaterMutation'
```
Expected: `TopLevelCopyIsIndependent` FAILS (the default shallow copy shares `m_impl`, so writes through `b` also show up in `a`). The other two SHOULD already pass (sub-view sharing works incidentally, and `freeze()` deep-copies via `nlohmann::json(*view)` in Task 1). If `SubViewSharesStorageWithParent` or `FreezeIsIndependentOfLaterMutation` also fail, the root cause is in Tasks 1/3/4 — fix there.

- [ ] **Step 3: Rewrite the copy/move ops in `metadata.cpp`**

In `src/slideio/core/metadata.cpp`, find the existing `= default` copy/move ops on `MetadataBuilder`:

```cpp
    MetadataBuilder::MetadataBuilder(const MetadataBuilder&) = default;
    MetadataBuilder::MetadataBuilder(MetadataBuilder&&) noexcept = default;
    MetadataBuilder& MetadataBuilder::operator=(const MetadataBuilder&) = default;
    MetadataBuilder& MetadataBuilder::operator=(MetadataBuilder&&) noexcept = default;
```

Replace the copy ops (keep the move ops as `= default`):

```cpp
    MetadataBuilder::MetadataBuilder(const MetadataBuilder& other)
        : m_impl(std::make_shared<Impl>())
    {
        // Deep copy the visible subtree into a fresh root.
        m_impl->root = std::make_shared<nlohmann::json>(*other.m_impl->view);
        m_impl->view = m_impl->root.get();
    }

    MetadataBuilder::MetadataBuilder(MetadataBuilder&&) noexcept = default;

    MetadataBuilder& MetadataBuilder::operator=(const MetadataBuilder& other)
    {
        if (this != &other) {
            auto fresh = std::make_shared<Impl>();
            fresh->root = std::make_shared<nlohmann::json>(*other.m_impl->view);
            fresh->view = fresh->root.get();
            m_impl = std::move(fresh);
        }
        return *this;
    }

    MetadataBuilder& MetadataBuilder::operator=(MetadataBuilder&&) noexcept = default;
```

The semantic: copying a sub-view "promotes" the visible subtree to a new top-level root. This matches user expectation ("I copied my builder, I expect an independent tree") and the spec.

- [ ] **Step 4: Build and verify all three new tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 17/17 pass — all previous tests plus the three new ones.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/core/metadata.cpp src/tests/main/test_metadata_builder.cpp
git commit -m "core: MetadataBuilder top-level copy is deep; sub-views share storage"
```

---

## Task 7: Internal helpers — `detail::builderFromJson` and `detail::makeDefaultMetadataBuilder`

These two helpers bridge nlohmann-json to `MetadataBuilder` for the cases that need it: a driver returning a fully-parsed json (SVSSlide), and the default `buildMetadataTree` implementation that dispatches on `MetadataFormat`.

**Files:**
- Modify: `src/slideio/core/metadata_internal.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/tests/main/test_metadata_builder.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_metadata_builder.cpp` — and add `#include "slideio/core/metadata_internal.hpp"` near the top of the file if it isn't already there:

```cpp
TEST(MetadataBuilder, BuilderFromJsonRoundtrip)
{
    nlohmann::json j = {
        {"channels", nlohmann::json::array({
            {{"wavelength", "488nm"}, {"exposure", "100ms"}},
            {{"wavelength", "561nm"}}
        })}
    };
    MetadataBuilder b = slideio::detail::builderFromJson(j);

    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("channels"));
    EXPECT_EQ(m["channels"].size(), 2u);
    EXPECT_EQ(m["channels"][0]["wavelength"].asString(), "488nm");
    EXPECT_EQ(m["channels"][0]["exposure"].asString(),   "100ms");
    EXPECT_EQ(m["channels"][1]["wavelength"].asString(), "561nm");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderTextFormat)
{
    MetadataBuilder b = slideio::detail::makeDefaultMetadataBuilder(
        "raw text content", slideio::MetadataFormat::Text);
    Metadata m = b.freeze();
    EXPECT_TRUE(m.contains("text"));
    EXPECT_EQ(m["text"].asString(), "raw text content");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderJsonFormat)
{
    MetadataBuilder b = slideio::detail::makeDefaultMetadataBuilder(
        R"({"a": 1, "b": "two"})", slideio::MetadataFormat::JSON);
    Metadata m = b.freeze();
    EXPECT_EQ(m["a"].asInt(), 1);
    EXPECT_EQ(m["b"].asString(), "two");
}

TEST(MetadataBuilder, MakeDefaultMetadataBuilderNoneFormat)
{
    MetadataBuilder b = slideio::detail::makeDefaultMetadataBuilder(
        "", slideio::MetadataFormat::None);
    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_EQ(m.size(), 0u);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
python3 install.py -a build -c release 2>&1 | tail -20
```
Expected: compile error — `detail::builderFromJson` and `detail::makeDefaultMetadataBuilder` undeclared.

- [ ] **Step 3: Add declarations to `metadata_internal.hpp`**

In `src/slideio/core/metadata_internal.hpp`, in the `slideio::detail` namespace, add immediately after the existing `makeMetadataFromJson` declaration:

```cpp
    SLIDEIO_CORE_EXPORTS MetadataBuilder builderFromJson(nlohmann::json root);
    SLIDEIO_CORE_EXPORTS MetadataBuilder makeDefaultMetadataBuilder(
        const std::string& rawMetadata, MetadataFormat fmt);
```

Both helpers return by value; the move constructor handles ownership transfer cheaply.

- [ ] **Step 4: Implement in `metadata.cpp`**

In `src/slideio/core/metadata.cpp`, in the `slideio::detail` namespace block (which already contains `makeMetadataFromJson` and `buildDefaultMetadataTree`), append:

```cpp
        MetadataBuilder builderFromJson(nlohmann::json root)
        {
            auto impl = std::make_shared<MetadataBuilder::Impl>();
            impl->root = std::make_shared<nlohmann::json>(std::move(root));
            impl->view = impl->root.get();
            return MetadataBuilder::fromImpl(std::move(impl));
        }

        MetadataBuilder makeDefaultMetadataBuilder(
            const std::string& rawMetadata, MetadataFormat fmt)
        {
            nlohmann::json root;
            buildDefaultMetadataTree(root, rawMetadata, fmt);
            return builderFromJson(std::move(root));
        }
```

- [ ] **Step 5: Expose a `MetadataBuilder::fromImpl` static factory**

The private `MetadataBuilder(std::shared_ptr<Impl>)` constructor is inaccessible from `detail::builderFromJson`. Mirror the `Metadata::fromImpl` pattern.

In `src/slideio/core/metadata.hpp`, in the `MetadataBuilder` class, in the `public:` section right before `struct Impl;`, add:

```cpp
        static MetadataBuilder fromImpl(std::shared_ptr<Impl> impl);
```

In `src/slideio/core/metadata.cpp`, add the body after the constructor definitions:

```cpp
    MetadataBuilder MetadataBuilder::fromImpl(std::shared_ptr<Impl> impl)
    {
        return MetadataBuilder(std::move(impl));
    }
```

- [ ] **Step 6: Build and verify the tests pass**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter='MetadataBuilder.*'
```
Expected: 21/21 pass.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/slideio/core/metadata_internal.hpp \
        src/tests/main/test_metadata_builder.cpp
git commit -m "core: add detail::builderFromJson and makeDefaultMetadataBuilder"
```

---

## Task 8: Migrate CVScene channel-attribute storage to `MetadataBuilder`

Replace `nlohmann::json m_channelAttributesJson` with `MetadataBuilder m_channelAttrs`. Update all five `setChannelAttribute` overloads to navigate the builder. Update `getChannelAttributes()` to pad and freeze. Drop the `growToChannel` anonymous-namespace helper.

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`

- [ ] **Step 1: Update the storage member in `cvscene.hpp`**

In `src/slideio/core/cvscene.hpp`, find the `protected:` section that currently declares:

```cpp
        nlohmann::json m_channelAttributesJson;
```

Replace with:

```cpp
        MetadataBuilder m_channelAttrs;
```

The `MetadataBuilder` type is declared in `slideio/core/metadata.hpp`, which `cvscene.hpp` already includes (line 10).

- [ ] **Step 2: Rewrite the five `setChannelAttribute` bodies in `cvscene.cpp`**

In `src/slideio/core/cvscene.cpp`, find the anonymous-namespace `growToChannel` helper (added in the channel-metadata refactor) and **delete** it entirely.

Then find the five `setChannelAttribute` overload bodies. Replace **each** of them with this shape (substituting the appropriate value-type parameter):

```cpp
void slideio::CVScene::setChannelAttribute(int channelIndex, const std::string &attributeName, const std::string &attributeValue)
{
    if(channelIndex < 0 || channelIndex >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "Invalid channel index: " << channelIndex
            << " Expected range: [0," << getNumChannels() << ")";
    }
    m_channelAttrs[static_cast<size_t>(channelIndex)][attributeName].set(attributeValue);
}
```

The other four bodies are identical in structure, differing only in the `attributeValue` parameter type (`const char*`, `bool`, `int64_t`, `double`). Apply the same replacement to each.

- [ ] **Step 3: Rewrite `getChannelAttributes()` in `cvscene.cpp`**

Find `CVScene::getChannelAttributes()` (the call_once-based accessor) and replace its body with:

```cpp
const Metadata& CVScene::getChannelAttributes() const
{
    std::call_once(m_channelAttrsOnce, [this]
    {
        const int numChannels = getNumChannels();
        MetadataBuilder padded = m_channelAttrs;             // deep top-level copy
        for (int i = 0; i < numChannels; ++i) {
            padded[static_cast<size_t>(i)].makeObject();      // ensure slot exists
        }
        m_channelAttributesMeta = padded.freeze();
    });
    return m_channelAttributesMeta;
}
```

The deep-copy semantics established in Task 6 mean `padded` is independent of `m_channelAttrs` — padding writes go to the copy, not the stored builder. This mirrors the pre-existing pattern (the old code copied `m_channelAttributesJson` to a local `root`, padded it, then froze that), so `m_channelAttrs` does not need to be `mutable`. The existing `mutable Metadata m_channelAttributesMeta` and `mutable std::once_flag m_channelAttrsOnce` already handle the const-method writes that matter.

`makeObject()` is idempotent on an Object node, so the loop is safe to run regardless of whether channels already have attributes set. The loop both ensures the Array exists (via the first `[]` auto-create on the copy) and pads it up to `numChannels`.

- [ ] **Step 4: Build**

```bash
python3 install.py -a build -c release
```

Expected: build succeeds.

- [ ] **Step 5: Run the channel-attribute test suite**

```bash
./build/release/bin/slideio_tests --gtest_filter='ChannelAttributesTest.*'
```

Expected: all 10 tests pass — same coverage as before the storage swap. The setters now write through `MetadataBuilder`; `getChannelAttributes()` reads through `freeze()`; the public Metadata API surface is unchanged.

- [ ] **Step 6: Run the broader driver/converter tests that exercise channel attributes**

```bash
./build/release/bin/slideio_tests              --gtest_filter='*ChannelAttribut*'
./build/release/bin/slideio_converter_tests    --gtest_filter='*ChannelAttribut*'
./build/release/bin/slideio_ometiff_tests      --gtest_filter='*channelAttributes*'
```

Expected: existing-test-image-present cases pass; absent-image cases produce the same file-not-found result they did before.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp
git commit -m "core: back CVScene channel attributes with MetadataBuilder"
```

---

## Task 9: Migrate `CVScene::buildMetadataTree` to return-style

Signature change on the base class only. No driver currently overrides `CVScene::buildMetadataTree`, so this is a self-contained migration.

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`

- [ ] **Step 1: Change the declaration in `cvscene.hpp`**

In `src/slideio/core/cvscene.hpp`, find the existing declaration (around lines 230–236):

```cpp
        /**@brief Driver hook: convert m_rawMetadata into a JSON tree.
         *
         * The default implementation handles MetadataFormat::{None,Text,JSON,XML}.
         * `rootHandle` is a type-erased pointer to nlohmann::json, valid only for
         * the duration of the call. Override in drivers that need semantic
         * structure; cast via slideio::detail::asJson(rootHandle) inside a .cpp
         * that includes "slideio/core/metadata_internal.hpp". */
        virtual void buildMetadataTree(void* rootHandle) const;
```

Replace with:

```cpp
        /**@brief Driver hook: convert m_rawMetadata into a Metadata tree.
         *
         * The default implementation handles MetadataFormat::{None,Text,JSON,XML}.
         * Drivers that need semantic structure override and return a
         * MetadataBuilder. To wrap an existing nlohmann::json tree, use
         * slideio::detail::builderFromJson from "slideio/core/metadata_internal.hpp".
         */
        virtual MetadataBuilder buildMetadataTree() const;
```

- [ ] **Step 2: Update the implementation in `cvscene.cpp`**

Find `CVScene::buildMetadataTree(void* rootHandle) const` (current body uses `detail::buildDefaultMetadataTree` + `detail::asJson`). Replace with:

```cpp
MetadataBuilder CVScene::buildMetadataTree() const
{
    return detail::makeDefaultMetadataBuilder(m_rawMetadata, m_metadataFormat);
}
```

- [ ] **Step 3: Update `CVScene::getMetadata()` to use the new return-style hook**

In the same file, find `CVScene::getMetadata() const`:

```cpp
const Metadata& CVScene::getMetadata() const
{
    std::call_once(m_metadataOnce, [this]
    {
        nlohmann::json root;
        buildMetadataTree(&root);
        m_metadata = detail::makeMetadataFromJson(std::move(root));
    });
    return m_metadata;
}
```

Replace with:

```cpp
const Metadata& CVScene::getMetadata() const
{
    std::call_once(m_metadataOnce, [this]
    {
        m_metadata = buildMetadataTree().freeze();
    });
    return m_metadata;
}
```

- [ ] **Step 4: Build**

```bash
python3 install.py -a build -c release
```

Expected: build succeeds. No driver overrides `CVScene::buildMetadataTree`, so no downstream signature mismatch is possible.

- [ ] **Step 5: Run the metadata-related tests to confirm `getMetadata()` still works**

```bash
./build/release/bin/slideio_tests --gtest_filter='Metadata.*:MetadataBuilder.*:ChannelAttributesTest.*'
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp
git commit -m "core: switch CVScene::buildMetadataTree to return-style MetadataBuilder"
```

---

## Task 10: Migrate `CVSlide::buildMetadataTree` + SVSSlide override (coupled)

Signature change on the base class plus the one driver that overrides it. These must change in the same commit to keep the build green.

**Files:**
- Modify: `src/slideio/core/cvslide.hpp`
- Modify: `src/slideio/core/cvslide.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.hpp`
- Modify: `src/slideio/drivers/svs/svsslide.cpp`

- [ ] **Step 1: Change the declaration in `cvslide.hpp`**

In `src/slideio/core/cvslide.hpp`, find (around line 83):

```cpp
        virtual void buildMetadataTree(void* rootHandle) const;
```

Replace with:

```cpp
        virtual MetadataBuilder buildMetadataTree() const;
```

- [ ] **Step 2: Update `CVSlide::buildMetadataTree` and `CVSlide::getMetadata` in `cvslide.cpp`**

In `src/slideio/core/cvslide.cpp`, find the existing `CVSlide::buildMetadataTree(void*) const` body:

```cpp
void CVSlide::buildMetadataTree(void* rootHandle) const
{
    detail::buildDefaultMetadataTree(detail::asJson(rootHandle),
                                     m_rawMetadata, m_metadataFormat);
}
```

Replace with:

```cpp
MetadataBuilder CVSlide::buildMetadataTree() const
{
    return detail::makeDefaultMetadataBuilder(m_rawMetadata, m_metadataFormat);
}
```

Then find `CVSlide::getMetadata() const`:

```cpp
const Metadata& CVSlide::getMetadata() const
{
    std::call_once(m_metadataOnce, [this]
    {
        nlohmann::json root;
        buildMetadataTree(&root);
        m_metadata = detail::makeMetadataFromJson(std::move(root));
    });
    return m_metadata;
}
```

Replace with:

```cpp
const Metadata& CVSlide::getMetadata() const
{
    std::call_once(m_metadataOnce, [this]
    {
        m_metadata = buildMetadataTree().freeze();
    });
    return m_metadata;
}
```

- [ ] **Step 3: Update SVSSlide declaration in `svsslide.hpp`**

In `src/slideio/drivers/svs/svsslide.hpp`, find (around line 41):

```cpp
        void buildMetadataTree(void* rootHandle) const override;
```

Replace with:

```cpp
        MetadataBuilder buildMetadataTree() const override;
```

- [ ] **Step 4: Update SVSSlide implementation in `svsslide.cpp`**

In `src/slideio/drivers/svs/svsslide.cpp`, find the existing override:

```cpp
void SVSSlide::buildMetadataTree(void* rootHandle) const
{
    detail::asJson(rootHandle) = SVSTools::parseAperioMetadata(m_rawMetadata);
}
```

Replace with:

```cpp
MetadataBuilder SVSSlide::buildMetadataTree() const
{
    return detail::builderFromJson(SVSTools::parseAperioMetadata(m_rawMetadata));
}
```

Check the top of `svsslide.cpp` — it should already include `slideio/core/metadata_internal.hpp` (the old code used `detail::asJson` from there). No new include is required.

- [ ] **Step 5: Build**

```bash
python3 install.py -a build -c release
```

Expected: build succeeds.

- [ ] **Step 6: Run the metadata and SVS test suites**

```bash
./build/release/bin/slideio_tests --gtest_filter='Metadata.*:MetadataBuilder.*'
./build/release/bin/slideio_tests --gtest_filter='*SVS*'
```

Expected: all `Metadata.*` and `MetadataBuilder.*` pass. SVS tests pass for present test images; absent-image cases produce the same file-not-found they did before.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvslide.hpp src/slideio/core/cvslide.cpp \
        src/slideio/drivers/svs/svsslide.hpp src/slideio/drivers/svs/svsslide.cpp
git commit -m "core+svs: switch CVSlide::buildMetadataTree to MetadataBuilder return-style"
```

---

## Task 11: Remove obsolete `detail::buildDefaultMetadataTree` and `detail::asJson`

After Tasks 8–10 there are no callers of the old `void*`-based path. Delete the obsolete declarations and the implementation.

**Files:**
- Modify: `src/slideio/core/metadata_internal.hpp`
- Modify: `src/slideio/core/metadata.cpp`

- [ ] **Step 1: Audit for remaining callers**

```bash
grep -rn 'detail::buildDefaultMetadataTree\|detail::asJson\|\bbuildDefaultMetadataTree\b\|\basJson\b' /Users/s.melnikov/projects/slideio/slideio/src/
```

Expected: matches only the *declarations* in `metadata_internal.hpp` and the *implementations* in `metadata.cpp`. No call sites.

If any other reference appears, stop — there is an unmigrated path that Tasks 8–10 missed. Report it and migrate first.

- [ ] **Step 2: Delete from `metadata_internal.hpp`**

In `src/slideio/core/metadata_internal.hpp`, delete the declaration of `buildDefaultMetadataTree`:

```cpp
    SLIDEIO_CORE_EXPORTS void buildDefaultMetadataTree(nlohmann::json& root,
                                                      const std::string& rawMetadata,
                                                      MetadataFormat fmt);
```

and the inline `asJson` helper:

```cpp
    inline nlohmann::json& asJson(void* handle)
    {
        return *static_cast<nlohmann::json*>(handle);
    }
```

- [ ] **Step 3: Delete the body from `metadata.cpp`**

In `src/slideio/core/metadata.cpp`, find `buildDefaultMetadataTree` (the function body whose declaration was removed in Step 2). Delete the entire function (the switch over `MetadataFormat` and the JSON/XML/Text/None handling).

The internal call from `makeDefaultMetadataBuilder` (added in Task 7) currently uses `buildDefaultMetadataTree`. Since both live in the same translation unit, that call site has to be updated. Find it in `metadata.cpp`:

```cpp
        MetadataBuilder makeDefaultMetadataBuilder(
            const std::string& rawMetadata, MetadataFormat fmt)
        {
            nlohmann::json root;
            buildDefaultMetadataTree(root, rawMetadata, fmt);
            return builderFromJson(std::move(root));
        }
```

Inline the format-dispatch logic directly (this is the same switch the old `buildDefaultMetadataTree` had):

```cpp
        MetadataBuilder makeDefaultMetadataBuilder(
            const std::string& rawMetadata, MetadataFormat fmt)
        {
            using nlohmann::json;
            json root;
            switch (fmt)
            {
            case MetadataFormat::JSON:
                try { root = json::parse(rawMetadata); }
                catch (...) {
                    root = json{{"#error", "invalid json"}};
                }
                break;
            case MetadataFormat::XML:
                root = xmlStringToJson(rawMetadata);
                break;
            case MetadataFormat::Text:
                root = json{{"text", rawMetadata}};
                break;
            case MetadataFormat::None:
            default:
                root = json::object();
                break;
            }
            return builderFromJson(std::move(root));
        }
```

- [ ] **Step 4: Build**

```bash
python3 install.py -a build -c release
```

Expected: build succeeds. The deleted helpers had no external callers.

- [ ] **Step 5: Re-audit for stale references**

```bash
grep -rn 'detail::buildDefaultMetadataTree\|detail::asJson\|\bbuildDefaultMetadataTree\b\|\basJson\b' /Users/s.melnikov/projects/slideio/slideio/src/
```

Expected: no matches anywhere in `src/`.

- [ ] **Step 6: Run the full metadata test suite to confirm the inlined dispatch still works**

```bash
./build/release/bin/slideio_tests --gtest_filter='Metadata.*:MetadataBuilder.*'
```

Expected: all pass — including the `MakeDefaultMetadataBuilderTextFormat`, `MakeDefaultMetadataBuilderJsonFormat`, and `MakeDefaultMetadataBuilderNoneFormat` tests from Task 7, which exercise the inlined logic directly.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/metadata_internal.hpp src/slideio/core/metadata.cpp
git commit -m "core: drop obsolete buildDefaultMetadataTree and asJson helpers"
```

---

## Task 12: Drop `nlohmann/json.hpp` include from `cvscene.hpp`; flip `nlohmann_json` link to `PRIVATE`

The last step. After this, `nlohmann::json` no longer appears in any public header of `slideio-core`, and downstream consumers of the core do not transitively see it.

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/CMakeLists.txt`

- [ ] **Step 1: Confirm `cvscene.hpp` no longer needs the include**

```bash
grep -n 'nlohmann' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvscene.hpp
```

Expected output: one match — the `#include <nlohmann/json.hpp>` line itself (Task 8 dropped the `m_channelAttributesJson` member; Task 9 dropped the docstring reference to nlohmann in `buildMetadataTree`'s docstring; this Task 12 drops the include).

If there is any reference to `nlohmann::` in the header *other* than the include line, stop and migrate first.

- [ ] **Step 2: Delete the include**

In `src/slideio/core/cvscene.hpp`, find the line:

```cpp
#include <nlohmann/json.hpp>
```

(currently near line 11, alongside other top-of-file includes). Delete it.

- [ ] **Step 3: Flip the CMake link to `PRIVATE`**

In `src/slideio/core/CMakeLists.txt`, find (around line 44):

```cmake
target_link_libraries(${LIBRARY_NAME} PUBLIC nlohmann_json::nlohmann_json)
```

Change to:

```cmake
target_link_libraries(${LIBRARY_NAME} PRIVATE nlohmann_json::nlohmann_json)
```

- [ ] **Step 4: Full build to catch any downstream consumer relying on transitive nlohmann visibility**

```bash
python3 install.py -a build -c release
```

Expected: build succeeds. If any downstream driver or test file is silently relying on transitive nlohmann visibility through `cvscene.hpp`, the build will fail with an "incomplete type `nlohmann::json`" or "no such file or directory" error pointing at the offending file. Fix by adding the explicit `#include <nlohmann/json.hpp>` to that file's own includes (drivers that legitimately use nlohmann should include it themselves, not rely on cvscene.hpp leaking it).

- [ ] **Step 5: Run the full test suite**

```bash
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_ometiff_tests
```

Expected: same pass/skip results as the pre-flight baseline. No new failures.

- [ ] **Step 6: Audit the final state**

```bash
grep -n 'nlohmann' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvscene.hpp \
                   /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvslide.hpp \
                   /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/metadata.hpp
```

Expected: no matches in any of the three public headers.

```bash
grep -n 'PUBLIC nlohmann_json' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/CMakeLists.txt
```

Expected: no match (we just flipped to PRIVATE).

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/CMakeLists.txt
git commit -m "core: drop nlohmann include from public header; link PRIVATE"
```

---

## Final verification

- [ ] **Confirm no nlohmann references in any public core header**

```bash
grep -rn 'nlohmann' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/*.hpp \
                   /Users/s.melnikov/projects/slideio/slideio/src/slideio/slideio/*.hpp \
                   /Users/s.melnikov/projects/slideio/slideio/src/slideio/base/*.hpp 2>/dev/null
```

Expected: zero matches. (The `metadata_internal.hpp` file legitimately includes nlohmann; it is a deliberately-internal header, not a public one.)

- [ ] **Confirm the new API is fully present**

```bash
grep -n 'class.*MetadataBuilder' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/metadata.hpp
grep -n 'getChannelAttributes' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvscene.hpp
grep -n 'MetadataBuilder buildMetadataTree' /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvscene.hpp \
                                            /Users/s.melnikov/projects/slideio/slideio/src/slideio/core/cvslide.hpp
```

Expected: one match each (in their respective files).

- [ ] **Run the full target test set**

```bash
python3 install.py -a build -c release
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_ometiff_tests
```

Expected: green across the board. Same pass/skip pattern as the pre-flight baseline; the public Metadata read API surface is unchanged, so every existing consumer continues to work.
