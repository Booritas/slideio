// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The Slidedat.ini reader: GKeyFile-compatible syntax (format description 2,
// design section 4) and locale-independent numbers.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "slideio/drivers/mrxs/mrxsini.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"

using slideio::mrxs::IniFile;

namespace
{
    // Runs f and returns the message of the RuntimeError it throws ("" if none).
    template <typename F>
    std::string errorOf(F f)
    {
        try {
            f();
        }
        catch (const slideio::RuntimeError& e) {
            return e.what();
        }
        return "";
    }

    bool startsWith(const std::string& text, const std::string& prefix)
    {
        return text.compare(0, prefix.size(), prefix) == 0;
    }
}

TEST(MRXSIni, readsGroupsAndKeysWithSpacesAroundTheEquals)
{
    const IniFile ini = IniFile::parse("[GENERAL]\nSLIDE_ID = ABC\nIMAGENUMBER_X=352\n[DATAFILE]\nFILE_0 = Data0000.dat\n");
    ASSERT_NE(nullptr, ini.find("GENERAL", "SLIDE_ID"));
    EXPECT_EQ("ABC", *ini.find("GENERAL", "SLIDE_ID"));
    EXPECT_EQ("352", *ini.find("GENERAL", "IMAGENUMBER_X"));
    EXPECT_EQ("Data0000.dat", *ini.find("DATAFILE", "FILE_0"));
    EXPECT_EQ(nullptr, ini.find("GENERAL", "FILE_0"));
    EXPECT_EQ(nullptr, ini.find("NOPE", "SLIDE_ID"));
}

TEST(MRXSIni, namesAreCaseSensitive)
{
    const IniFile ini = IniFile::parse("[GENERAL]\nSLIDE_ID = ABC\n");
    EXPECT_EQ(nullptr, ini.find("general", "SLIDE_ID"));
    EXPECT_EQ(nullptr, ini.find("GENERAL", "slide_id"));
}

TEST(MRXSIni, stripsAUtf8ByteOrderMark)
{
    const IniFile ini = IniFile::parse("\xEF\xBB\xBF[GENERAL]\nSLIDE_ID = ABC\n");
    ASSERT_NE(nullptr, ini.find("GENERAL", "SLIDE_ID"));
}

TEST(MRXSIni, acceptsCrlfLineEndings)
{
    const IniFile ini = IniFile::parse("[GENERAL]\r\nSLIDE_ID = ABC\r\nX = 1\r\n");
    EXPECT_EQ("ABC", *ini.find("GENERAL", "SLIDE_ID"));
    EXPECT_EQ("1", *ini.find("GENERAL", "X"));
}

TEST(MRXSIni, ignoresLinesWithAnEmptyKey)
{
    // Real MIRAX metadata contains these; OpenSlide removes them before parsing.
    const IniFile ini = IniFile::parse("[GENERAL]\n= stray value\n   =another\nSLIDE_ID = ABC\n");
    EXPECT_EQ("ABC", *ini.find("GENERAL", "SLIDE_ID"));
}

TEST(MRXSIni, ignoresCommentsAndBlankLines)
{
    const IniFile ini = IniFile::parse("# leading comment\n\n[GENERAL]\n  \n# SLIDE_ID = commented\nSLIDE_ID = ABC\n");
    EXPECT_EQ("ABC", *ini.find("GENERAL", "SLIDE_ID"));
}

TEST(MRXSIni, aRepeatedKeyKeepsItsLastValue)
{
    // GKeyFile behaviour, confirmed against OpenSlide 4.0.1 (fixture ini-duplicates).
    const IniFile ini = IniFile::parse("[GENERAL]\nSLIDE_NAME = first\nSLIDE_NAME = last\n");
    EXPECT_EQ("last", *ini.find("GENERAL", "SLIDE_NAME"));
}

TEST(MRXSIni, aRepeatedGroupMergesIntoTheFirst)
{
    const IniFile ini = IniFile::parse("[EXTRA]\nA = a\n[OTHER]\nX = 1\n[EXTRA]\nB = b\n");
    EXPECT_EQ("a", *ini.find("EXTRA", "A"));
    EXPECT_EQ("b", *ini.find("EXTRA", "B"));
    int extras = 0;
    for (const auto& group : ini.groups()) {
        extras += group.name == "EXTRA" ? 1 : 0;
    }
    EXPECT_EQ(1, extras);
}

