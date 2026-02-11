#version 450

// Vertex attributes (same as main pass for vertex input compatibility)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;      // Unused
layout(location = 2) in vec3 inNormal;     // Unused
layout(location = 3) in vec2 inUV;         // Unused
layout(location = 4) in vec4 inTangent;    // Unused

// Push constants: face VP + model + light position & far plane
layout(push_constant) uniform PushConstants {
    mat4 faceVP;              // Per-face view-projection matrix
    mat4 model;               // Object's model matrix
    vec4 lightPosAndFarPlane; // xyz = light world position, w = far plane distance
} pushConstants;

layout(location = 0) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = pushConstants.faceVP * worldPos;
}
