// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/pole_lib.hpp"
#include "slideio/core/exceptions.hpp"
#include <functional>
#include <stdexcept>

using namespace slideio;
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4834)
#endif

TEST(ZVIUtils, read_stream_int)
{
    std::string file_path = TestTools::getTestImagePath("zvi","Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto begin = doc.begin();
    auto end = doc.end();
    auto storage = doc.find_storage("/Image");
    ASSERT_TRUE(storage != doc.end());
    auto contents = storage->find_stream("/Image/Contents");
    ASSERT_TRUE(contents != storage->end());

    ZVIUtils::BufferedStream stream(contents->stream());
    ZVIUtils::skipItems(stream, 4);

    int32_t width = ZVIUtils::readIntItem(stream);
    EXPECT_EQ(width, 1480);

    int32_t height = ZVIUtils::readIntItem(stream);
    EXPECT_EQ(height, 1132);

    int32_t depth = ZVIUtils::readIntItem(stream);
    EXPECT_EQ(depth, 0);

    int32_t pixelFormat = ZVIUtils::readIntItem(stream);
    EXPECT_EQ(pixelFormat, 4);

    int32_t rawCount = ZVIUtils::readIntItem(stream);
    EXPECT_EQ(rawCount, 3);
}

TEST(ZVIUtils, read_stream_double)
{
    std::string file_path = TestTools::getTestImagePath("zvi","Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto begin = doc.begin();
    auto end = doc.end();
    auto scaling_storage = doc.find_storage("/Image/Scaling");
    ASSERT_TRUE(scaling_storage != doc.end());
    auto contents_stream = scaling_storage->find_stream("/Image/Scaling/Contents");
    ASSERT_TRUE(contents_stream != scaling_storage->end());
    ZVIUtils::BufferedStream stream(contents_stream->stream());
    ZVIUtils::skipItems(stream, 3);
    double value = ZVIUtils::readDoubleItem(stream);
    ASSERT_DOUBLE_EQ(value, 0.0645);
    int scalingUnits = ZVIUtils::readIntItem(stream);
    ASSERT_EQ(scalingUnits, 76);
}

TEST(ZVIUtils, read_stream_string)
{
    std::string file_path = TestTools::getTestImagePath("zvi","Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto begin = doc.begin();
    auto end = doc.end();
    auto scaling_storage = doc.find_storage("/Image/Scaling");
    ASSERT_TRUE(scaling_storage != doc.end());
    auto contents_stream = scaling_storage->find_stream("/Image/Scaling/Contents");
    ASSERT_TRUE(contents_stream != scaling_storage->end());
    ZVIUtils::BufferedStream stream(contents_stream->stream());
    ZVIUtils::skipItems(stream, 1);
    std::string key = ZVIUtils::readStringItem(stream);
    ASSERT_EQ(key, std::string("Scaling124"));
}

TEST(ZVIUtils, StreamKeeper)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ZVIUtils::StreamKeeper stream(doc, "/Image/Scaling/Contents");
    ZVIUtils::skipItems(stream, 1);
    std::string key = ZVIUtils::readStringItem(stream);
    ASSERT_EQ(key, std::string("Scaling124"));
}

TEST(ZVIUtils, StreamKeeperNegative)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);

    ASSERT_THROW(ZVIUtils::StreamKeeper(doc, "/Image/Scaling1/Contents"), slideio::RuntimeError);
}

