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
    // A private copy of a real tiff, so a test can assert the handle was closed by
    // deleting the file. libtiff opens without FILE_SHARE_DELETE, so a file with a live
    // handle cannot be removed -- which makes "closed" observable rather than assumed.
    std::filesystem::path copyTestTiff(const char* name) {
        const std::string source = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
        const std::filesystem::path copy = std::filesystem::temp_directory_path() / name;
        std::error_code ignored;
        std::filesystem::remove(copy, ignored);
        std::filesystem::copy_file(source, copy, std::filesystem::copy_options::overwrite_existing);
        return copy;
    }

    bool canDelete(const std::filesystem::path& path) {
        std::error_code error;
        return std::filesystem::remove(path, error);
    }
}

TEST(TIFFKeeper, destructorClosesTheHandle) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-dtor.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        ASSERT_TRUE(keeper.isValid());
        EXPECT_FALSE(canDelete(file)) << "the handle is open, so the file is locked";
    }
    EXPECT_TRUE(canDelete(file)) << "the destructor must close the handle";
}

TEST(TIFFKeeper, releaseGivesUpOwnershipWithoutClosing) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-release.tif");
    libtiff::TIFF* handle = nullptr;
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        handle = keeper.release();
        EXPECT_FALSE(keeper.isValid()) << "release leaves the keeper empty";
        EXPECT_TRUE(handle != nullptr);
    }
    EXPECT_FALSE(canDelete(file)) << "release must NOT close: the caller owns it now";
    TiffTools::closeTiffFile(handle);
    EXPECT_TRUE(canDelete(file));
}

// The defect the debt entry names: assigning a new handle over a held one dropped the
// old handle without closing it.
TEST(TIFFKeeper, resetClosesTheHandleItReplaces) {
    const std::filesystem::path first = copyTestTiff("tiffkeeper-reset-first.tif");
    const std::filesystem::path second = copyTestTiff("tiffkeeper-reset-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.reset(TiffTools::openTiffFile(second.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(canDelete(first)) << "reset must close the handle it replaced";
    }
    EXPECT_TRUE(canDelete(second));
}

// The same defect by its other door, which the debt entry does not record.
TEST(TIFFKeeper, openTiffFileClosesTheHandleItReplaces) {
    const std::filesystem::path first = copyTestTiff("tiffkeeper-open-first.tif");
    const std::filesystem::path second = copyTestTiff("tiffkeeper-open-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.openTiffFile(second.string(), true);
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(canDelete(first)) << "openTiffFile must close the handle it replaced";
    }
    EXPECT_TRUE(canDelete(second));
}

// The empty case, which the close-first logic must not get wrong: resetting a keeper
// that holds nothing is just taking ownership, with nothing to close.
TEST(TIFFKeeper, resetOnAnEmptyKeeperTakesOwnership) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-reset-empty.tif");
    {
        TIFFKeeper keeper;
        EXPECT_FALSE(keeper.isValid());
        keeper.reset(TiffTools::openTiffFile(file.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(canDelete(file)) << "the keeper holds it open";
    }
    EXPECT_TRUE(canDelete(file));
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
// owning the same handle; EXPECT_FALSE(source.isValid()) fails cleanly at line 111, but
// then, at scope exit, target's destructor closes the handle and source's destructor
// closes it again -- a double close that aborts the whole slideio_tests process before
// the test summary prints, taking the rest of the suite's results down with it.
TEST(TIFFKeeper, moveConstructionTransfersOwnership) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-move-ctor.tif");
    {
        TIFFKeeper source(TiffTools::openTiffFile(file.string()));
        TIFFKeeper target(std::move(source));
        EXPECT_TRUE(target.isValid());
        EXPECT_FALSE(source.isValid()) << "a moved-from keeper owns nothing";
        EXPECT_FALSE(canDelete(file)) << "the target still holds it open";
    }
    EXPECT_TRUE(canDelete(file)) << "one close, by the target";
}

// Observed against the unmodified class: "target = std::move(source)" resolves to the
// implicitly-generated COPY assignment operator for the same reason as above (an rvalue
// binds to "const TIFFKeeper&" ahead of the user-defined conversion needed to reach
// "operator=(TIFF*)"). Both EXPECT failures at lines 124-125 print cleanly -- target's
// original handle was overwritten, not closed, and source is left "valid" -- but then,
// at scope exit, target and source's destructors both close the one handle they now
// share, which aborts the process before the test summary prints.
TEST(TIFFKeeper, moveAssignmentClosesItsOwnHandleFirst) {
    const std::filesystem::path own = copyTestTiff("tiffkeeper-move-own.tif");
    const std::filesystem::path taken = copyTestTiff("tiffkeeper-move-taken.tif");
    {
        TIFFKeeper target(TiffTools::openTiffFile(own.string()));
        TIFFKeeper source(TiffTools::openTiffFile(taken.string()));
        target = std::move(source);
        EXPECT_TRUE(canDelete(own)) << "move assignment must close what it gives up";
        EXPECT_FALSE(source.isValid());
    }
    EXPECT_TRUE(canDelete(taken));
}
