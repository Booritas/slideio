// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include <memory>

namespace slideio
{
    class CVScene;
    class Scene;

    // The seam between the public Scene and the CVScene it wraps.
    //
    // Both operations used to be public members of Scene: a constructor taking
    // a std::shared_ptr<CVScene>, and getCVScene(). CVScene is internal --
    // cvscene.hpp includes <opencv2/core.hpp> and has never been installed --
    // so no consumer of the package could construct a Scene or do anything with
    // what getCVScene() handed back, beyond copying an opaque shared_ptr. They
    // were machinery in a class whose whole job is to be the public face of the
    // library.
    //
    // They live here instead, in a header that is not installed, and Scene
    // befriends this struct. The forward declaration of CVScene stays in
    // scene.hpp regardless -- Scene holds a std::shared_ptr<CVScene> member, so
    // the name has to be visible there -- but it is now private machinery
    // rather than part of the class's interface.
    //
    // A struct of statics rather than free functions because friendship is
    // granted to one name: Scene says `friend struct SceneInternal;` once,
    // instead of naming every function that needs in.
    struct SLIDEIO_EXPORTS SceneInternal
    {
        // Wraps a CVScene in a public Scene. Used by Slide, by the transformer
        // when it wraps a scene in a TransformerScene, and by tests.
        static std::shared_ptr<Scene> createScene(std::shared_ptr<CVScene> cvScene);

        // The CVScene a Scene wraps. Used by the converter and the transformer,
        // which do their work against the OpenCV-based interface.
        static std::shared_ptr<CVScene> getCVScene(const std::shared_ptr<Scene>& scene);
    };
}