TEST(ZVIUtils, readItem)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ZVIUtils::StreamKeeper stream(doc, "/Image/Scaling/Contents");
    ZVIUtils::skipItems(stream, 1);
    auto stringItem = ZVIUtils::readItem(stream);
    std::string tps = std::get<std::string>(stringItem);
    std::string value = std::get<std::string>(stringItem);
    ASSERT_EQ(value, std::string("Scaling124"));
    auto intItem = ZVIUtils::readItem(stream);
    EXPECT_TRUE(std::holds_alternative<int32_t>(intItem));
    int32_t tpi = std::get<int32_t>(intItem);
    ASSERT_TRUE(tpi == 0);
    EXPECT_FALSE(std::holds_alternative<std::string>(intItem));
    EXPECT_THROW(std::get<std::string>(intItem), std::bad_variant_access);
    auto doubleItem = ZVIUtils::readItem(stream);
    EXPECT_TRUE(std::holds_alternative<double>(doubleItem));
    EXPECT_THROW(std::get<int32_t>(doubleItem), std::bad_variant_access);
    EXPECT_THROW(std::get<std::string>(doubleItem), std::bad_variant_access);
    double tpd = std::get<double>(doubleItem);
    EXPECT_DOUBLE_EQ(tpd, 0.0645);

}