TEST(MRXSIni, keepsGroupsInFileOrder)
{
    const IniFile ini = IniFile::parse("[B]\nk = 1\n[A]\nk = 2\n[B]\nj = 3\n");
    ASSERT_EQ(2u, ini.groups().size());
    EXPECT_EQ("B", ini.groups()[0].name);
    EXPECT_EQ("A", ini.groups()[1].name);
}

TEST(MRXSIni, rejectsAKeyOutsideAnyGroup)
{
    const std::string message = errorOf([] { IniFile::parse("SLIDE_ID = ABC\n[GENERAL]\n"); });
    EXPECT_TRUE(startsWith(message, "MRXS corrupt:")) << message;
}

TEST(MRXSIni, rejectsALineThatIsNeitherGroupNorKey)
{
    const std::string message = errorOf([] { IniFile::parse("[GENERAL]\nthis is not a key\n"); });
    EXPECT_TRUE(startsWith(message, "MRXS corrupt:")) << message;
}

TEST(MRXSIni, requireNamesTheMissingKey)
{
    const IniFile ini = IniFile::parse("[GENERAL]\nSLIDE_ID = ABC\n");
    EXPECT_EQ("ABC", ini.require("GENERAL", "SLIDE_ID"));
    const std::string message = errorOf([&] { ini.require("GENERAL", "IMAGENUMBER_X"); });
    EXPECT_TRUE(startsWith(message, "MRXS corrupt:")) << message;
    EXPECT_NE(std::string::npos, message.find("IMAGENUMBER_X")) << message;
}

TEST(MRXSIni, readRefusesAFileOverTheSizeLimit)
{
    slideio::TempFile file("ini");
    {
        std::ofstream out(file.getPath(), std::ios::binary);
        out << "[GENERAL]\n";
        out << std::string(IniFile::kMaxBytes, '#');
    }
    const std::string message = errorOf([&] { IniFile::read(file.getPath()); });
    EXPECT_TRUE(startsWith(message, "MRXS limit:")) << message;
}

TEST(MRXSIni, readKeepsTheRawText)
{
    slideio::TempFile file("ini");
    const std::string text = "\xEF\xBB\xBF[GENERAL]\r\nSLIDE_ID = ABC\r\n";
    {
        std::ofstream out(file.getPath(), std::ios::binary);
        out << text;
    }
    const IniFile ini = IniFile::read(file.getPath());
    EXPECT_EQ(text, ini.text());
    EXPECT_EQ("ABC", *ini.find("GENERAL", "SLIDE_ID"));
}

TEST(MRXSIni, parsesIntegers)
{
    EXPECT_EQ(42, slideio::mrxs::parseInteger("42", "X"));
    EXPECT_EQ(-7, slideio::mrxs::parseInteger("-7", "X"));
    EXPECT_EQ(16777215, slideio::mrxs::parseInteger("16777215", "X"));
    for (const char* bad : {"", "4x", "x4", "1.5", "0x10", "99999999999999999999"}) {
        const std::string message = errorOf([&] { slideio::mrxs::parseInteger(bad, "IMAGENUMBER_X"); });
        EXPECT_TRUE(startsWith(message, "MRXS corrupt:")) << "'" << bad << "': " << message;
    }
}

TEST(MRXSIni, parsesRealsWithoutTheLocale)
{
    EXPECT_DOUBLE_EQ(7.5, slideio::mrxs::parseReal("7.5", "X"));
    EXPECT_DOUBLE_EQ(3.71723625557207, slideio::mrxs::parseReal("3.71723625557207", "X"));
    EXPECT_DOUBLE_EQ(0.001, slideio::mrxs::parseReal("1e-3", "X"));
    EXPECT_DOUBLE_EQ(-2.0, slideio::mrxs::parseReal("-2", "X"));
    for (const char* bad : {"", "1,5", "nan", "inf", "7.5px", "--1"}) {
        const std::string message = errorOf([&] { slideio::mrxs::parseReal(bad, "OVERLAP_X"); });
        EXPECT_TRUE(startsWith(message, "MRXS corrupt:")) << "'" << bad << "': " << message;
    }
}
