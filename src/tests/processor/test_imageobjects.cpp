#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/processor.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/processor/imageobjectmanager.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

TEST(ImageObjects, ImageObjectManager) {
    ImageObjectManager* imObjMngr = ImageObjectManager::getInstance();
    ImageObject& obj1 = imObjMngr->createObject();
    ASSERT_EQ(1, obj1.m_id);
    ASSERT_EQ(1, imObjMngr->getObjectCount());
    ImageObject& obj2 = imObjMngr->getObject(1);
    ASSERT_EQ(1, obj2.m_id);
}

