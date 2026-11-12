// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if defined(WIN32)
#include <windows.h>
#include <winioctl.h>
#else
#include <fstream>
#endif
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    std::string fixture(const char* name)
    {
        return std::string(ND2_TEST_DATA_DIR) + "/" + name;
    }

    ChunkFile readFixture(const char* name)
    {
        FileReader file(fixture(name));
        return ChunkFile::read(file);
    }

    /// The first bytes of a chunk's name field.
    std::string nameFieldStart(const char* fixtureName, const ChunkHeader& h, size_t n)
    {
        FileReader file(fixture(fixtureName));
        std::string s(n, '\0');
        file.readAt(h.offset + 16, &s[0], n);
        return s;
    }

    // ---- a sparse file with a chunk above 4 GiB ------------------------------------

    void put32(std::vector<uint8_t>& b, uint32_t v)
    {
        for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
    }

    void put64(std::vector<uint8_t>& b, uint64_t v)
    {
        for (int i = 0; i < 8; ++i) b.push_back(uint8_t(v >> (8 * i)));
    }

    void putName(std::vector<uint8_t>& b, const std::string& name, size_t fieldSize)
    {
        b.insert(b.end(), name.begin(), name.end());
        b.resize(b.size() + (fieldSize - name.size()), 0);
    }

    /// One chunk: header, name field of `nameLength` bytes, data.
    std::vector<uint8_t> chunk(const std::string& name, uint32_t nameLength, const std::vector<uint8_t>& data)
    {
        std::vector<uint8_t> b;
        put32(b, 0x0ABECEDA);
        put32(b, nameLength);
        put64(b, data.size());
        putName(b, name, nameLength);
        b.insert(b.end(), data.begin(), data.end());
        return b;
    }

    /// Writes bytes at an absolute offset of a (sparse) file; false if it cannot.
    class SparseWriter
    {
    public:
        explicit SparseWriter(const std::string& path)
        {
#if defined(WIN32)
            m_handle = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_handle != INVALID_HANDLE_VALUE) {
                DWORD returned = 0;
                if (!DeviceIoControl(m_handle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr)) {
                    CloseHandle(m_handle);
                    m_handle = INVALID_HANDLE_VALUE;
                }
            }
#else
            m_file.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
#endif
        }
        ~SparseWriter()
        {
#if defined(WIN32)
            if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);
#endif
        }
        bool ok() const
        {
#if defined(WIN32)
            return m_handle != INVALID_HANDLE_VALUE;
#else
            return m_file.good();
#endif
        }
        bool writeAt(uint64_t offset, const std::vector<uint8_t>& bytes)
        {
#if defined(WIN32)
            LARGE_INTEGER pos;
            pos.QuadPart = static_cast<LONGLONG>(offset);
            if (!SetFilePointerEx(m_handle, pos, nullptr, FILE_BEGIN)) return false;
            DWORD written = 0;
            return WriteFile(m_handle, bytes.data(), DWORD(bytes.size()), &written, nullptr)
                && written == bytes.size();
#else
            m_file.seekp(static_cast<std::streamoff>(offset));
            m_file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
            m_file.flush();
            return m_file.good();
#endif
        }
    private:
#if defined(WIN32)
        HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
        std::ofstream m_file;
#endif
    };
}

TEST(Nd2ChunkFile, ReadsVersion3)
{
    const ChunkFile c = readFixture("v3_basic.nd2");
    EXPECT_EQ(c.versionMajor(), 3);
    EXPECT_EQ(c.versionMinor(), 0);
    EXPECT_NE(c.find("ImageAttributesLV!"), nullptr);
    EXPECT_EQ(c.find("NoSuchChunk!"), nullptr);
    ASSERT_EQ(c.frames().size(), 1u);
    const ChunkHeader& f = c.frames().at(0);
    EXPECT_EQ(f.nameLength, 4072u);
    EXPECT_EQ(f.dataOffset() % 4096, 4088u);
    EXPECT_FALSE(c.recoveredByScan());
    EXPECT_EQ(c.badMapEntries(), 0);
}

