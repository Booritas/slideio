// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.

#include "imageobjectmanager.hpp"
#include "imageobject.hpp"

using namespace slideio;


ImageObjectManager* ImageObjectManager::getInstance() {
    static ImageObjectManager instance;
    return &instance;
}

ImageObjectManager::ImageObjectManager() : m_objectCount(0) {
    m_objects.resize(m_pageSize);
}

ImageObjectManager::~ImageObjectManager() {
    clear();
}

ImageObject& ImageObjectManager::getObject(int id) {
    return m_objects[idToIndex(id)];
}

const ImageObject& ImageObjectManager::getObject(int id) const {
    return m_objects[idToIndex(id)];
}

void ImageObjectManager::removeObject(int id) {
    m_objects[idToIndex(id)].m_id = 0;
    m_removed.emplace(id);
    --m_objectCount;
}

ImageObject& ImageObjectManager::createObject() {
    if (m_removed.empty()) {
        const int size = m_objectCount;
        const int newId = indexToId(size);
        const int newIndex = idToIndex(newId);
        if(newId >= static_cast<int>(m_objects.size())) {
            m_objects.resize(newBufferSize(newIndex + 1));
        }
        ImageObject& obj = m_objects[newIndex];
        obj.m_id = newId;
        ++m_objectCount;
        return obj;
    }
    else {
        const int id = m_removed.top();
        const int index = idToIndex(id);
        m_removed.pop();
        m_objects[index].m_id = id;
        ++m_objectCount;
        return m_objects[index];
    }
}

void ImageObjectManager::clear() {
    m_objects.clear();
    m_objectCount = 0;
}

void ImageObjectManager::bulkCreate(int count, std::vector<int>& ids) {
    ids.resize(count);
    int allocated = 0;
    for (; allocated < count && !m_removed.empty(); ++allocated) {
        const int id = m_removed.top();
        m_removed.pop();
        m_objects[idToIndex(id)].m_id = id;
        ids[allocated] = id;
        ++m_objectCount;
    }
    if (allocated < count) {
        const int left = count - allocated;
        const int newIndex = m_objectCount;
        int newId = indexToId(newIndex);
        const int newSize = m_objectCount + left;
        if(newSize > static_cast<int>(m_objects.size())) {
            m_objects.resize(newBufferSize(newSize));
        }
        for (int i = 0; i < left; ++i, ++newId, ++allocated) {
            m_objects[idToIndex(newId)].m_id = newId;
            ids[allocated] = newId;
            ++m_objectCount;
        }
    }
}
