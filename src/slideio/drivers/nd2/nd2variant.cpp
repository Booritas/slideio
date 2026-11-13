// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2variant.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"

#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

using namespace slideio;
using namespace slideio::nd2;

namespace slideio
{
    namespace nd2
    {
        /// The only way the decoders write into a Variant.
        struct VariantAccess
        {
            using Members = std::vector<std::pair<std::string, Variant>>;
            static Variant scalarBool(bool v) { Variant r; r.m_kind = Variant::Kind::Bool; r.m_int = v ? 1 : 0; return r; }
            static Variant scalarInt(int64_t v) { Variant r; r.m_kind = Variant::Kind::Int; r.m_int = v; return r; }
            static Variant scalarUInt(uint64_t v) { Variant r; r.m_kind = Variant::Kind::UInt; r.m_uint = v; return r; }
            static Variant scalarDouble(double v) { Variant r; r.m_kind = Variant::Kind::Double; r.m_double = v; return r; }
            static Variant scalarString(std::string v) { Variant r; r.m_kind = Variant::Kind::String; r.m_string = std::move(v); return r; }
            static Variant scalarBytes(std::vector<uint8_t> v) { Variant r; r.m_kind = Variant::Kind::Bytes; r.m_bytes = std::move(v); return r; }
            static bool isEmptyContainer(const Variant& v)
            {
                return v.m_kind == Variant::Kind::Map && v.m_members.empty() && v.m_items.empty();
            }
            static Variant listOf(Variant first)
            {
                Variant r;
                r.m_kind = Variant::Kind::List;
                r.m_items.push_back(std::move(first));
                return r;
            }
            static bool isListOf(const Variant& v) { return v.m_kind == Variant::Kind::List; }
            static void append(Variant& list, Variant item) { list.m_items.push_back(std::move(item)); }
            static bool isIndexName(const std::string& n)
            {
                if (n.size() != 11 || n[0] != 'i') return false;
                return std::all_of(n.begin() + 1, n.end(), [](char c) { return c >= '0' && c <= '9'; });
            }
            static std::string indexName(size_t i)
            {
                std::string digits = std::to_string(i);
                if (digits.size() < 10) digits.insert(0, 10 - digits.size(), '0');
                return "i" + digits;
            }
            /// Turns the decoded children of one level into a List (all empty-named)
            /// or a Map (mixed: empty names become iNNNNNNNNNN).
            static Variant finalize(Members&& children)
            {
                Variant r;
                const bool allEmpty = !children.empty() &&
                    std::all_of(children.begin(), children.end(), [](const auto& c) { return c.first.empty(); });
                if (allEmpty) {
                    r.m_kind = Variant::Kind::List;
                    r.m_items.reserve(children.size());
                    for (auto& c : children) r.m_items.push_back(std::move(c.second));
                    return r;
                }
                size_t emptyIndex = 0;
                for (auto& c : children) {
                    if (c.first.empty()) c.first = indexName(emptyIndex++);
                }
                r.m_kind = Variant::Kind::Map;
                r.m_members = std::move(children);
                // Indexed members are stored once and items() views them in place:
                // copying them into m_items doubled the tree at every indexed level.
                r.m_indexed = !r.m_members.empty() &&
                    std::all_of(r.m_members.begin(), r.m_members.end(),
                        [](const auto& m) { return isIndexName(m.first); });
                return r;
            }
        };
    }
}

namespace
{
    constexpr int kMaxDepth = 64;

    [[noreturn]] void fail(const std::string& what)
    {
        RAISE_RUNTIME_ERROR << "ND2: " << what;
    }

    using Members = VariantAccess::Members;

    // ------------------------------------------------------------------ CLX Lite

    struct Cursor
    {
        const uint8_t* data;
        size_t size;     // end of the buffer
        size_t pos;
        size_t* budget;  // inflated bytes still allowed, shared by the whole decode

