#include "core/camera_controller.hpp"

namespace luster
{
    void CameraController::update(Camera& cam, float dt, const InputSnapshot& in) const
    {
        // Compute basis from camera data
        glm::vec3 eye = cam.eye();
        glm::vec3 target = cam.target();
        glm::vec3 up = cam.up();
        glm::vec3 forward = glm::normalize(target - eye);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));

        float speed = moveSpeed_ * dt;
        if (in.keyShift) speed *= fastMultiplier_;
        if (in.keyCaps) speed *= slowMultiplier_;

        if (in.keyW) { eye += forward * speed; target += forward * speed; }
        if (in.keyS) { eye -= forward * speed; target -= forward * speed; }
        if (in.keyA) { eye -= right * speed;   target -= right * speed; }
        if (in.keyD) { eye += right * speed;   target += right * speed; }
        if (in.keyQ) { eye -= up * speed;      target -= up * speed; }
        if (in.keyE) { eye += up * speed;      target += up * speed; }

        // Mouse look (LMB)
        static float yaw = 0.0f;
        static float pitch = 0.0f;
        if (in.mouseButtons & SDL_BUTTON_LMASK)
        {
            yaw   += in.mouseDx * mouseSensitivity_;
            pitch -= in.mouseDy * mouseSensitivity_;
            pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));
            glm::vec3 dir{};
            dir.x = cosf(pitch) * sinf(yaw);
            dir.y = sinf(pitch);
            dir.z = cosf(pitch) * cosf(yaw);
            target = eye + glm::normalize(dir);
        }

        cam.setViewLookAt(eye, target, up);
    }
}


