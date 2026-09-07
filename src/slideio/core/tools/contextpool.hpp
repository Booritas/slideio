// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include <cassert>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace slideio
{
    /**
     * A bounded free-list of ReadContext objects.
     *
     * acquire() hands out any free context rather than one keyed to the calling
     * thread. That is deliberate: a free list can be bounded, releases a
     * context when the borrow dies rather than when the thread does, and does
     * not leak one context per thread that has read and exited. It requires
     * that contexts be interchangeable between threads, which every context in
     * the tree is -- a TIFF handle repositioned per read, or a scratch buffer.
     *
     * Contexts are constructed lazily, so a scene nobody reads concurrently
     * holds exactly one.
     */
    class SLIDEIO_CORE_EXPORTS ContextPool
    {
    public:
        /// Pass as maxContexts when the context holds no scarce resource, so
        /// that capping it would serialise a path with no contention.
        static constexpr int kUnbounded = 0;
        /// min(8, hardware_concurrency()), at least 1.
        static int defaultMax();

        using Factory = std::function<std::unique_ptr<ReadContext>()>;

        explicit ContextPool(Factory factory, int maxContexts = defaultMax());
        /// Blocks until every outstanding Borrow has been returned.
        ~ContextPool();

        ContextPool(const ContextPool&) = delete;
        ContextPool& operator=(const ContextPool&) = delete;

        class SLIDEIO_CORE_EXPORTS Borrow
        {
        public:
            Borrow() = default;
            Borrow(Borrow&& other) noexcept;
            Borrow& operator=(Borrow&& other) noexcept;
            ~Borrow();

            Borrow(const Borrow&) = delete;
            Borrow& operator=(const Borrow&) = delete;

            ReadContext& get() const { return *m_context; }

            // Debug builds verify the requested type actually matches the
            // borrowed context (an assert paired with a dynamic_cast); release
            // builds -- this sits on the per-read path -- do a plain
            // static_cast with no run-time check at all.
            template <class T>
            T& as() const {
                assert(dynamic_cast<T*>(m_context) != nullptr &&
                       "ContextPool::Borrow::as<T>(): borrowed context is not a T");
                return static_cast<T&>(*m_context);
            }

        private:
            friend class ContextPool;
            Borrow(ContextPool* pool, ReadContext* context)
                : m_pool(pool), m_context(context) {}
            void release();

            ContextPool* m_pool = nullptr;
            ReadContext* m_context = nullptr;
        };

        /// Reuses a free context, else constructs one, else blocks until another
        /// thread returns one. Throws if the pool is being destroyed.
        Borrow acquire();

        /// Contexts constructed so far. For tests.
        int contextCount() const;

    private:
        void give(ReadContext* context);

        Factory m_factory;
        int m_maxContexts;
        mutable std::mutex m_mutex;
        std::condition_variable m_available;
        std::vector<std::unique_ptr<ReadContext>> m_contexts;  // owns everything
        std::vector<ReadContext*> m_free;
        int m_borrowed = 0;
        bool m_closing = false;
    };
}
