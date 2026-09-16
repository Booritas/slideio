// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <filesystem>
#include <string>

using namespace slideio;

namespace
{
    // A private copy of a real tiff, so a test can open and close a handle on it
    // without disturbing the shared test image. Closure is observed with
    // TestTools::isFileHeldOpen, which asks whether this process still holds a
    // descriptor on the file -- see testtools.cpp for why the earlier "try to delete
    // it" probe only worked on Windows.
    //
    // Scoped so the copy is removed when the test ends: unlike the delete probe it
    // replaces, isFileHeldOpen leaves the file in place, so each test cleans up after
    // itself rather than leaving multi-megabyte copies in the temp directory.
    class TempTiff
    {
    public:
        explicit TempTiff(const char* name)
            : m_path(std::filesystem::temp_directory_path() / name)
        {
            const std::string source = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
            std::filesystem::copy_file(source, m_path, std::filesystem::copy_options::overwrite_existing);
        }
        ~TempTiff() {
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
        }
        TempTiff(const TempTiff&) = delete;
        TempTiff& operator=(const TempTiff&) = delete;

        std::string string() const { return m_path.string(); }
        bool isHeldOpen() const { return TestTools::isFileHeldOpen(m_path.string()); }

    private:
        std::filesystem::path m_path;
    };
}

TEST(TIFFKeeper, destructorClosesTheHandle) {
    const TempTiff file("tiffkeeper-dtor.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        ASSERT_TRUE(keeper.isValid());
        EXPECT_TRUE(file.isHeldOpen()) << "the keeper holds the handle open";
    }
    EXPECT_FALSE(file.isHeldOpen()) << "the destructor must close the handle";
}

TEST(TIFFKeeper, releaseGivesUpOwnershipWithoutClosing) {
    const TempTiff file("tiffkeeper-release.tif");
    libtiff::TIFF* handle = nullptr;
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        handle = keeper.release();
        EXPECT_FALSE(keeper.isValid()) << "release leaves the keeper empty";
        EXPECT_TRUE(handle != nullptr);
    }
    EXPECT_TRUE(file.isHeldOpen()) << "release must NOT close: the caller owns it now";
    TiffTools::closeTiffFile(handle);
    EXPECT_FALSE(file.isHeldOpen());
}

// The defect the debt entry names: assigning a new handle over a held one dropped the
// old handle without closing it.
TEST(TIFFKeeper, resetClosesTheHandleItReplaces) {
    const TempTiff first("tiffkeeper-reset-first.tif");
    const TempTiff second("tiffkeeper-reset-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.reset(TiffTools::openTiffFile(second.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(first.isHeldOpen()) << "reset must close the handle it replaced";
        EXPECT_TRUE(second.isHeldOpen()) << "and must hold on to the one it took";
    }
    EXPECT_FALSE(second.isHeldOpen());
}

// The same defect by its other door, which the debt entry does not record.
TEST(TIFFKeeper, openTiffFileClosesTheHandleItReplaces) {
    const TempTiff first("tiffkeeper-open-first.tif");
    const TempTiff second("tiffkeeper-open-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.openTiffFile(second.string(), true);
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(first.isHeldOpen()) << "openTiffFile must close the handle it replaced";
        EXPECT_TRUE(second.isHeldOpen()) << "and must hold on to the one it opened";
    }
    EXPECT_FALSE(second.isHeldOpen());
}

// The empty case, which the close-first logic must not get wrong: resetting a keeper
// that holds nothing is just taking ownership, with nothing to close.
TEST(TIFFKeeper, resetOnAnEmptyKeeperTakesOwnership) {
    const TempTiff file("tiffkeeper-reset-empty.tif");
    {
        TIFFKeeper keeper;
        EXPECT_FALSE(keeper.isValid());
        keeper.reset(TiffTools::openTiffFile(file.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(file.isHeldOpen()) << "the keeper holds it open";
    }
    EXPECT_FALSE(file.isHeldOpen());
}

// TIFFKeeper is move-only by design, so copying one is a compile error rather than a
// test: an owning handle with two owners closes twice. There is no test here because a
// test that must fail to compile is not worth the build machinery -- the deleted copy
// constructor in tiffkeeper.hpp is the guarantee.

// Observed against the unmodified class: with no user-declared move constructor (and
// the user-declared destructor suppresses the implicit one), "TIFFKeeper target(std::move(source))"
// resolves to the implicitly-generated COPY constructor (an rvalue binds to
// "const TIFFKeeper&", which beats the user-defined-conversion route through
// "operator TIFF*()" needed to reach the TIFF*-taking constructor). Both keepers end up
// owning the same handle; EXPECT_FALSE(source.isValid()) fails cleanly at line 137, but
// then, at scope exit, target's destructor closes the handle and source's destructor
// closes it again -- a double close that aborts the whole slideio_tests process before
// the test summary prints, taking the rest of the suite's results down with it.
TEST(TIFFKeeper, moveConstructionTransfersOwnership) {
    const TempTiff file("tiffkeeper-move-ctor.tif");
    {
        TIFFKeeper source(TiffTools::openTiffFile(file.string()));
        TIFFKeeper target(std::move(source));
        EXPECT_TRUE(target.isValid());
        EXPECT_FALSE(source.isValid()) << "a moved-from keeper owns nothing";
        EXPECT_TRUE(file.isHeldOpen()) << "the target still holds it open";
    }
    EXPECT_FALSE(file.isHeldOpen()) << "one close, by the target";
}

// Observed against the unmodified class: "target = std::move(source)" resolves to the
// implicitly-generated COPY assignment operator for the same reason as above (an rvalue
// binds to "const TIFFKeeper&" ahead of the user-defined conversion needed to reach
// "operator=(TIFF*)"). Both EXPECT failures at lines 157 and 159 print cleanly -- target's
// original handle was overwritten, not closed, and source is left "valid" -- but then,
// at scope exit, target and source's destructors both close the one handle they now
// share, which aborts the process before the test summary prints.
TEST(TIFFKeeper, moveAssignmentClosesItsOwnHandleFirst) {
    const TempTiff own("tiffkeeper-move-own.tif");
    const TempTiff taken("tiffkeeper-move-taken.tif");
    {
        TIFFKeeper target(TiffTools::openTiffFile(own.string()));
        TIFFKeeper source(TiffTools::openTiffFile(taken.string()));
        target = std::move(source);
        EXPECT_FALSE(own.isHeldOpen()) << "move assignment must close what it gives up";
        EXPECT_TRUE(taken.isHeldOpen()) << "and must keep the handle it took";
        EXPECT_FALSE(source.isValid());
    }
    EXPECT_FALSE(taken.isHeldOpen());
}
