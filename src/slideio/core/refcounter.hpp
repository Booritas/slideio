// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
namespace slideio
{
    class SLIDEIO_CORE_EXPORTS RefCounter
    {
    public:
        void increaseCounter() {
            if (!m_counter)
                initializeCounter();
            ++m_counter;
        }
        void decreaseCounter() {
            --m_counter;
            if (!m_counter)
                cleanCounter();
        }
    protected:
        virtual void initializeCounter(){};
        virtual void cleanCounter(){};
    private:
        int m_counter = 0;
    };

    class SLIDEIO_CORE_EXPORTS RefCounterGuard
    {
    public:
        RefCounterGuard(RefCounter* counter) : m_counter(counter) {
            m_counter->increaseCounter();
        }
        ~RefCounterGuard() {
            m_counter->decreaseCounter();
        }
        RefCounter* m_counter;
    };
};
