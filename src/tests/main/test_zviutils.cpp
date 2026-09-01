// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/pole_lib.hpp"
#include "slideio/core/exceptions.hpp"
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

    ole::basic_stream stream = contents->stream();
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
    ZVIUtils::skipItems(contents_stream->stream(), 3);
    double value = ZVIUtils::readDoubleItem(contents_stream->stream());
    ASSERT_DOUBLE_EQ(value, 0.0645);
    int scalingUnits = ZVIUtils::readIntItem(contents_stream->stream());
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
    ZVIUtils::skipItems(contents_stream->stream(), 1);
    std::string key = ZVIUtils::readStringItem(contents_stream->stream());
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    ole::compound_document doc(filePath);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper keeper(doc, "/Image/Item(0)/Tags/Contents");
    ole::basic_stream& stream = keeper;

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
