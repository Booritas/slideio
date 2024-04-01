// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <memory>
#include "imageobjectmanager.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class CVScene;
    class Storage;

    class SLIDEIO_PROCESSOR_EXPORTS Project
    {
    public:
        Project(std::shared_ptr<CVScene>& scene, std::shared_ptr<Storage>& storage);
        Project() = delete;
        Project(const Project&) = delete;
        Project(Project&&) = delete;
        virtual ~Project();
        std::shared_ptr<ImageObjectManager> getObjects() const {
            return m_objects;
        }
        std::shared_ptr<CVScene> getScene() const {
            return m_scene;
        }
        std::shared_ptr<Storage> getStorage() const {
            return m_storage;
        }
    private:
        std::shared_ptr<ImageObjectManager> m_objects;
        std::shared_ptr<CVScene> m_scene;
        std::shared_ptr<Storage> m_storage;
    };
};

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
