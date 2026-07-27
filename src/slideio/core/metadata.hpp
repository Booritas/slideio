// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/slideio_core_def.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace slideio
{
    /**@brief Read-only tree view over slide/scene metadata.
     *
     * Built lazily by CVScene / CVSlide on first call to getMetadata(). Children
     * returned by operator[] / find() are lightweight views into the same root
     * tree and share its lifetime.
     */
    class SLIDEIO_CORE_EXPORTS Metadata
    {
    public:
        enum class Type
        {
            Null,
            Bool,
            Int,
            Double,
            String,
            Array,
            Object
        };

        Metadata(); // Null node
        ~Metadata();
        Metadata(const Metadata&);
        Metadata(Metadata&&) noexcept;
        Metadata& operator=(const Metadata&);
        Metadata& operator=(Metadata&&) noexcept;

        Type type() const;
        bool isNull() const { return type() == Type::Null; }
        bool isObject() const { return type() == Type::Object; }
        bool isArray() const { return type() == Type::Array; }

        bool asBool() const;
        int64_t asInt() const;
        double asDouble() const;
        std::string asString() const;

        size_t size() const;
        bool contains(const std::string& key) const;
        Metadata operator[](const std::string& key) const;
        Metadata operator[](size_t index) const;
        Metadata find(const std::string& jsonPointer) const;
        std::vector<std::string> keys() const;

        std::string toJson(int indent = -1) const;

        struct Impl;
        static Metadata fromImpl(std::shared_ptr<const Impl> impl);

    private:
        explicit Metadata(std::shared_ptr<const Impl> impl);
        std::shared_ptr<const Impl> m_impl;
    };

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
        void set(bool value);
        void set(int64_t value);
        void set(double value);
        void set(const char* value);

        // Navigation. Returns a sub-builder sharing root storage with this one.
        // Auto-creates and coerces: operator[](key) on a Null node turns it into
        // an Object; on an Object node it returns a sub-view (auto-creating the
        // key with Null if absent). Throws slideio::RuntimeError on type mismatch
        // (e.g. operator[](key) on an Array or scalar node).
        MetadataBuilder operator[](const std::string& key);

        // operator[](index) on a Null node coerces to Array and grows to
        // index+1 (new slots default to empty Objects, not Null, so the
        // common pattern b[i][key].set(v) works without intermediate
        // makeObject() on the new slot). Throws on type mismatch
        // (e.g. operator[](index) on an Object or scalar node).
        MetadataBuilder operator[](size_t index);

        // Ensures the current node is an Object. Idempotent if already
        // an Object (content preserved); replaces a scalar/Null/Array otherwise.
        void makeObject();

        // Ensures the current node is an Array. Idempotent if already
        // an Array (content preserved); replaces a scalar/Null/Object otherwise.
        void makeArray();

        // Inspection.
        bool isNull() const;
        bool isObject() const;
        bool isArray() const;
        size_t size() const;

        // Snapshot the current state into an immutable Metadata.
        Metadata freeze() const;

        struct Impl;
        static MetadataBuilder fromImpl(std::shared_ptr<Impl> impl);

    private:
        explicit MetadataBuilder(std::shared_ptr<Impl> impl);
        std::shared_ptr<Impl> m_impl;
    };
} // namespace slideio

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
