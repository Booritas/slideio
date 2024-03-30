#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/imageobjectmanager.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

using namespace slideio;

TEST(ImageObjects, addRemoveSingleObjects) {
    ImageObjectManager* imObjMngr = ImageObjectManager::getInstance();
    imObjMngr->clear();
    {
        ImageObject& obj1 = imObjMngr->createObject();
        ASSERT_EQ(1, obj1.m_id);
        ASSERT_EQ(1, imObjMngr->getObjectCount());
        ImageObject& obj2 = imObjMngr->getObject(1);
        ASSERT_EQ(1, obj2.m_id);
    }
    {
        ImageObject& obj1 = imObjMngr->createObject();
        ASSERT_EQ(2, obj1.m_id);
        ASSERT_EQ(2, imObjMngr->getObjectCount());
        ImageObject& obj2 = imObjMngr->getObject(2);
        ASSERT_EQ(2, obj2.m_id);
    }
    {
        imObjMngr->removeObject(1);
        ASSERT_EQ(1, imObjMngr->getObjectCount());
        ImageObject& obj3 = imObjMngr->createObject();
        ASSERT_EQ(1, obj3.m_id);
        ASSERT_EQ(2, imObjMngr->getObjectCount());
    }
}

TEST(ImageObjects, bulkAddObjects) {
    ImageObjectManager* imObjMngr = ImageObjectManager::getInstance();
    imObjMngr->clear();
    {
        std::vector<int> ids;
        imObjMngr->bulkCreate(10, ids);
        ASSERT_EQ(10, ids.size());
        ASSERT_EQ(10, imObjMngr->getObjectCount());
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(i+1, ids[i]);
            ImageObject& obj = imObjMngr->getObject(ids[i]);
            ASSERT_EQ(ids[i], obj.m_id);
        }
    }
    {
        std::vector<int> ids;
        imObjMngr->bulkCreate(10, ids);
        ASSERT_EQ(10, ids.size());
        ASSERT_EQ(20, imObjMngr->getObjectCount());
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(i+11, ids[i]);
            ImageObject& obj = imObjMngr->getObject(ids[i]);
            ASSERT_EQ(ids[i], obj.m_id);
        }
    }
    {
        std::vector<int> ids = { 1, 5, 7, 15 };
        for (int id : ids) {
            imObjMngr->removeObject(id);
        }
        ASSERT_EQ(16, imObjMngr->getObjectCount());
        std::vector<int> ids2;
        imObjMngr->bulkCreate(10, ids2);
        ASSERT_EQ(10, ids2.size());
        ASSERT_EQ(26, imObjMngr->getObjectCount());
        std::vector<int> idNew = { 1, 5, 7, 15, 21, 22, 23, 24, 25, 26 };
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(idNew[i], ids2[i]);
            ImageObject& obj = imObjMngr->getObject(ids2[i]);
            ASSERT_EQ(ids2[i], obj.m_id);
        }
    }
}

TEST(ImageObjects, capacity) {
    ImageObjectManager* imObjMngr = ImageObjectManager::getInstance();
    imObjMngr->clear();
    imObjMngr->setPageSize(10);
    ASSERT_EQ(10, imObjMngr->getCapacity());
    std::vector<int> ids;
    imObjMngr->bulkCreate(10, ids);
    ASSERT_EQ(10, imObjMngr->getCapacity());
    imObjMngr->bulkCreate(10, ids);
    ASSERT_EQ(20, imObjMngr->getCapacity());
    for(int i = 0; i < 20; ++i) {
        ASSERT_EQ(i+1, imObjMngr->getObject(i + 1).m_id);
    }
}

TEST(ImageObjects, clear) {
    ImageObjectManager* imObjMngr = ImageObjectManager::getInstance();
    imObjMngr->clear();
    ASSERT_EQ(0, imObjMngr->getObjectCount());
    std::vector<int> ids;
    imObjMngr->bulkCreate(100, ids);
    ASSERT_EQ(100, imObjMngr->getObjectCount());
    imObjMngr->clear();
    ASSERT_EQ(0, imObjMngr->getObjectCount());
}
