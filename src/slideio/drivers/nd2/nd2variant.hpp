// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include <nlohmann/json_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace slideio
{
    namespace nd2
    {
        struct VariantAccess;

        /// One node of an ND2 metadata tree, produced by both the CLX Lite (binary)
        /// and the CLX XML decoder. A self-contained value type: it owns everything
        /// below it and can be copied and stored freely (subtrees included).
        ///
        /// Lists: repeated empty-named siblings (Lite) or `no_name`, `BinaryItem`,
        /// `TextInfoItem`, `Wavelength`, `MinSrc`, `MaxSrc` elements (XML) become a
        /// `List`. A Map whose members are all named `i0000000000`, `i0000000001`...
        /// is also addressable by index through items(), so `uLoopPars` wrapped in
        /// `i0000000000` reads the same in both encodings. Such a Map stores each
        /// member once: items() is a view over members(), not a copy of them, so the
        /// tree stays linear in its input however deeply indexed Maps nest.
        class SLIDEIO_ND2_EXPORTS Variant
        {
        public:
            enum class Kind { Null, Bool, Int, UInt, Double, String, Bytes, Map, List };

            /// Read-only view of the elements of a List or the members of an indexed
            /// Map, in order. Valid while the Variant it came from is alive.
            class Items
            {
            public:
                class const_iterator
                {
                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = Variant;
                    using difference_type = std::ptrdiff_t;
                    using pointer = const Variant*;
                    using reference = const Variant&;
                    const_iterator() = default;
                    reference operator*() const { return m_list ? m_list[m_index] : m_map[m_index].second; }
                    pointer operator->() const { return &**this; }
                    const_iterator& operator++() { ++m_index; return *this; }
                    const_iterator operator++(int) { const_iterator t = *this; ++m_index; return t; }
                    bool operator==(const const_iterator& o) const { return m_index == o.m_index; }
                    bool operator!=(const const_iterator& o) const { return m_index != o.m_index; }
                private:
                    friend class Items;
                    const_iterator(const Variant* list, const std::pair<std::string, Variant>* map, size_t index)
                        : m_list(list), m_map(map), m_index(index) {}
                    const Variant* m_list = nullptr;
                    const std::pair<std::string, Variant>* m_map = nullptr;
                    size_t m_index = 0;
                };
                using iterator = const_iterator;

                size_t size() const { return m_size; }
                bool empty() const { return m_size == 0; }
                const Variant& operator[](size_t i) const { return m_list ? m_list[i] : m_map[i].second; }
                const Variant& front() const { return (*this)[0]; }
                const Variant& back() const { return (*this)[m_size - 1]; }
                const_iterator begin() const { return const_iterator(m_list, m_map, 0); }
                const_iterator end() const { return const_iterator(m_list, m_map, m_size); }

            private:
                friend class Variant;
                Items() = default;
                Items(const Variant* list, const std::pair<std::string, Variant>* map, size_t size)
                    : m_list(list), m_map(map), m_size(size) {}
                const Variant* m_list = nullptr;
                const std::pair<std::string, Variant>* m_map = nullptr;
                size_t m_size = 0;
            };

            Kind kind() const { return m_kind; }
            /// Map child by name (the first one if the name repeats); nullptr if absent
            /// or if this is not a Map.
            const Variant* get(const std::string& name) const;
            /// Chained get(); nullptr on any miss.
            const Variant* path(std::initializer_list<const char*> names) const;
            /// List elements (also the indexed members of an `i0000000000` Map);
            /// empty otherwise.
            Items items() const
            {
                if (m_kind == Kind::List) return Items(m_items.data(), nullptr, m_items.size());
                if (m_kind == Kind::Map && m_indexed) return Items(nullptr, m_members.data(), m_members.size());
                return Items();
            }
            /// Map members in document order; empty for other kinds.
            const std::vector<std::pair<std::string, Variant>>& members() const { return m_members; }
            /// Lenient scalars. Numeric kinds convert; Bytes, Map, List, Null and
            /// String return the fallback (asString converts scalars to text).
            int64_t asInt(int64_t fallback = 0) const;
            double asDouble(double fallback = 0) const;
            bool asBool(bool fallback = false) const;
            std::string asString() const;
            const std::vector<uint8_t>& bytes() const { return m_bytes; }
            /// Bytes become {"bytes": length}; a List is an array; a Map an object.
            nlohmann::json toJson() const;

        private:
            friend struct VariantAccess;
            Kind m_kind = Kind::Null;
            int64_t m_int = 0;
            uint64_t m_uint = 0;
            double m_double = 0;
            std::string m_string;
            std::vector<uint8_t> m_bytes;
            std::vector<std::pair<std::string, Variant>> m_members;
            std::vector<Variant> m_items;   // List elements only
            bool m_indexed = false;         // a Map whose members are all iNNNNNNNNNN
        };

        /// Total bytes all compressed (type 76) records of one buffer may inflate to,
        /// nested ones included.
        constexpr size_t kDefaultMaxInflatedBytes = size_t(256) << 20;

        /// Decodes CLX Lite. Throws RuntimeError "ND2: ..." on malformed data.
        SLIDEIO_ND2_EXPORTS Variant decodeClxLite(const uint8_t* data, size_t size,
            size_t maxInflatedBytes = kDefaultMaxInflatedBytes);
        /// Decodes CLX XML (`<variant>` root). Throws RuntimeError "ND2: ...".
        SLIDEIO_ND2_EXPORTS Variant decodeClxXml(const std::string& text);
        /// Decodes the XML box of a legacy JPEG 2000 ND2 file (FD 11.2): an optional
        /// declaration and LIM comment, then a root element of any name. A `<variant>` root,
        /// or the `<variant>` child of another root (`<MetadataSeq>`, `<Metadata_V1.2>`...),
        /// is the content, else the root itself (`<TextInfo>`, `<Calibration>`). Every
        /// element's attributes other than runtype, value, version and _VERSION become
        /// members. The single wrapping `no_name` is unwrapped. Throws RuntimeError "ND2: ...".
        SLIDEIO_ND2_EXPORTS Variant decodeLegacyXml(const std::vector<uint8_t>& data);
        /// '<' as the first byte means XML, anything else Lite. Unwraps the `SLx...`
        /// level (Lite) or the single `no_name` child (XML).
        SLIDEIO_ND2_EXPORTS Variant decodeMetadata(const std::vector<uint8_t>& data,
            size_t maxInflatedBytes = kDefaultMaxInflatedBytes);
    }
}