        size_t remaining() const { return size - pos; }
        void need(size_t n, const char* what) const
        {
            if (n > remaining()) fail(std::string("CLX Lite record truncated in ") + what);
        }
        uint8_t u8(const char* what) { need(1, what); return data[pos++]; }
        uint32_t u32(const char* what)
        {
            need(4, what);
            uint32_t v = 0;
            for (int i = 3; i >= 0; --i) v = (v << 8) | data[pos + i];
            pos += 4;
            return v;
        }
        uint64_t u64(const char* what)
        {
            need(8, what);
            uint64_t v = 0;
            for (int i = 7; i >= 0; --i) v = (v << 8) | data[pos + i];
            pos += 8;
            return v;
        }
    };

    /// Inflates a whole zlib stream. `budget` is what is left of the total inflated
    /// bytes allowed for the whole buffer; it is charged as output is produced, so
    /// nothing is allocated past it.
    std::vector<uint8_t> inflateAll(const uint8_t* src, size_t n, size_t& budget)
    {
        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));
        if (inflateInit(&zs) != Z_OK) fail("cannot initialise zlib");
        zs.next_in = const_cast<Bytef*>(src);
        zs.avail_in = static_cast<uInt>(std::min<size_t>(n, std::numeric_limits<uInt>::max()));
        std::vector<uint8_t> out;
        std::vector<uint8_t> chunk(64 * 1024);
        int rc = Z_OK;
        while (rc != Z_STREAM_END) {
            zs.next_out = chunk.data();
            zs.avail_out = static_cast<uInt>(chunk.size());
            rc = inflate(&zs, Z_NO_FLUSH);
            if (rc != Z_OK && rc != Z_STREAM_END) {
                inflateEnd(&zs);
                fail("compressed CLX Lite record is not valid zlib data");
            }
            const size_t produced = chunk.size() - zs.avail_out;
            if (produced > budget) {
                inflateEnd(&zs);
                fail("compressed CLX Lite data inflates beyond the size limit");
            }
            budget -= produced;
            out.insert(out.end(), chunk.begin(), chunk.begin() + produced);
            if (rc == Z_OK && zs.avail_in == 0 && zs.avail_out != 0) {
                inflateEnd(&zs);
                fail("compressed CLX Lite record is truncated");
            }
        }
        inflateEnd(&zs);
        return out;
    }

    void decodeRecords(Cursor& c, size_t end, int depth, Members& out);

    std::string readName(Cursor& c)
    {
        const uint8_t chars = c.u8("a name length");
        c.need(size_t(chars) * 2, "a name");
        std::u16string u;
        for (size_t i = 0; i < chars; ++i) {
            u.push_back(static_cast<char16_t>(c.data[c.pos + 2 * i] | (c.data[c.pos + 2 * i + 1] << 8)));
        }
        c.pos += size_t(chars) * 2;
        while (!u.empty() && u.back() == u'\0') u.pop_back();
        return Tools::fromUnicode16(u);
    }

    /// Decodes one record starting at c.pos (its type byte); appends to `out`.
    /// `end` is the end of the region the record lives in.
    void decodeRecord(Cursor& c, size_t end, int depth, Members& out)
    {
        if (depth > kMaxDepth) fail("CLX Lite tree is nested too deeply");
        const size_t start = c.pos;
        const uint8_t type = c.u8("a record type");
        if (type == 76) {
            // Compressed: a 2-byte header (type, nameChars), 10 skipped bytes, then
            // zlib to the end of the region: the stream starts at offset 12.
            constexpr size_t kHeader = 12;
            const size_t avail = end - start;
            if (avail < kHeader + 2) fail("compressed CLX Lite record is too short");
            std::vector<uint8_t> raw = inflateAll(c.data + start + kHeader, avail - kHeader, *c.budget);
            c.pos = end;
            Cursor inner{ raw.data(), raw.size(), 0, c.budget };
            decodeRecords(inner, raw.size(), depth + 1, out);
            return;
        }
        std::string name = readName(c);
        switch (type) {
        case 1: out.emplace_back(std::move(name), VariantAccess::scalarBool(c.u8("a bool") != 0)); break;
        case 2: out.emplace_back(std::move(name), VariantAccess::scalarInt(static_cast<int32_t>(c.u32("an int32")))); break;
        case 3: out.emplace_back(std::move(name), VariantAccess::scalarUInt(c.u32("a uint32"))); break;
        case 4: out.emplace_back(std::move(name), VariantAccess::scalarInt(static_cast<int64_t>(c.u64("an int64")))); break;
        case 5: out.emplace_back(std::move(name), VariantAccess::scalarUInt(c.u64("a uint64"))); break;
        case 6: {
            const uint64_t bits = c.u64("a double");
            double d;
            std::memcpy(&d, &bits, sizeof(d));
            out.emplace_back(std::move(name), VariantAccess::scalarDouble(d));
            break;
        }
        case 7: out.emplace_back(std::move(name), VariantAccess::scalarUInt(c.u64("a pointer"))); break;
        case 8: {
            std::u16string u;
            for (;;) {
                c.need(2, "a string");
                const char16_t ch = static_cast<char16_t>(c.data[c.pos] | (c.data[c.pos + 1] << 8));
                c.pos += 2;
                if (ch == u'\0') break;
                u.push_back(ch);
            }
            out.emplace_back(std::move(name), VariantAccess::scalarString(Tools::fromUnicode16(u)));
            break;
        }
        case 9: {
            const uint64_t len = c.u64("a byte array length");
            if (len > c.remaining()) fail("CLX Lite byte array runs past the end of the data");
            std::vector<uint8_t> bytes(c.data + c.pos, c.data + c.pos + static_cast<size_t>(len));
            c.pos += static_cast<size_t>(len);
            out.emplace_back(std::move(name), VariantAccess::scalarBytes(std::move(bytes)));
            break;
        }
        case 10:
        case 11: {
            const uint32_t count = c.u32("a level item count");
            const uint64_t indexOffset = c.u64("a level index offset");
            // type 11: measured from this record's type byte; type 10 (deprecated,
            // never seen): absolute from the start of the buffer
            const uint64_t base = (type == 11) ? start : 0;
            if (indexOffset > c.size || base + indexOffset > c.size) fail("CLX Lite level index offset is out of range");
            const size_t childrenEnd = static_cast<size_t>(base + indexOffset);
            if (childrenEnd < c.pos || childrenEnd > end) fail("CLX Lite level index offset is out of range");
            if (uint64_t(count) * 8 > c.size - childrenEnd) fail("CLX Lite level index table runs past the end of the data");
            Members children;
            decodeRecords(c, childrenEnd, depth + 1, children);
            c.pos = childrenEnd + size_t(count) * 8;
            out.emplace_back(std::move(name), VariantAccess::finalize(std::move(children)));
            break;
        }
        default:
            fail("unknown CLX Lite record type " + std::to_string(int(type)));
        }
    }

    void decodeRecords(Cursor& c, size_t end, int depth, Members& out)
    {
        while (c.pos < end) {
            decodeRecord(c, end, depth, out);
            if (c.pos > end) fail("CLX Lite record runs past the end of its level");
        }
    }

    // ------------------------------------------------------------------- CLX XML

    const std::set<std::string>& listNames()
    {
        static const std::set<std::string> names = {
            "no_name", "BinaryItem", "TextInfoItem", "Wavelength", "MinSrc", "MaxSrc" };
        return names;
    }

    /// The whole text must be one number: "12abc" and "1.#QNAN" are not.
    bool parseDouble(const std::string& text, double& out)
    {
        std::string s = text;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) return false;
        std::istringstream in(s);
        in.imbue(std::locale::classic());   // "." is the decimal point, whatever the process locale
        in >> out;
        return !in.fail() && in.eof();
    }

    /// NaN and infinity as the MSVC runtime and others print them.
    bool parseSpecialDouble(const std::string& text, double& out)
    {
        std::string s = text;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
        s = s.substr(b);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (s == "1.#qnan" || s == "-1.#ind" || s == "nan" || s == "1.#ind" || s == "-1.#qnan" || s == "-nan") {
            out = std::numeric_limits<double>::quiet_NaN();
        }
        else if (s == "1.#inf" || s == "inf" || s == "+inf") out = std::numeric_limits<double>::infinity();
        else if (s == "-1.#inf" || s == "-inf") out = -std::numeric_limits<double>::infinity();
        else return false;
        return true;
    }

    template <typename T>
    bool parseInteger(const std::string& s, T& out)
    {
        const char* b = s.data();
        const char* e = b + s.size();
        if (b != e && *b == '+') ++b;
        auto r = std::from_chars(b, e, out);
        return r.ec == std::errc() && r.ptr == e;
    }

    std::string lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return s;
    }

    Variant scalarFromText(const std::string& runtype, const std::string& text, const std::string& element)
    {
        if (runtype == "bool") {
            const std::string t = lower(text);
            return VariantAccess::scalarBool(t == "true" || t == "1");
        }
        if (runtype == "lx_int32" || runtype == "lx_int64") {
            int64_t v;
            if (!parseInteger(text, v)) fail("bad integer '" + text + "' in element " + element);
            return VariantAccess::scalarInt(v);
        }
        if (runtype == "lx_uint32" || runtype == "lx_uint64") {
            uint64_t v;
            if (!parseInteger(text, v)) fail("bad integer '" + text + "' in element " + element);
            return VariantAccess::scalarUInt(v);
        }
        if (runtype == "double" || runtype == "float") {
            double v;
            if (!parseSpecialDouble(text, v) && !parseDouble(text, v)) v = std::numeric_limits<double>::quiet_NaN();
            return VariantAccess::scalarDouble(v);
        }
        if (runtype == "CLxByteArray") {
            return VariantAccess::scalarBytes(std::vector<uint8_t>(text.begin(), text.end()));   // the raw UTF-8 bytes, as the nd2 package stores them
        }
        return VariantAccess::scalarString(text);   // CLxStringW, unknown, anything else
    }

    bool isScalarRuntype(const std::string& rt)
    {
        return rt == "bool" || rt == "lx_int32" || rt == "lx_uint32" || rt == "lx_int64" ||
            rt == "lx_uint64" || rt == "double" || rt == "float" || rt == "CLxStringW" ||
            rt == "CLxByteArray" || rt == "unknown";
    }

    /// Adds the attributes of `e` as members, as numbers when they read as numbers.
    /// `runtype`, `value`, `_VERSION` and (in legacy mode) `version` are not data.
    void addAttributes(const tinyxml2::XMLElement* e, bool legacy, Members& children)
    {
        for (auto a = e->FirstAttribute(); a; a = a->Next()) {
            const std::string n = a->Name();
            if (n == "runtype" || n == "_VERSION" || n == "value" || (legacy && n == "version")) continue;
            const std::string t = a->Value();
            int64_t i;
            double d;
            if (parseInteger(t, i)) children.emplace_back(n, VariantAccess::scalarInt(i));
            else if (parseDouble(t, d)) children.emplace_back(n, VariantAccess::scalarDouble(d));
            else children.emplace_back(n, VariantAccess::scalarString(t));
        }
    }

    /// `legacy`: every element's attributes are data (legacy JPEG 2000 ND2 boxes such as
    /// `<TextInfoItem Text="..."/>`), not only those of `_VERSION` elements.
    Variant convertElement(const tinyxml2::XMLElement* e, int depth, bool legacy)
    {
        const char* rt = e->Attribute("runtype");
        const char* value = e->Attribute("value");
        const std::string runtype = rt ? rt : "";
        if (value && (isScalarRuntype(runtype) || (runtype.empty() && !e->FirstChildElement()))) {
            return scalarFromText(runtype, value, e->Name());
        }
        Members children;
        for (auto child = e->FirstChildElement(); child; child = child->NextSiblingElement()) {
            if (depth > kMaxDepth * 4) fail("CLX XML tree is nested too deeply");
            const std::string name = child->Name();
            const bool listItem = listNames().count(name) != 0;
            Variant v = convertElement(child, depth + 1, legacy);
            if (listItem && VariantAccess::isEmptyContainer(v)) continue;   // empty list elements are dropped
            children.emplace_back(name == "no_name" ? std::string() : name, std::move(v));
        }
        if (legacy) {
            addAttributes(e, legacy, children);
        }
        // List-name elements are list elements. When nothing else is a sibling the
        // parent is the list (as the empty-named siblings of CLX Lite); otherwise the
        // repeats of one such name are gathered into a List member of that name.
        const bool onlyListNames = !children.empty() && std::all_of(children.begin(), children.end(),
            [](const auto& c) { return c.first.empty() || listNames().count(c.first) != 0; });
        if (onlyListNames) {
            for (auto& c : children) c.first.clear();
        }
        else {
            Members gathered;
            for (auto& c : children) {
                if (c.first.empty() || listNames().count(c.first) == 0) {
                    gathered.push_back(std::move(c));
                    continue;
                }
                auto it = std::find_if(gathered.begin(), gathered.end(), [&](const auto& g) {
                    return g.first == c.first && VariantAccess::isListOf(g.second); });
                if (it == gathered.end()) {
                    gathered.emplace_back(c.first, VariantAccess::listOf(std::move(c.second)));
                }
                else {
                    VariantAccess::append(it->second, std::move(c.second));
                }
            }
            children = std::move(gathered);
        }
        if (!legacy && e->Attribute("_VERSION")) {
            // very old files also keep data in the attributes of the element
            addAttributes(e, legacy, children);
        }
        return VariantAccess::finalize(std::move(children));
    }

    std::string sanitizeXml(const std::string& text)
    {
        std::string s = text;
        // Some files carry control characters, which XML forbids.
        for (char& ch : s) {
            const unsigned char u = static_cast<unsigned char>(ch);
            if (u < 0x20 && u != '\t' && u != '\r' && u != '\n') ch = ' ';
        }
        return s;
    }

    /// The single Map/List child that wraps the content (`SLx...` or `no_name`).
    Variant unwrap(Variant root)
    {
        using Kind = Variant::Kind;
        if (root.kind() == Kind::List && root.items().size() == 1) {
            const Variant& only = root.items()[0];
            if (only.kind() == Kind::Map || only.kind() == Kind::List) return only;
        }
        if (root.kind() == Kind::Map && root.members().size() == 1) {
            const auto& m = root.members()[0];
            if (m.first.compare(0, 3, "SLx") == 0 &&
                (m.second.kind() == Kind::Map || m.second.kind() == Kind::List)) {
                return m.second;
            }
        }
        return root;
    }

    std::string doubleToString(double d)
    {
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
        std::string s;
        for (int precision : {15, 17}) {
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out.precision(precision);
            out << d;
            s = out.str();
            double back;
            if (parseDouble(s, back) && back == d) break;
        }
        return s;
    }
}

