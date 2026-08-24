// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitiffkeeper.hpp"
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "tests/testlib/testtools.hpp"
#include <slideio/slideio/imagedrivermanager.hpp>
#include <filesystem>
#include <string>

using namespace slideio;

namespace
{
    // A private copy of a real ndpi, so a test can open and close a handle on it
    // without disturbing the shared test image. Closure is observed with
    // TestTools::isFileHeldOpen, which asks whether this process still holds a
    // descriptor on the file -- see testtools.cpp for why the earlier "try to delete
    // it" probe only worked on Windows.
    //
    // Scoped so the copy is removed when the test ends: unlike the delete probe it
    // replaces, isFileHeldOpen leaves the file in place, and these copies are three
    // megabytes each.
    class TempNdpi
    {
    public:
        explicit TempNdpi(const char* name)
            : m_path(std::filesystem::temp_directory_path() / name)
        {
            const std::string source = TestTools::getTestImagePath("ndpi", "test3-TRITC 2 (560).ndpi");
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
            std::filesystem::copy_file(source, m_path, std::filesystem::copy_options::overwrite_existing);
        }
        ~TempNdpi() {
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
        }
        TempNdpi(const TempNdpi&) = delete;
        TempNdpi& operator=(const TempNdpi&) = delete;

        std::string string() const { return m_path.string(); }
        bool isHeldOpen() const { return TestTools::isFileHeldOpen(m_path.string()); }

    private:
        std::filesystem::path m_path;
    };
}

class NDPITIFFKeeperTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ImageDriverManager::setLogLevel("ERROR");
    }
    // As in test_ndpitiff_tools.cpp and test_ndpi_driver.cpp: pure RAII over libtiff's
    // process-global handlers, referenced by no test, and not to be deleted as unused.
    // It matters more here than there, because of an ordering gap the keeper cannot
    // close on its own: in "NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(path))"
    // the file is opened while evaluating the constructor's argument, so the open
    // happens before the constructor body installs the keeper's own handler. Without
    // this member those opens ran against libtiff's defaults, and the fixture's unknown
    // private tag reached stderr as a raw TIFFReadDirectory warning. With it, the
    // warning routes through SLIDEIO_LOG and the ERROR level above filters it.
    NDPITIFFMessageHandler m_messageHandler;
};

TEST_F(NDPITIFFKeeperTests, destructorClosesTheHandle) {
    const TempNdpi file("ndpikeeper-dtor.ndpi");
    {
        NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(file.string()));
        ASSERT_TRUE(keeper.isValid());
        EXPECT_TRUE(file.isHeldOpen()) << "the keeper holds the handle open";
    }
    EXPECT_FALSE(file.isHeldOpen()) << "the destructor must close the handle";
}

TEST_F(NDPITIFFKeeperTests, filePathConstructorOpensTheFile) {
    const TempNdpi file("ndpikeeper-ctor-path.ndpi");
    {
        NDPITIFFKeeper keeper(file.string());
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(file.isHeldOpen()) << "the keeper holds it open";
    }
    EXPECT_FALSE(file.isHeldOpen());
}

TEST_F(NDPITIFFKeeperTests, releaseGivesUpOwnershipWithoutClosing) {
    const TempNdpi file("ndpikeeper-release.ndpi");
    libtiff::TIFF* handle = nullptr;
    {
        NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(file.string()));
        handle = keeper.release();
        EXPECT_FALSE(keeper.isValid()) << "release leaves the keeper empty";
        EXPECT_TRUE(handle != nullptr);
    }
    EXPECT_TRUE(file.isHeldOpen()) << "release must NOT close: the caller owns it now";
    NDPITiffTools::closeTiffFile(handle);
    EXPECT_FALSE(file.isHeldOpen());
}

// The defect TECH_DEBT.md section 1 problem 2 records for TIFFKeeper, which
// NDPITIFFKeeper still carried. Observed against the unmodified class: with
// "keeper = NDPITiffTools::openTiffFile(second)" going through
// operator=(libtiff::TIFF*), which overwrote m_hFile without closing it, the assertion
// below failed with "Actual: false" -- the first file was still locked by a handle
// nothing owned any more, leaked until the process exited.
TEST_F(NDPITIFFKeeperTests, resetClosesTheHandleItReplaces) {
    const TempNdpi first("ndpikeeper-reset-first.ndpi");
    const TempNdpi second("ndpikeeper-reset-second.ndpi");
    {
        NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(first.string()));
        keeper.reset(NDPITiffTools::openTiffFile(second.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(first.isHeldOpen()) << "reset must close the handle it replaced";
        EXPECT_TRUE(second.isHeldOpen()) << "and must hold on to the one it took";
    }
    EXPECT_FALSE(second.isHeldOpen());
}

// The same defect by its other door: NDPIFile::init reached it as
// "m_tiff = NDPITiffTools::openTiffFile(filePath)".
TEST_F(NDPITIFFKeeperTests, openTiffFileClosesTheHandleItReplaces) {
    const TempNdpi first("ndpikeeper-open-first.ndpi");
    const TempNdpi second("ndpikeeper-open-second.ndpi");
    {
        NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(first.string()));
        keeper.openTiffFile(second.string());
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(first.isHeldOpen()) << "openTiffFile must close the handle it replaced";
        EXPECT_TRUE(second.isHeldOpen()) << "and must hold on to the one it opened";
    }
    EXPECT_FALSE(second.isHeldOpen());
}

