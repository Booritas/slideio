// src/tests/main/test_random_access_stream_contract.cpp
#include "memorystream.hpp"
#include "random_access_stream_contract.hpp"
#include "slideio/imagetools/filestream.hpp"
#include <filesystem>
#include <fstream>

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

}} // namespace