// --------------------------------------------------------------------- Variant

const Variant* Variant::get(const std::string& name) const
{
    for (const auto& m : m_members) {
        if (m.first == name) return &m.second;
    }
    return nullptr;
}

const Variant* Variant::path(std::initializer_list<const char*> names) const
{
    const Variant* node = this;
    for (const char* n : names) {
        node = node->get(n);
        if (!node) return nullptr;
    }
    return node;
}

int64_t Variant::asInt(int64_t fallback) const
{
    switch (m_kind) {
    case Kind::Bool:
    case Kind::Int: return m_int;
    case Kind::UInt:
        return m_uint > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
            ? std::numeric_limits<int64_t>::max() : static_cast<int64_t>(m_uint);
    case Kind::Double:
        if (std::isfinite(m_double) && m_double >= -9.2e18 && m_double <= 9.2e18) return static_cast<int64_t>(m_double);
        return fallback;
    default: return fallback;
    }
}

double Variant::asDouble(double fallback) const
{
    switch (m_kind) {
    case Kind::Bool:
    case Kind::Int: return static_cast<double>(m_int);
    case Kind::UInt: return static_cast<double>(m_uint);
    case Kind::Double: return m_double;
    default: return fallback;
    }
}

bool Variant::asBool(bool fallback) const
{
    switch (m_kind) {
    case Kind::Bool:
    case Kind::Int: return m_int != 0;
    case Kind::UInt: return m_uint != 0;
    case Kind::Double: return m_double != 0;
    default: return fallback;
    }
}

