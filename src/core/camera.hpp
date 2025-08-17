#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>

namespace luster
{
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    class Camera
    {
    public:
        void setPerspective(float fovYRadians, float aspect, float nearZ, float farZ)
        {
            projectionType_ = ProjectionType::Perspective;
            fovY_ = fovYRadians; aspect_ = aspect; near_ = nearZ; far_ = farZ;
            updateProjection();
        }

        void setOrthographic(float width, float height, float nearZ, float farZ)
        {
            projectionType_ = ProjectionType::Orthographic;
            orthoWidth_ = width; orthoHeight_ = height; near_ = nearZ; far_ = farZ;
            updateProjection();
        }

        void setViewLookAt(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up)
        {
            eye_ = eye; target_ = target; up_ = up;
            view_ = glm::lookAt(eye_, target_, up_);
        }

        void setAspect(float aspect)
        {
            aspect_ = aspect; updateProjection();
        }

        const glm::mat4& view() const { return view_; }
        const glm::mat4& proj() const { return proj_; }
        ProjectionType projectionType() const { return projectionType_; }
        const glm::vec3& eye() const { return eye_; }

        // Input-driven updates (WASD + right-mouse look)
        void updateFromSdl(float dt)
        {
            // 确保键盘状态已更新
            SDL_PumpEvents();
            const bool* ks = SDL_GetKeyboardState(nullptr);

            glm::vec3 forward = glm::normalize(target_ - eye_);
            glm::vec3 right = glm::cross(forward, up_);
            if (glm::dot(right, right) < 1e-6f)
            {
                // 当 forward 与 up 共线时，选择一个备用的 up 以得到有效的 right
                const glm::vec3 altUp = glm::abs(forward.z) > 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                                  : glm::vec3(0.0f, 0.0f, 1.0f);
                right = glm::cross(forward, altUp);
            }
            right = glm::normalize(right);
            float speed = moveSpeed_ * dt;
            // 速度修饰：Shift 加速，CapsLock 慢速
            const SDL_Keymod mods = SDL_GetModState();
            const bool shiftDown = ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT];
            const bool capsOn = (mods & SDL_KMOD_CAPS) != 0 || ks[SDL_SCANCODE_CAPSLOCK];
            if (shiftDown) speed *= fastMultiplier_;
            if (capsOn) speed *= slowMultiplier_;
            if (ks[SDL_SCANCODE_W]) eye_ += forward * speed, target_ += forward * speed;
            if (ks[SDL_SCANCODE_S]) eye_ -= forward * speed, target_ -= forward * speed;
            if (ks[SDL_SCANCODE_A]) eye_ -= right * speed,   target_ -= right * speed;
            if (ks[SDL_SCANCODE_D]) eye_ += right * speed,   target_ += right * speed;
            // Q/E vertical movement along up axis
            if (ks[SDL_SCANCODE_Q]) { eye_ -= up_ * speed; target_ -= up_ * speed; }
            if (ks[SDL_SCANCODE_E]) { eye_ += up_ * speed; target_ += up_ * speed; }

            // 鼠标视角：按住右键，使用位置差分作为相对位移
            float mx = 0.0f, my = 0.0f;
            SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
            static float lastx = mx, lasty = my;
            float dx = mx - lastx;
            float dy = my - lasty;
            lastx = mx; lasty = my;
            if (buttons & SDL_BUTTON_RMASK)
            {
                yaw_   += dx * mouseSensitivity_;
                pitch_ += dy * mouseSensitivity_;
                pitch_ = glm::clamp(pitch_, glm::radians(-89.0f), glm::radians(89.0f));
                // Rebuild forward from yaw/pitch around world up (Z)
                glm::vec3 dir{};
                dir.x = cosf(pitch_) * cosf(yaw_);
                dir.y = cosf(pitch_) * sinf(yaw_);
                dir.z = sinf(pitch_);
                target_ = eye_ + glm::normalize(dir);
            }
            view_ = glm::lookAt(eye_, target_, up_);
        }

    private:
        void updateProjection()
        {
            if (projectionType_ == ProjectionType::Perspective)
            {
                proj_ = glm::perspectiveRH_ZO(fovY_, aspect_, near_, far_);
                proj_[1][1] *= -1.0f; // GLM for Vulkan (flip Y)
            }
            else
            {
                float hw = orthoWidth_ * 0.5f;
                float hh = orthoHeight_ * 0.5f;
                proj_ = glm::ortho(-hw, hw, -hh, hh, near_, far_);
                proj_[1][1] *= -1.0f;
            }
        }

        ProjectionType projectionType_ = ProjectionType::Perspective;
        glm::mat4 view_{1.0f};
        glm::mat4 proj_{1.0f};

        // perspective
        float fovY_ = glm::radians(60.0f);
        float aspect_ = 16.0f / 9.0f;
        // ortho
        float orthoWidth_ = 2.0f;
        float orthoHeight_ = 2.0f;
        // common
        float near_ = 0.1f;
        float far_ = 100.0f;

        glm::vec3 eye_{0.0f, 0.0f, -3.0f};
        glm::vec3 target_{0.0f, 0.0f, 0.0f};
        glm::vec3 up_{0.0f, 1.0f, 0.0f};

        // input params
        float moveSpeed_ = 8.0f;           // units per second
        float fastMultiplier_ = 3.0f;      // when Shift is held
        float slowMultiplier_ = 0.3f;      // when CapsLock is active
        float mouseSensitivity_ = 0.005f;  // radians per pixel
        float yaw_ = 0.0f;                 // around Z
        float pitch_ = 0.0f;               // around right axis
    };
}



