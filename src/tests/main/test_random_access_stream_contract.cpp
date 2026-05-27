// src/tests/main/test_random_access_stream_contract.cpp
#include "memorystream.hpp"
#include "random_access_stream_contract.hpp"
#include "slideio/imagetools/filestream.hpp"
#include "slideio/imagetools/httpstream.hpp"
#include "http_fixture/http_fixture.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace slideio { namespace tests {

struct MemoryStreamFactory : StreamFactory {
    std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) override
    {
        return std::make_shared<MemoryStream>(bytes);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(MemoryStream, RandomAccessStreamContract,
                               ::testing::Types<MemoryStreamFactory>);

struct FileStreamFactory : StreamFactory {
    std::vector<std::filesystem::path> tempFiles;

    ~FileStreamFactory() override {
        for (const auto& p : tempFiles) { std::error_code ec; std::filesystem::remove(p, ec); }
    }

    std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) override
    {
        auto path = std::filesystem::temp_directory_path() /
                    ("slideio_fs_" + std::to_string(reinterpret_cast<uintptr_t>(this))
                     + "_" + std::to_string(tempFiles.size()) + ".bin");
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        tempFiles.push_back(path);
        return std::make_shared<slideio::FileStream>(path.string());
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(FileStream, RandomAccessStreamContract,
                               ::testing::Types<FileStreamFactory>);

// HttpStream backend. The contract's make() is called many times with different
// byte vectors, but HttpStream needs a URL. We share a single HttpFixture (lazy
// static, rooted at a temp dir created once) across all make() calls, and write
// each payload to a uniquely-named file under that root.
struct HttpStreamFactory : StreamFactory {
    static HttpFixture& sharedFixture() {
        static std::filesystem::path root = [] {
            auto p = std::filesystem::temp_directory_path() / "slideio_http_contract";
            std::filesystem::create_directories(p);
            return p;
        }();
        // HttpFixture is non-copyable/non-movable, so construct it in place via a
        // unique_ptr held in a function-local static (initialized exactly once).
        static std::unique_ptr<HttpFixture> fixture = [] {
            auto fx = std::make_unique<HttpFixture>(root);
            // Warm-up: poll a control endpoint until the freshly-spawned server
            // accepts connections, so the first HttpStream HEAD probe (which has
            // no retry) doesn't race the server bind.
            for (int i = 0; i < 50; ++i) {
                try { fx->servedCount(); break; }
                catch (...) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            }
            return fx;
        }();
        return *fixture;
    }
    static std::filesystem::path sharedRoot() {
        return std::filesystem::temp_directory_path() / "slideio_http_contract";
    }

    std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) override
    {
        // Caching is process-wide; ensure it is on for the contract tests in case
        // an earlier test (e.g. HttpStreamTest.CacheDisableForcesGetPerRead) left
        // it disabled.
        slideio::HttpStream::setCacheEnabled(true);

        static std::atomic<uint64_t> counter{0};
        const std::string filename = "c" + std::to_string(counter++) + ".bin";
        const auto path = sharedRoot() / filename;
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        HttpFixture& fx = sharedFixture();
        return std::make_shared<slideio::HttpStream>(fx.url(filename));
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(HttpStream, RandomAccessStreamContract,
                               ::testing::Types<HttpStreamFactory>);

}} // namespace