TEST(Nd2ChunkFile, UnpaddedNames)
{
    const ChunkFile c = readFixture("v20_unpadded.nd2");
    EXPECT_EQ(c.versionMajor(), 2);
    EXPECT_EQ(c.versionMinor(), 0);
    ASSERT_TRUE(c.frames().count(0));
    EXPECT_EQ(c.frames().at(0).nameLength, std::strlen("ImageDataSeq|0!"));
    EXPECT_FALSE(c.recoveredByScan());
}

TEST(Nd2ChunkFile, UnalignedChunks)
{
    const ChunkFile c = readFixture("v21_xml.nd2");
    EXPECT_EQ(c.versionMajor(), 2);
    EXPECT_EQ(c.versionMinor(), 1);
    EXPECT_EQ(c.frames().size(), 6u);
    EXPECT_FALSE(c.recoveredByScan());
    EXPECT_EQ(c.badMapEntries(), 0);
    for (const auto& kv : c.frames()) {
        const std::string expected = "ImageDataSeq|" + std::to_string(kv.first) + "!";
        EXPECT_EQ(nameFieldStart("v21_xml.nd2", kv.second, expected.size()), expected);
    }
}

TEST(Nd2ChunkFile, StaleMapRecovered)
{
    const ChunkFile c = readFixture("stale_map.nd2");
    EXPECT_TRUE(c.recoveredByScan());
    EXPECT_EQ(c.badMapEntries(), 2);
    ASSERT_EQ(c.frames().size(), 5u);
    for (const auto& kv : c.frames()) {
        const std::string expected = "ImageDataSeq|" + std::to_string(kv.first) + "!";
        EXPECT_EQ(nameFieldStart("stale_map.nd2", kv.second, expected.size()), expected);
    }
    EXPECT_NE(c.find("ImageAttributesLV!"), nullptr);
}

TEST(Nd2ChunkFile, TruncatedChunkAbsent)
{
    const ChunkFile c = readFixture("truncated_frame.nd2");
    ASSERT_EQ(c.frames().size(), 2u);
    EXPECT_TRUE(c.frames().count(0));
    EXPECT_TRUE(c.frames().count(1));
    EXPECT_FALSE(c.frames().count(2));
    EXPECT_EQ(c.find("ImageDataSeq|2!"), nullptr);
}

TEST(Nd2ChunkFile, RejectsNonNd2)
{
    try {
        readFixture("not_nd2.nd2");
        FAIL() << "expected an exception";
    }
    catch (const std::exception& ex) {
        EXPECT_NE(std::string(ex.what()).find("ND2: not a Nikon ND2 file"), std::string::npos) << ex.what();
    }
}

TEST(Nd2ChunkFile, ReadDataReturnsChunkPayload)
{
    FileReader file(fixture("v3_basic.nd2"));
    const ChunkFile c = ChunkFile::read(file);
    const ChunkHeader* attrs = c.find("ImageAttributesLV!");
    ASSERT_NE(attrs, nullptr);
    EXPECT_EQ(c.readData(file, *attrs).size(), attrs->dataLength);
}

