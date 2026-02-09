#pragma once

#include "../mesh/Mesh.h"
#include "Transform.h"

// A renderable entity: mesh geometry + spatial transform.
// Material reference will be added in Phase 4.
struct SceneObject {
    Mesh mesh;
    Transform transform;
};
