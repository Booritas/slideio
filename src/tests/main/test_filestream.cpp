// src/tests/main/test_filestream.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/filestream.hpp"
#include <filesystem>
#include <fstream>

TEST(FileStreamTest, ThrowsOnNonexistent) {
    EXPECT_ANY_THROW(slideio::FileStream("Z:/definitely/does/not/exist.bin"));
}

TEST(FileStreamTest, ReportsCorrectSize) {
    auto path = std::filesystem::temp_directory_path() / "slideio_fs_size.bin";
    { std::ofstream out(path, std::ios::binary); out << "hello world"; }
    // Scope the stream so its OS file handle is released before remove();
    // on Windows an open handle blocks deletion of the file.
    {
        slideio::FileStream s(path.string());
        EXPECT_EQ(s.size(), 11u);
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(FileStreamTest, UriHasFileScheme) {
    auto path = std::filesystem::temp_directory_path() / "slideio_fs_uri.bin";
    { std::ofstream out(path, std::ios::binary); out << "x"; }
    {
        slideio::FileStream s(path.string());
        EXPECT_NE(s.uri().find("file://"), std::string::npos);
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}
