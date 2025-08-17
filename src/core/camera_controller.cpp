#include "core/camera_controller.hpp"

namespace luster
{
    void CameraController::update(Camera& cam, float dt, const InputSnapshot& in) const
    {
        // Mirror logic from Camera::updateFromInput but parameterized by controller speeds
        glm::vec3 forward = glm::normalize(cam.eye() + (glm::vec3(0.0f) - glm::vec3(0.0f)));
        // Reconstruct forward from view matrix: take inverse to get world space direction of -Z
        // Simpler: derive from eye/target since Camera maintains them implicitly
        // We'll approximate forward as normalize(target - eye) via local access pattern
        // Note: we don't have direct target getter; rebuild via view inverse is heavy. Instead,
        // we nudge using Camera's existing method by temporarily constructing from view matrix.

        // Fallback: compute forward from current view matrix assuming standard lookAt
        const glm::mat4& V = cam.view();
        // Basis vectors (rows of inverse, or columns of V for world axes in camera space)
        glm::vec3 right = glm::normalize(glm::vec3(V[0][0], V[1][0], V[2][0]));
        glm::vec3 up    = glm::normalize(glm::vec3(V[0][1], V[1][1], V[2][1]));
        glm::vec3 forwardFromView = -glm::normalize(glm::vec3(V[0][2], V[1][2], V[2][2]));

        float speed = moveSpeed_ * dt;
        if (in.keyShift) speed *= fastMultiplier_;
        if (in.keyCaps) speed *= slowMultiplier_;

        glm::vec3 eye = cam.eye();
        glm::vec3 target = eye + forwardFromView;

        if (in.keyW) { eye += forwardFromView * speed; target += forwardFromView * speed; }
        if (in.keyS) { eye -= forwardFromView * speed; target -= forwardFromView * speed; }
        if (in.keyA) { eye -= right * speed;           target -= right * speed; }
        if (in.keyD) { eye += right * speed;           target += right * speed; }
        if (in.keyQ) { eye -= up * speed;              target -= up * speed; }
        if (in.keyE) { eye += up * speed;              target += up * speed; }

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