TEST(Nd2ChunkFile, OffsetsAbove4GiB)
{
    const uint64_t frameOffset = 5ULL << 30;
    const std::string name = "ImageDataSeq|0!";

    std::vector<uint8_t> signatureData(64, 0);
    std::memcpy(signatureData.data(), "Ver3.0", 6);
    const auto signature = chunk("ND2 FILE SIGNATURE CHUNK NAME01!", 32, signatureData);

    // 8-byte time prefix, then 16 pixels.
    std::vector<uint8_t> frameData(8, 0);
    for (int i = 0; i < 16; ++i) frameData.push_back(uint8_t(100 + i));
    const auto frame = chunk(name, 4072, frameData);

    const uint64_t mapOffset = frameOffset + frame.size();
    std::vector<uint8_t> mapData;
    putName(mapData, "ND2 FILE SIGNATURE CHUNK NAME01!", 32);
    put64(mapData, 0);
    put64(mapData, signatureData.size());
    mapData.insert(mapData.end(), name.begin(), name.end());
    put64(mapData, frameOffset);
    put64(mapData, frameData.size());
    putName(mapData, "ND2 CHUNK MAP SIGNATURE 0000001!", 32);   // terminating entry = trailer
    put64(mapData, mapOffset);
    const auto map = chunk("ND2 FILEMAP SIGNATURE NAME 0001!", 32, mapData);

    TempFile temp(".nd2");
    {
        SparseWriter w(temp.getPath().string());
        if (!w.ok()) {
            GTEST_SKIP() << "the filesystem refuses sparse files";
        }
        if (!w.writeAt(0, signature) || !w.writeAt(frameOffset, frame) || !w.writeAt(mapOffset, map)) {
            GTEST_SKIP() << "cannot write a sparse file above 4 GiB here";
        }
    }

    FileReader file(temp.getPath().string());
    const ChunkFile c = ChunkFile::read(file);
    EXPECT_FALSE(c.recoveredByScan());
    ASSERT_EQ(c.frames().size(), 1u);
    const ChunkHeader& f = c.frames().at(0);
    EXPECT_EQ(f.offset, 5ULL << 30);
    const std::vector<uint8_t> data = c.readData(file, f);
    ASSERT_EQ(data.size(), frameData.size());
    EXPECT_EQ(data, frameData);
}

// A scan that runs (here: the trailer is damaged, so the map is unreadable) must not
// accept a chunk whose data runs past EOF either.
TEST(Nd2ChunkFile, ScanSkipsTruncatedChunk)
{
    std::vector<uint8_t> bytes;
    {
        FileReader src(fixture("truncated_frame.nd2"));
        bytes.resize(static_cast<size_t>(src.size()));
        src.readAt(0, bytes.data(), bytes.size());
    }
    bytes[bytes.size() - 40] ^= 0xFF;   // break the trailer signature
    TempFile temp(".nd2");
    {
        SparseWriter w(temp.getPath().string());
        ASSERT_TRUE(w.ok());
        ASSERT_TRUE(w.writeAt(0, bytes));
    }
    FileReader file(temp.getPath().string());
    const ChunkFile c = ChunkFile::read(file);
    EXPECT_TRUE(c.recoveredByScan());
    ASSERT_EQ(c.frames().size(), 2u);
    EXPECT_TRUE(c.frames().count(0));
    EXPECT_TRUE(c.frames().count(1));
    EXPECT_NE(c.find("ImageAttributesLV!"), nullptr);
}

TEST(Nd2ChunkFile, RejectsUnsupportedMajor)
{
    std::vector<uint8_t> bytes;
    {
        FileReader src(fixture("v3_basic.nd2"));
        bytes.resize(static_cast<size_t>(src.size()));
        src.readAt(0, bytes.data(), bytes.size());
    }
    // signature chunk: 16-byte header + 32-byte name, then "Ver3.0"
    ASSERT_EQ(std::memcmp(&bytes[48], "Ver3.0", 6), 0);
    bytes[51] = '4';
    TempFile temp(".nd2");
    {
        SparseWriter w(temp.getPath().string());
        ASSERT_TRUE(w.ok());
        ASSERT_TRUE(w.writeAt(0, bytes));
    }
    FileReader file(temp.getPath().string());
    try {
        ChunkFile::read(file);
        FAIL() << "expected an exception";
    }
    catch (const std::exception& ex) {
        EXPECT_NE(std::string(ex.what()).find("unsupported ND2 version 4.0"), std::string::npos) << ex.what();
    }
}