// The empty case, which the close-first logic must not get wrong: resetting a keeper
// that holds nothing is just taking ownership, with nothing to close.
TEST_F(NDPITIFFKeeperTests, resetOnAnEmptyKeeperTakesOwnership) {
    const TempNdpi file("ndpikeeper-reset-empty.ndpi");
    {
        NDPITIFFKeeper keeper;
        EXPECT_FALSE(keeper.isValid());
        keeper.reset(NDPITiffTools::openTiffFile(file.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(file.isHeldOpen()) << "the keeper holds it open";
    }
    EXPECT_FALSE(file.isHeldOpen());
}

// Observed against the unmodified NDPITiffTools::closeTiffFile, which called
// libtiff::TIFFClose unconditionally: this aborted the test process with an access
// violation (SEH 0xc0000005). The keeper's own destructor guarded itself with
// "if (m_hFile)", so the crash was only reachable through the free function -- which
// NDPIFile's destructor called directly.
TEST_F(NDPITIFFKeeperTests, closingAnEmptyKeeperIsSafe) {
    NDPITIFFKeeper keeper;
    ASSERT_FALSE(keeper.isValid());
    keeper.closeTiffFile();
    EXPECT_FALSE(keeper.isValid());
}

// NDPITIFFKeeper is move-only by design, so copying one is a compile error rather than
// a test: an owning handle with two owners closes twice. There is no test here because a
// test that must fail to compile is not worth the build machinery -- the deleted copy
// constructor in ndpitiffkeeper.hpp is the guarantee.

// Before this change the class held a raw libtiff::TIFF* and declared a destructor and
// nothing else, so the implicit copy constructor and copy assignment operator were
// generated, and no move operations were declared. "NDPITIFFKeeper target(std::move(source))"
// therefore resolved to the implicit COPY constructor: both keepers ended up owning the
// same handle, EXPECT_FALSE(source.isValid()) below would have failed, and then both
// destructors would have closed the one handle. This was latent rather than live --
// NDPIFile owns the keeper by value but is itself only ever held by unique_ptr and
// passed as a raw pointer, so nothing in the driver copied one.
TEST_F(NDPITIFFKeeperTests, moveConstructionTransfersOwnership) {
    const TempNdpi file("ndpikeeper-move-ctor.ndpi");
    {
        NDPITIFFKeeper source(NDPITiffTools::openTiffFile(file.string()));
        NDPITIFFKeeper target(std::move(source));
        EXPECT_TRUE(target.isValid());
        EXPECT_FALSE(source.isValid()) << "a moved-from keeper owns nothing";
        EXPECT_TRUE(file.isHeldOpen()) << "the target still holds it open";
    }
    EXPECT_FALSE(file.isHeldOpen()) << "one close, by the target";
}

// "target = std::move(source)" resolved to the implicit COPY assignment operator for the
// same reason as above. It overwrote target's handle without closing it -- the leak
// resetClosesTheHandleItReplaces covers -- and left both keepers owning source's handle.
TEST_F(NDPITIFFKeeperTests, moveAssignmentClosesItsOwnHandleFirst) {
    const TempNdpi own("ndpikeeper-move-own.ndpi");
    const TempNdpi taken("ndpikeeper-move-taken.ndpi");
    {
        NDPITIFFKeeper target(NDPITiffTools::openTiffFile(own.string()));
        NDPITIFFKeeper source(NDPITiffTools::openTiffFile(taken.string()));
        target = std::move(source);
        EXPECT_FALSE(own.isHeldOpen()) << "move assignment must close what it gives up";
        EXPECT_TRUE(taken.isHeldOpen()) << "and must keep the handle it took";
        EXPECT_FALSE(source.isValid());
    }
    EXPECT_FALSE(taken.isHeldOpen());
}