// skipItem() must step over exactly as many bytes as readItem() consumes.
// When the two disagree the stream desynchronizes and the *next* readItem()
// interprets payload bytes as a type token, which surfaces far from the real
// cause as "Unsupported item type: <garbage>".
// /Image/Item(0)/Tags/Contents of Zeiss-1-Merged.zvi carries VT_DATE items,
// which skipItem() used to step over as 4 bytes instead of 8.
TEST(ZVIUtils, skipItemConsumesSameBytesAsReadItem)
{
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    ole::compound_document doc(filePath);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper keeper(doc, "/Image/Item(0)/Tags/Contents");
    ZVIUtils::BufferedStream& stream = keeper;

    ZVIUtils::readIntItem(stream); // {Version}
    const int32_t count = ZVIUtils::readIntItem(stream);
    ASSERT_GT(count, 0);

    // Each tag is a (Value, TagID, Attribute) triple of items.
    for (int32_t index = 0; index < count * 3; ++index)
    {
        const std::streamoff start = stream.pos();
        ZVIUtils::readItem(stream);
        const std::streamoff afterRead = stream.pos();
        stream.seek(start, std::ios::beg);
        ZVIUtils::skipItem(stream);
        ASSERT_EQ(stream.pos(), afterRead)
            << "item " << index << " at stream offset " << start;
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "slideio/drivers/zvi/zvitags.hpp"

TEST(ZVITags, getZviTagName_known)
{
    EXPECT_STREQ(slideio::getZviTagName(1537), "Title");
    EXPECT_STREQ(slideio::getZviTagName(1538), "Author");
    EXPECT_STREQ(slideio::getZviTagName(1553), "Filename");
    EXPECT_STREQ(slideio::getZviTagName(769),  "Scale Factor For X");
}

TEST(ZVITags, getZviTagName_unknown)
{
    EXPECT_EQ(slideio::getZviTagName(99999), nullptr);
    EXPECT_EQ(slideio::getZviTagName(0),     nullptr);
}

TEST(ZVIUtils, readAllTags_imageTagsContents)
{
    std::string file_path = TestTools::getTestImagePath("zvi","Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper stream(doc, "/Image/Tags/Contents");
    std::vector<ZVIUtils::ZviTagEntry> entries =
        ZVIUtils::readAllTags(stream, /*hasClsidHeader=*/false);
    ASSERT_FALSE(entries.empty());

    bool hasFilename = false;
    for (const auto& e : entries) {
        if (e.id == static_cast<int32_t>(ZVITAG::ZVITAG_FILE_NAME)) {
            ASSERT_TRUE(std::holds_alternative<std::string>(e.value));
            EXPECT_FALSE(std::get<std::string>(e.value).empty());
            hasFilename = true;
            break;
        }
    }
    EXPECT_TRUE(hasFilename);
}

// POLE signals a short read through its return value and leaves the caller's
// buffer untouched: it neither zeroes the buffer nor advances the position.
// The item readers ignored that, so at the end of a stream readItem() and
// skipItem() decoded an uninitialized stack variable as the item type. That is
// what reached the field as "Unsupported item type: <garbage>" -- a different
// number on every run, never the actual cause -- while readIntItem(), whose
// type variable happens to be zero initialized, reported the same condition as
// "Expected integer. Received:0" (discussion #73).
TEST(ZVIUtils, itemReadersReportEndOfStream)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    ole::compound_document doc(filePath);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper keeper(doc, "/Image/Item(0)/Tags/Contents");
    ZVIUtils::BufferedStream& stream = keeper;

    const std::streamoff size = ZVIUtils::streamSize(stream);
    ASSERT_GT(size, 0);

    auto messageOf = [&](const std::function<void()>& call) -> std::string {
        stream.seek(size, std::ios::beg);
        try {
            call();
        }
        catch (const std::exception& e) {
            return e.what();
        }
        return std::string("<no exception>");
    };

    EXPECT_NE(messageOf([&] { ZVIUtils::readItem(stream); }).find("end of stream"),
              std::string::npos);
    EXPECT_NE(messageOf([&] { ZVIUtils::skipItem(stream); }).find("end of stream"),
              std::string::npos);
    EXPECT_NE(messageOf([&] { ZVIUtils::readIntItem(stream); }).find("end of stream"),
              std::string::npos);
}

TEST(ZVIUtils, streamSizeAndBytesLeft)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    ole::compound_document doc(filePath);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper keeper(doc, "/Image/Item(0)/Tags/Contents");
    ZVIUtils::BufferedStream& stream = keeper;

    const std::streamoff size = ZVIUtils::streamSize(stream);
    ASSERT_GT(size, 0);
    // streamSize() must not disturb the position it was called at.
    EXPECT_EQ(stream.pos(), 0);
    EXPECT_EQ(ZVIUtils::bytesLeft(stream), size);

    ZVIUtils::readIntItem(stream); // {Version}
    EXPECT_EQ(ZVIUtils::bytesLeft(stream), size - 6);

    stream.seek(size, std::ios::beg);
    EXPECT_EQ(ZVIUtils::bytesLeft(stream), 0);
}

// A ZVI tag stream whose declared {Count} exceeds the tags it actually holds
// must yield the tags that are there, not fail the whole file. Bio-Formats'
// ZeissZVIReader.parseTags() bounds its loop the same way.
// The fixture: /Image/Item(0)/Tags/Contents of Zeiss-1-Merged.zvi is 606 bytes
// and starting at offset 20 the next two items are VT_I4 8 and VT_I4 1480, so
// readAllTags() reads {Version}=8, {Count}=1480 with only 83 items -- 27 whole
// (Value, TagID, Attribute) triples -- left in the stream.
TEST(ZVIUtils, readAllTagsStopsAtEndOfStream)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    ole::compound_document doc(filePath);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper keeper(doc, "/Image/Item(0)/Tags/Contents");
    ZVIUtils::BufferedStream& stream = keeper;
    ASSERT_EQ(ZVIUtils::streamSize(stream), 606);

    stream.seek(20, std::ios::beg);
    std::vector<ZVIUtils::ZviTagEntry> entries;
    ASSERT_NO_THROW(entries = ZVIUtils::readAllTags(stream, /*hasClsidHeader=*/false));
    EXPECT_FALSE(entries.empty());
    EXPECT_LE(entries.size(), 27u);
}

