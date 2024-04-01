// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "project.hpp"

slideio::Project::Project(std::shared_ptr<CVScene>& scene, std::shared_ptr<Storage>& storage)
{
    m_scene = scene;
    m_storage = storage;
    m_objects = std::make_shared<ImageObjectManager>();
}

slideio::Project::~Project() {
    m_objects->clear();
}
