// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/uridispatcher.hpp"

#include <filesystem>
#include <fstream>

using slideio::detectUriScheme;
using slideio::UriScheme;

TEST(UriDispatcherTest, DetectsLocalPaths) {
    EXPECT_EQ(detectUriScheme("/abs/path/file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("C:\\Users\\foo\\file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("relative/file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("file:///abs/path/file.svs"), UriScheme::LocalFile);
}

TEST(UriDispatcherTest, DetectsS3) {
    EXPECT_EQ(detectUriScheme("s3://bucket/key/file.svs"), UriScheme::S3);
    EXPECT_EQ(detectUriScheme("S3://bucket/key.svs"), UriScheme::S3);
}

TEST(UriDispatcherTest, DetectsHttp) {
    EXPECT_EQ(detectUriScheme("http://host/p"), UriScheme::Http);
    EXPECT_EQ(detectUriScheme("https://host/p?X-Amz-Signature=abc"), UriScheme::Http);
}

TEST(UriDispatcherTest, TranslatesS3ToHttps) {
    EXPECT_EQ(slideio::s3UriToHttps("s3://mybucket/path/to/slide.svs"),
              "https://mybucket.s3.amazonaws.com/path/to/slide.svs");
}

TEST(UriDispatcherTest, CreateStreamReturnsFileStreamForPath) {
    auto path = std::filesystem::temp_directory_path() / "slideio_ud_create.bin";
    { std::ofstream out(path, std::ios::binary); out << "hello world"; }
    auto stream = slideio::createStream(path.string());
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->size(), 11u);
    // Release the stream before removing: on Windows an open file handle
    // blocks deletion.
    stream.reset();
    std::filesystem::remove(path);
}

TEST(UriDispatcherTest, UriResourceNameStripsSchemeAndQuery) {
    EXPECT_EQ(slideio::uriResourceName("s3://bucket/dir/slide.svs"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("https://h/p/slide.svs?sig=x"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("/abs/dir/slide.svs"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("C:\\dir\\slide.svs"), "slide.svs");
}

TEST(UriDispatcherTest, SiblingUri) {
    // Resolve a referenced file next to a base URI, preserving the scheme/prefix.
    // Used by AFI (index -> referenced SVS) and OME-TIFF (image -> companion XML).
    EXPECT_EQ(slideio::siblingUri("http://h/dir/a.svs", "b.svs"), "http://h/dir/b.svs");
    EXPECT_EQ(slideio::siblingUri("s3://bucket/dir/a.afi", "b.svs"), "s3://bucket/dir/b.svs");
    EXPECT_EQ(slideio::siblingUri("/abs/dir/a.afi", "b.svs"), "/abs/dir/b.svs");
    EXPECT_EQ(slideio::siblingUri("C:\\dir\\a.afi", "b.svs"), "C:\\dir\\b.svs");
    // A query string on the base is dropped before resolving the sibling.
    EXPECT_EQ(slideio::siblingUri("https://h/dir/a.afi?sig=x", "b.svs"), "https://h/dir/b.svs");
    // A name that already carries a scheme is returned unchanged.
    EXPECT_EQ(slideio::siblingUri("s3://bucket/dir/a.afi", "https://other/x.svs"),
              "https://other/x.svs");
}