std::string Variant::asString() const
{
    switch (m_kind) {
    case Kind::Bool: return m_int ? "true" : "false";
    case Kind::Int: return std::to_string(m_int);
    case Kind::UInt: return std::to_string(m_uint);
    case Kind::Double: return doubleToString(m_double);
    case Kind::String: return m_string;
    default: return {};
    }
}

nlohmann::json Variant::toJson() const
{
    using json = nlohmann::json;
    switch (m_kind) {
    case Kind::Null: return json(nullptr);
    case Kind::Bool: return json(m_int != 0);
    case Kind::Int: return json(m_int);
    case Kind::UInt: return json(m_uint);
    case Kind::Double: return json(m_double);
    case Kind::String: return json(m_string);
    case Kind::Bytes: return json{ {"bytes", m_bytes.size()} };
    case Kind::List: {
        json a = json::array();
        for (const auto& i : m_items) a.push_back(i.toJson());
        return a;
    }
    case Kind::Map: {
        json o = json::object();
        for (const auto& m : m_members) o[m.first] = m.second.toJson();
        return o;
    }
    }
    return json(nullptr);
}

// -------------------------------------------------------------------- decoders

Variant slideio::nd2::decodeClxLite(const uint8_t* data, size_t size, size_t maxInflatedBytes)
{
    if (!data && size) fail("no CLX Lite data");
    size_t budget = maxInflatedBytes;
    Cursor c{ data, size, 0, &budget };
    Members top;
    decodeRecords(c, size, 0, top);
    return VariantAccess::finalize(std::move(top));
}

