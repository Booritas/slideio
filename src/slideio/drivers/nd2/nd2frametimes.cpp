// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2frametimes.hpp"

#include <limits>
#include <mutex>
#include <unordered_map>

using namespace slideio::nd2;

struct FrameTimes::Store
{
    std::vector<double> table;
    Reader read;
    std::mutex mutex;
    std::unordered_map<int64_t, double> readTimes;
};

FrameTimes::FrameTimes(std::vector<double> table, int64_t count, Reader read)
    : m_store(std::make_shared<Store>()), m_count(count)
{
    m_store->table = std::move(table);
    m_store->read = std::move(read);
}

FrameTimes::FrameTimes(std::vector<double> times)
    : FrameTimes(times, static_cast<int64_t>(times.size()))
{
}

FrameTimes::FrameTimes(std::initializer_list<double> times)
    : FrameTimes(std::vector<double>(times))
{
}

double FrameTimes::operator[](int64_t frame) const
{
    const double unknown = std::numeric_limits<double>::quiet_NaN();
    if (!m_store || frame < 0 || frame >= m_count) {
        return unknown;
    }
    if (frame < static_cast<int64_t>(m_store->table.size())) {
        return m_store->table[static_cast<size_t>(frame)];
    }
    if (!m_store->read) {
        return unknown;
    }
    std::lock_guard<std::mutex> lock(m_store->mutex);
    const auto it = m_store->readTimes.find(frame);
    if (it != m_store->readTimes.end()) {
        return it->second;
    }
    const double t = m_store->read(frame);
    m_store->readTimes.emplace(frame, t);
    return t;
}
