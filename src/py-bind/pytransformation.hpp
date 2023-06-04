// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <pybind11/pybind11.h>
#include "pyscene.hpp"

namespace slideio
{
    class Transformation;
}

class PyScene;

std::shared_ptr<PyScene> pyTransformScene(std::shared_ptr<PyScene>& pyScene, const pybind11::list& l);
