// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/exceptions.hpp"
#include <algorithm>
#include <thread>

using namespace slideio;

int ContextPool::defaultMax() {
    const unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) {
        return 1;
    }
    return static_cast<int>(std::min(8u, cores));
}

ContextPool::ContextPool(Factory factory, int maxContexts)
    : m_factory(std::move(factory)), m_maxContexts(maxContexts) {
    if (!m_factory) {
        RAISE_RUNTIME_ERROR << "ContextPool: a context factory is required";
    }
    if (m_maxContexts < 0) {
        RAISE_RUNTIME_ERROR << "ContextPool: negative bound " << m_maxContexts;
    }
}

ContextPool::~ContextPool() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_closing = true;
    // Wake anyone parked in acquire() so they can see m_closing and leave.
    // Without this they sleep until some unrelated event notifies them, and the
    // last give() below could wake this destructor first.
    m_available.notify_all();
    // A read racing a Slide close must not have its context freed underneath
    // it -- and a thread parked in acquire() must not have the mutex it is
    // about to relock freed underneath it either. A waiter still inside
    // wait() has to reacquire m_mutex before it can return, so finishing here
    // while m_waiters > 0 would destroy that mutex mid-acquisition, which is
    // undefined behaviour rather than merely a missed wakeup. Both counters,
    // therefore, not just m_borrowed.
    m_available.wait(lock, [this]() { return m_borrowed == 0 && m_waiters == 0; });
    m_free.clear();
    m_contexts.clear();
}

int ContextPool::contextCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_contexts.size());
}

ContextPool::Borrow ContextPool::acquire() {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        if (m_closing) {
            RAISE_RUNTIME_ERROR << "ContextPool: the pool is being destroyed";
        }
        if (!m_free.empty()) {
            ReadContext* context = m_free.back();
            m_free.pop_back();
            ++m_borrowed;
            return Borrow(this, context);
        }
        // NOTE: this must count contexts that are still under construction on
        // another thread, not just m_contexts.size(). A context is only pushed
        // into m_contexts once its factory call returns (see below), but
        // m_borrowed is incremented before that -- as soon as permission to
        // grow is granted, while still under the lock. So "total contexts that
        // exist or are promised" is m_borrowed (in-flight + lent-out built
        // ones) plus m_free.size() (built, idle ones), *not* m_contexts.size()
        // (which lags behind while a grow is in flight). Checking
        // m_contexts.size() here instead would let more than m_maxContexts
        // threads all observe "not yet grown" and all proceed to grow at once,
        // overshooting the bound -- exactly the bug neverExceedsItsBound exists
        // to catch.
        const bool mayGrow = m_maxContexts == kUnbounded
                             || m_borrowed + static_cast<int>(m_free.size()) < m_maxContexts;
        if (mayGrow) {
            ++m_borrowed;
            // Every throwing path out of this branch has to put that increment
            // back, or the destructor's m_borrowed == 0 predicate never becomes
            // true and ~ContextPool blocks forever. There are three such paths
            // and only two of them are obvious: the factory, the null check,
            // and m_contexts.push_back -- which can throw bad_alloc *after* the
            // factory has already succeeded. A guard covers the branch as a
            // whole so a fourth one cannot be added without being covered too.
            bool grown = false;
            struct GrowthGuard
            {
                std::unique_lock<std::mutex>& lock;
                int& borrowed;
                std::condition_variable& available;
                const bool& grown;
                ~GrowthGuard() {
                    if (grown) {
                        return;
                    }
                    if (!lock.owns_lock()) {
                        lock.lock();
                    }
                    --borrowed;
                    available.notify_all();
                }
            } guard{lock, m_borrowed, m_available, grown};
            lock.unlock();
            // The factory does I/O (TIFFOpen), so it runs outside the lock.
            std::unique_ptr<ReadContext> created = m_factory();
            if (!created) {
                RAISE_RUNTIME_ERROR << "ContextPool: the factory returned null";
            }
            ReadContext* context = created.get();
            lock.lock();
            m_contexts.push_back(std::move(created));
            grown = true;
            return Borrow(this, context);
        }
        {
            // Counted across the wait so a destructor can tell the difference
            // between "nobody is using the pool" and "nobody is using it yet".
            // The decrement happens under the same lock acquisition that wait()
            // returns with, and the loop re-checks m_closing before releasing
            // it, so the count never reads zero while this thread still intends
            // to wait.
            struct WaiterCount {
                int& waiters;
                std::condition_variable& available;
                const bool& closing;
                ~WaiterCount() {
                    --waiters;
                    if (closing) {
                        // A destructor may be waiting for exactly this.
                        available.notify_all();
                    }
                }
            } counted{m_waiters, m_available, m_closing};
            ++m_waiters;
            m_available.wait(lock);
        }
    }
}

void ContextPool::give(ReadContext* context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_free.push_back(context);
    --m_borrowed;
    m_available.notify_all();
}

ContextPool::Borrow::Borrow(Borrow&& other) noexcept
    : m_pool(other.m_pool), m_context(other.m_context) {
    other.m_pool = nullptr;
    other.m_context = nullptr;
}

ContextPool::Borrow& ContextPool::Borrow::operator=(Borrow&& other) noexcept {
    if (this != &other) {
        release();
        m_pool = other.m_pool;
        m_context = other.m_context;
        other.m_pool = nullptr;
        other.m_context = nullptr;
    }
    return *this;
}

ContextPool::Borrow::~Borrow() {
    release();
}

void ContextPool::Borrow::release() {
    if (m_pool && m_context) {
        m_pool->give(m_context);
    }
    m_pool = nullptr;
    m_context = nullptr;
}
