// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/processor/slideio_processor_def.hpp"
#include "slideio/processor/imageobject.hpp"
#include <queue>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_PROCESSOR_EXPORTS ImageObjectManager
    {
    public:
        static ImageObjectManager* getInstance();
    private:
        ImageObjectManager();
        ~ImageObjectManager();
    public:
        ImageObjectManager(const ImageObjectManager&) = delete;
        void operator=(const ImageObjectManager&) = delete;
        ImageObjectManager(ImageObjectManager&&) = delete;
        void operator=(ImageObjectManager&&) = delete;
        ImageObject& getObject(int id);
        const ImageObject& getObject(int id) const;
        void removeObject(int id);
        ImageObject& createObject();
        void clear();
        int getObjectCount() const;
        void bulkCreate(int count, std::vector<int>& ids);
    private:
        std::vector<ImageObject> m_objects;
        std::priority_queue<int, std::vector<int>, std::greater<>> m_removed;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
