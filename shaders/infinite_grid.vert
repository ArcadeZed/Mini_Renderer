#version 450

// Fullscreen quad vertices (no vertex buffer needed, generated in shader)
vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float ambientStrength;
    float shininess;
} ubo;

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;
layout(location = 2) out mat4 fragView;
layout(location = 6) out mat4 fragProj;

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec2 pos = positions[gl_VertexIndex];

    // Unproject near and far points to get ray in world space
    nearPoint = unprojectPoint(pos.x, pos.y, 0.0, ubo.view, ubo.proj).xyz;
    farPoint = unprojectPoint(pos.x, pos.y, 1.0, ubo.view, ubo.proj).xyz;

    fragView = ubo.view;
    fragProj = ubo.proj;

    gl_Position = vec4(pos, 0.0, 1.0);
}
