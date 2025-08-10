#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 0) out vec3 vColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}