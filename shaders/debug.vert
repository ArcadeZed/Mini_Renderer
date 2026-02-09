#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;    // Not used, but must match vertex structure
layout(location = 3) in vec2 inUV;        // Not used, but must match vertex structure

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

layout(location = 0) out vec3 fragColor;

void main() {
    // Simple MVP transformation
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    // Pass through vertex color unchanged (no lighting, no texture)
    fragColor = inColor;
}
