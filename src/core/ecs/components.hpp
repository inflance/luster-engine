#pragma once

#include <glm/glm.hpp>

namespace luster::ecs
{
    struct Transform
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotationEuler{0.0f}; // radians
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
    };

    struct MeshTag { };
    struct MaterialTag { };
    struct CameraComponent
    {
        float fovYRadians = glm::radians(60.0f);
        float nearZ = 0.1f;
        float farZ = 100.0f;
    };
    struct LightComponent
    {
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
    };
}


