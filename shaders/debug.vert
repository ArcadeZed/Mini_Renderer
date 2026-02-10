#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;    // Not used, but must match vertex structure
layout(location = 3) in vec2 inUV;        // Not used, but must match vertex structure

layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float ambientStrength;
    float shininess;
} ubo;

// Output to Geometry Shader - only clip space position and color
layout(location = 0) out vec3 outColor;

void main() {
    // Transform line endpoint to clip space
    gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);

    // Pass color to geometry shader
    outColor = inColor;
}
