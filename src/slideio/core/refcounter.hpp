// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <atomic>
namespace slideio
{
    class SLIDEIO_CORE_EXPORTS RefCounter
    {
    public:
        void increaseCounter() {
            if (m_counter.fetch_add(1, std::memory_order_acq_rel) == 0)
                initializeCounter();
        }
        void decreaseCounter() {
            if (m_counter.fetch_sub(1, std::memory_order_acq_rel) == 1)
                cleanCounter();
        }
    protected:
        virtual void initializeCounter(){};
        virtual void cleanCounter(){};
    private:
        std::atomic<int> m_counter{0};
    };

    class SLIDEIO_CORE_EXPORTS RefCounterGuard
    {
    public:
        RefCounterGuard(RefCounter* counter) : m_counter(counter) {
            if (m_counter)
                m_counter->increaseCounter();
        }
        ~RefCounterGuard() {
            if (m_counter)
                m_counter->decreaseCounter();
        }
        RefCounter* m_counter;
    };
};