Variant slideio::nd2::decodeClxXml(const std::string& text)
{
    const std::string clean = sanitizeXml(text);
    tinyxml2::XMLDocument doc;   // tinyxml2 neither expands nor fetches external entities
    if (doc.Parse(clean.data(), clean.size()) != tinyxml2::XML_SUCCESS) {
        fail(std::string("CLX XML does not parse: ") + doc.ErrorStr());
    }
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root || std::string(root->Name()) != "variant") fail("CLX XML root element is not <variant>");
    Variant v = convertElement(root, 0, false);
    return v;
}

Variant slideio::nd2::decodeLegacyXml(const std::vector<uint8_t>& data)
{
    if (data.empty()) fail("legacy XML box is empty");
    size_t first = 0;
    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) first = 3;
    const std::string clean = sanitizeXml(std::string(data.begin() + first, data.end()));
    tinyxml2::XMLDocument doc;   // tinyxml2 neither expands nor fetches external entities
    if (doc.Parse(clean.data(), clean.size()) != tinyxml2::XML_SUCCESS) {
        fail(std::string("legacy XML does not parse: ") + doc.ErrorStr());
    }
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) fail("legacy XML has no root element");
    const tinyxml2::XMLElement* content = root;
    if (std::string(root->Name()) != "variant") {
        // <MetadataSeq>, <CalibrationSeq>, <Metadata_V1.2>... wrap a <variant>; when there
        // is none (<TextInfo>, <Calibration>) the root element is the content.
        if (const tinyxml2::XMLElement* v = root->FirstChildElement("variant")) content = v;
    }
    return unwrap(convertElement(content, 0, true));
}

Variant slideio::nd2::decodeMetadata(const std::vector<uint8_t>& data, size_t maxInflatedBytes)
{
    if (data.empty()) fail("metadata chunk is empty");
    size_t first = 0;
    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) first = 3;
    if (first >= data.size()) fail("metadata chunk holds only a byte order mark");
    if (data[first] == '<') {
        return unwrap(decodeClxXml(std::string(data.begin() + first, data.end())));
    }
    return unwrap(decodeClxLite(data.data(), data.size(), maxInflatedBytes));
}
