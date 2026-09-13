// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.#pragma once
#pragma once

#include "slideio/drivers/zvi/zvi_api_def.hpp"
#include "slideio/drivers/zvi/pole_lib.hpp"
#include "slideio/core/slideio_enums.hpp"
#include <codecvt>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

enum class ZVIPixelFormat;

namespace slideio
{
    namespace ZVIUtils
    {
        typedef enum tagVARENUM
        {
            VT_EMPTY = 0x0000,
            VT_NULL = 0x0001,
            VT_I2 = 0x0002,
            VT_I4 = 0x0003,
            VT_R4 = 0x0004,
            VT_R8 = 0x0005,
            VT_CY = 0x0006,
            VT_DATE = 0x0007,
            VT_BSTR = 0x0008,
            VT_DISPATCH = 0x0009,
            VT_ERROR = 0x000A,
            VT_BOOL = 0x000B,
            VT_VARIANT = 0x000C,
            VT_UNKNOWN = 0x000D,
            VT_DECIMAL = 0x000E,
            VT_I1 = 0x0010,
            VT_UI1 = 0x0011,
            VT_UI2 = 0x0012,
            VT_UI4 = 0x0013,
            VT_I8 = 0x0014,
            VT_UI8 = 0x0015,
            VT_INT = 0x0016,
            VT_UINT = 0x0017,
            VT_VOID = 0x0018,
            VT_HRESULT = 0x0019,
            VT_PTR = 0x001A,
            VT_SAFEARRAY = 0x001B,
            VT_CARRAY = 0x001C,
            VT_USERDEFINED = 0x001D,
            VT_LPSTR = 0x001E,
            VT_LPWSTR = 0x001F,
            VT_RECORD = 0x0024,
            VT_INT_PTR = 0x0025,
            VT_UINT_PTR = 0x0026,
            VT_BLOB = 0x0041,
            VT_STREAM = 0x0042,
            VT_STORAGE = 0x0043,
            VT_STREAMED_OBJECT = 0x0044,
            VT_STORED_OBJECT = 0x0045,
            VT_ARRAY = 0x2000,
            VT_BYREF = 0x4000
        } VARENUM;
        typedef std::variant<std::monostate, bool, int32_t, uint32_t, uint64_t, int64_t, double, std::string> Variant;

        struct ZviTagEntry {
            int32_t id;
            Variant value;
        };

        // A stream read once and then served from memory.
        //
        // The init-time parsers walk a stream field by field -- two bytes of
        // type, then a value -- and pole's positional read path has no buffer
        // beneath it, so each of those reached the file: 14 reads for a
        // nine-field parse. See `software-docs/TECH_DEBT.md` §26, which records
        // the measurement and why the fix belongs here rather than in pole.
        //
        // Init-time parsing only. The pixel path reads whole tiles through
        // ConstStreamKeeper and must keep doing so -- routing it through here
        // would copy every tile an extra time to no purpose.
        class SLIDEIO_ZVI_EXPORTS BufferedStream
        {
        public:
            explicit BufferedStream(const ole::basic_stream& stream);

            // The three operations the parsers use, with pole's semantics
            // preserved exactly, because the parsers depend on them: `read`
            // clamps to what is left and advances by the count it returns, and
            // `seek` refuses a target outside the stream and leaves the
            // position where it was. Both report only through the return value.
            std::streamsize read(char* buffer, std::streamsize size);
            std::streamoff seek(std::streamoff offset, std::ios::seekdir way);
            std::streamoff pos() const { return m_pos; }
            std::streamoff size() const { return static_cast<std::streamoff>(m_data.size()); }

        private:
            std::vector<uint8_t> m_data;
            std::streamoff m_pos = 0;
        };

        // Reads a ZVI Tags stream:
        //   - hasClsidHeader=true:  16 raw bytes (CLSID) precede {Version}{Count}.
        //                           Use this for the root-level <Tags> stream.
        //   - hasClsidHeader=false: stream starts directly with {Version}{Count}.
        //                           Use this for [Image]/[Tags]/<Contents> and
        //                           [Item(n)]/[Tags]/<Contents>.
        // Returns one ZviTagEntry per (Value, TagID, Attribute) triple. Entries
        // whose Value variant is monostate (VT_EMPTY / unsupported types) are
        // dropped. {Attribute} is consumed and discarded.
        std::vector<ZviTagEntry> SLIDEIO_ZVI_EXPORTS readAllTags(
            BufferedStream& stream, bool hasClsidHeader);

        // Total size of `stream` in bytes. The position is left where it was.
        std::streamoff SLIDEIO_ZVI_EXPORTS streamSize(BufferedStream& stream);
        // Bytes between the current position of `stream` and its end. POLE
        // neither zeroes the destination nor advances the position on a read
        // that runs past the end, so a loop over a stream has to check what is
        // left before reading rather than inspect the result afterwards.
        std::streamoff SLIDEIO_ZVI_EXPORTS bytesLeft(BufferedStream& stream);

        // Reads exactly `size` bytes or throws. POLE reports a short read only
        // through its return value: it leaves the destination buffer untouched
        // and does not advance the position, so an unchecked read at the end of
        // a stream hands back whatever the destination already held.
        void SLIDEIO_ZVI_EXPORTS readExactly(BufferedStream& stream, void* buffer,
                                             std::streamsize size);

        void SLIDEIO_ZVI_EXPORTS skipItem(BufferedStream& stream);
        void SLIDEIO_ZVI_EXPORTS skipItems(BufferedStream& stream, int count);
        int32_t SLIDEIO_ZVI_EXPORTS readIntItem(BufferedStream& stream);
        double SLIDEIO_ZVI_EXPORTS readDoubleItem(BufferedStream& stream);
        std::string SLIDEIO_ZVI_EXPORTS readStringItem(BufferedStream& stream);
        Variant SLIDEIO_ZVI_EXPORTS readItem(BufferedStream& stream, bool skipUnusedTypes = true);
        slideio::DataType dataTypeFromPixelFormat(const ZVIPixelFormat pixel_format);
        int channelCountFromPixelFormat(ZVIPixelFormat pixelFormat);

        // Resolves a stream path and buffers the stream for parsing. Converts to
        // a BufferedStream rather than to the pole stream itself, so the
        // parsers it feeds read from memory; see BufferedStream above for why.
        class SLIDEIO_ZVI_EXPORTS StreamKeeper
        {
        public:
            StreamKeeper(ole::compound_document& doc, const std::string& path);
            operator BufferedStream& () {
                return m_buffer;
            }
            BufferedStream* operator ->() {
                return &m_buffer;
            }
        private:
            BufferedStream m_buffer;
        };

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
    }
}
#if defined(_MSC_VER)
#pragma warning( pop )
#endif
