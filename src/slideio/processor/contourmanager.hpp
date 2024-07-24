// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <cstdint>
#include <list>
#include <memory>

#include "slideio/processor/slideio_processor_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class  ContourManager
    {
    private:
        class Page {
        public:
            Page(int32_t pageSize) : m_pageSize(pageSize) {
                m_data.reset(new int32_t[pageSize]);
            }
            int32_t* addContour(int32_t* points, int32_t count) {
                int32_t* gap = findGap(count);
                if(gap) {
                    std::copy(points, points + count, gap + 1);
                    int32_t* newGap = gap + count + 1;
                    newGap[0] = gap[0] - count - 1;
                    removeGap(gap);
                    insertNewGap(newGap);
                    gap[0] = count;
                    m_contours.push_back(gap);
                    return gap+1;
                }
                const int32_t freeSize = m_pageSize - m_freeBegin;
                if(freeSize < (count + 1)) {
                    return nullptr;
                }
                int32_t * contour = m_data.get() + m_freeBegin;
                contour[0] = count;
                m_freeBegin+=count+1;
                std::copy_n(points, count, contour + 1);
                m_contours.push_back(contour);
                return contour+1;
            }
            void removeContour(int32_t* contour) {
                m_contours.remove(contour);
                insertNewGap(contour);
            }
            void removeGap(int32_t* gap) {
                m_gaps.remove(gap);
            }
            void insertNewGap(int32_t* gap) {
                const int32_t gapSize = gap[0];
                for(auto it = m_gaps.begin(); it != m_gaps.end(); ++it) {
                    if((*it)[0] > gapSize) {
                        m_gaps.insert(it, gap);
                        return;
                    }
                }
                m_gaps.push_back(gap);
            }
            int32_t * findGap(int32_t count) {
                for(auto it = m_gaps.begin(); it != m_gaps.end(); ++it) {
                    if((*it)[0] >= count) {
                        return *it;
                    }
                }
                return nullptr;
            }
        private:
            std::unique_ptr<int32_t> m_data;
            std::list<int32_t*> m_contours;
            std::list<int32_t*> m_gaps;
            int32_t m_freeBegin = 0;
            int32_t m_pageSize;
        };
    public:
        ContourManager() {
            addPage();
        }
        ~ContourManager() = default;
        ContourManager(const ContourManager&) = delete;
        void operator=(const ContourManager&) = delete;
        ContourManager(ContourManager&&) = delete;
        void operator=(ContourManager&&) = delete;
        void clear() {
            m_pages.clear();
        }
        void setPageSize(int pageSize) {
            m_pageSize = pageSize;
        }
        int32_t* addContour(int32_t* points, int32_t count) {
            if(m_pages.empty()) {
                addPage();
            }
            int32_t* contour = m_pages.back()->addContour(points, count);
            if(!contour) {
                std::shared_ptr<Page> page = addPage(std::max(m_pageSize, count+1));
                contour = page->addContour(points, count);
            }
            return contour;
        }
    private:
        std::shared_ptr<Page> addPage(int32_t pageSize=0) {
            std::shared_ptr<Page> page = std::make_shared<Page>(pageSize==0?m_pageSize:pageSize);
            m_pages.push_back(page);
            return page;
        }
    private:
        int32_t m_pageSize = 6*1024*1024;
        std::list<std::shared_ptr<Page>> m_pages;
    };
}
