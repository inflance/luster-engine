#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/input.hpp"

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
        const glm::vec3& target() const { return target_; }
        const glm::vec3& up() const { return up_; }

        void setEye(const glm::vec3& e) { eye_ = e; view_ = glm::lookAt(eye_, target_, up_); }
        void setTarget(const glm::vec3& t) { target_ = t; view_ = glm::lookAt(eye_, target_, up_); }
        void setUp(const glm::vec3& u) { up_ = u; view_ = glm::lookAt(eye_, target_, up_); }

        // Input-driven updates using a snapshot (WASD+QE, Shift/Caps speed, LMB look)
        void updateFromInput(float dt, const InputSnapshot& in)
        {
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
            if (in.keyShift) speed *= fastMultiplier_;
            if (in.keyCaps) speed *= slowMultiplier_;
            if (in.keyW) eye_ += forward * speed, target_ += forward * speed;
            if (in.keyS) eye_ -= forward * speed, target_ -= forward * speed;
            if (in.keyA) eye_ -= right * speed,   target_ -= right * speed;
            if (in.keyD) eye_ += right * speed,   target_ += right * speed;
            // Q/E vertical movement along up axis
            if (in.keyQ) { eye_ -= up_ * speed; target_ -= up_ * speed; }
            if (in.keyE) { eye_ += up_ * speed; target_ += up_ * speed; }

            // 鼠标视角：按住左键，使用位置差分作为相对位移
            if (in.mouseButtons & SDL_BUTTON_LMASK)
            {
                // SDL 屏幕坐标 Y 向下为正，通常需要反向以获得直觉上的“上移鼠标→抬头”
                yaw_   += in.mouseDx * mouseSensitivity_;
                pitch_ -= in.mouseDy * mouseSensitivity_;
                // 限制俯仰角，避免万向节锁
                pitch_ = glm::clamp(pitch_, glm::radians(-89.0f), glm::radians(89.0f));

                // 基于世界 Y 轴为上（up_ = (0,1,0)）重建朝向：
                // yaw 绕 Y 轴，pitch 绕右手坐标系的 X 轴
                glm::vec3 dir{};
                dir.x = cosf(pitch_) * sinf(yaw_);
                dir.y = sinf(pitch_);
                dir.z = cosf(pitch_) * cosf(yaw_);
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
        float yaw_ = 0.0f;                 // around Z (unused by controller path)
        float pitch_ = 0.0f;               // around right axis (unused by controller path)
    };
}



