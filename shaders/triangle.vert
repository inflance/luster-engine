#version 450
layout(location = 0) out vec3 vColor;

const vec2 POS[3] = vec2[](
    vec2(0.0, -0.6),
    vec2(0.6, 0.6),
    vec2(-0.6, 0.6)
);
const vec3 COL[3] = vec3[](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.2),
    vec3(0.2, 0.2, 1.0)
);

void main() {
    gl_Position = vec4(POS[gl_VertexIndex], 0.0, 1.0);
    vColor = COL[gl_VertexIndex];
}