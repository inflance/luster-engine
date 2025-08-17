#pragma once

#include "core/camera.hpp"
#include "core/input.hpp"

namespace luster
{
    // Encapsulates camera movement/look logic separate from the camera data
    class CameraController
    {
    public:
        void setMoveSpeed(float s) { moveSpeed_ = s; }
        void setFastMultiplier(float m) { fastMultiplier_ = m; }
        void setSlowMultiplier(float m) { slowMultiplier_ = m; }
        void setMouseSensitivity(float s) { mouseSensitivity_ = s; }

        float moveSpeed() const { return moveSpeed_; }
        float fastMultiplier() const { return fastMultiplier_; }
        float slowMultiplier() const { return slowMultiplier_; }
        float mouseSensitivity() const { return mouseSensitivity_; }

        void update(Camera& cam, float dt, const InputSnapshot& in) const;

    private:
        float moveSpeed_ = 8.0f;
        float fastMultiplier_ = 3.0f;
        float slowMultiplier_ = 0.3f;
        float mouseSensitivity_ = 0.005f;
    };
}


