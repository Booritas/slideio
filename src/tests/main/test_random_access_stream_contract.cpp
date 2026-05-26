// src/tests/main/test_random_access_stream_contract.cpp
#include "memorystream.hpp"
#include "random_access_stream_contract.hpp"

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

}} // namespace