// Parsing walks a stream field by field -- readIntItem and friends read a
// handful of bytes at a time. Each of those used to reach the file, because
// pole's positional read path has no buffer under it (TECH_DEBT 26). Reading
// the stream once and parsing from memory is what this asserts; without it the
// count is one file read per field.
TEST(ZVIUtils, parsingAStreamReadsTheFileOnceNotOncePerField)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto storage = doc.find_storage("/Image");
    ASSERT_TRUE(storage != doc.end());
    auto contents = storage->find_stream("/Image/Contents");
    ASSERT_TRUE(contents != storage->end());

    const ole::basic_stream& underlying = contents->stream();
    const unsigned long long before = underlying.read_calls();

    ZVIUtils::BufferedStream stream(underlying);
    ZVIUtils::skipItems(stream, 4);
    for (int i = 0; i < 5; ++i) {
        (void)ZVIUtils::readIntItem(stream);
    }

    const unsigned long long calls = underlying.read_calls() - before;
    // Three, and the number is not arbitrary: BufferedStream issues exactly one
    // read_at, and /Image/Contents is 390 bytes -- a small stream, whose seven
    // 64-byte blocks sit in three distinct big blocks of the container, so one
    // logical read costs three file reads. That is this stream's floor, not a
    // tunable. What the bound excludes is the old behaviour, a file read per
    // parsed field, which for this parse was 14.
    EXPECT_LE(calls, 3u)
        << "parsing issued " << calls << " file reads; before buffering it was 14";
}

// [Item(n)]/<Contents> holds the pixel payload immediately after the header the
// init parser reads, so buffering the whole stream made opening a slide read
// every image in the file and discard it. The bounded constructor refills a
// window instead -- which is only worth having if it parses identically, so
// this compares it against the whole-stream parse field for field.
//
// The window is deliberately absurd: 16 bytes forces a refill several times per
// field, including partway through the length-prefixed position blob, which is
// where an off-by-one in the window arithmetic would show up.
TEST(ZVIUtils, boundedBufferingParsesIdenticallyToWholeStreamBuffering)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto storage = doc.find_storage("/Image");
    ASSERT_TRUE(storage != doc.end());
    auto contents = storage->find_stream("/Image/Contents");
    ASSERT_TRUE(contents != storage->end());
    const ole::basic_stream& underlying = contents->stream();

    auto parse = [&underlying](ZVIUtils::BufferedStream& stream) {
        ZVIUtils::skipItems(stream, 4);
        std::vector<int32_t> values;
        for (int i = 0; i < 5; ++i) {
            values.push_back(ZVIUtils::readIntItem(stream));
        }
        return values;
    };

    ZVIUtils::BufferedStream whole(underlying);
    const std::vector<int32_t> expected = parse(whole);

    ZVIUtils::BufferedStream windowed(underlying, 16);
    const std::vector<int32_t> actual = parse(windowed);

    ASSERT_EQ(expected, actual);
    // size() must keep meaning the whole stream whatever the window holds:
    // streamSize(), bytesLeft() and seek(..., ios::end) are all defined against
    // it, and the tag parsers use them to decide when to stop.
    ASSERT_EQ(whole.size(), windowed.size());
    ASSERT_EQ(underlying.size(), windowed.size());
    ASSERT_EQ(whole.pos(), windowed.pos());
}

// Seeking backwards has to refill, not read out of a window that has moved past
// the target. The parsers seek within a stream (skipExactly, and the tag reader
// rewinding on an unsupported type), so this is a real path and not a synthetic
// one.
TEST(ZVIUtils, boundedBufferingRereadsAfterABackwardSeek)
{
    std::string file_path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(file_path);
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    auto storage = doc.find_storage("/Image");
    ASSERT_TRUE(storage != doc.end());
    auto contents = storage->find_stream("/Image/Contents");
    ASSERT_TRUE(contents != storage->end());

    ZVIUtils::BufferedStream stream(contents->stream(), 16);

    std::vector<char> first(8);
    ASSERT_EQ(8, stream.read(first.data(), 8));

    // Well past the window, then back to the start.
    stream.seek(200, std::ios::beg);
    std::vector<char> middle(8);
    ASSERT_EQ(8, stream.read(middle.data(), 8));

    stream.seek(0, std::ios::beg);
    std::vector<char> again(8);
    ASSERT_EQ(8, stream.read(again.data(), 8));
    ASSERT_EQ(first, again);
}
