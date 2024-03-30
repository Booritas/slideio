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

ImageObjectManager::ImageObjectManager() {
	m_objects.reserve(100000);
}

ImageObjectManager::~ImageObjectManager() {
    clear();
}

ImageObject& ImageObjectManager::getObject(int id) {
	return m_objects[id-1];
}

const ImageObject& ImageObjectManager::getObject(int id) const {
	return m_objects[id+1];
}

void ImageObjectManager::removeObject(int id) {
	m_objects[id].m_id = 0;
	m_removed.emplace(id);
}

ImageObject& ImageObjectManager::createObject() {
	if(m_removed.empty()) {
		int id = static_cast<int>(m_objects.size()) + 1;
        m_objects.resize(id);
		ImageObject& obj = m_objects.back();
		obj.m_id = id;
        return obj;
    }
    else {
        const int id = m_removed.top();
        m_removed.pop();
		m_objects[id].m_id = id;
        return m_objects[id];
    }
}

void ImageObjectManager::clear() {
	m_objects.clear();
}

int ImageObjectManager::getObjectCount() const {
	return static_cast<int>(m_objects.size());
}

void ImageObjectManager::bulkCreate(int count, std::vector<int>& ids) {
	ids.resize(count);
	int allocated = 0;
	for (; allocated < count && m_removed.empty(); ++allocated) {
		const int id = m_removed.top();
		m_removed.pop();
		m_objects[id].m_id = id;
		ids[allocated] = id;
	}
    if(allocated<count) {
		const int left = count - allocated;
		int id = static_cast<int>(m_objects.size()) + 1;
		m_objects.resize(id + left);
		for(int i = 0; i < left; ++i, ++id, ++allocated) {
            m_objects[id-1].m_id = id;
            ids[allocated] = id;
        }
	}
}
