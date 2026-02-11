#version 450

// Vertex attributes (position is all we need for shadows)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;      // Unused
layout(location = 2) in vec3 inNormal;     // Unused
layout(location = 3) in vec2 inUV;         // Unused
layout(location = 4) in vec4 inTangent;    // Unused

// Push constants: light space matrix + model matrix
layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    mat4 model;             // Object's model matrix
} pushConstants;

void main() {
    // Transform vertex to light space
    gl_Position = pushConstants.lightSpaceMatrix * pushConstants.model * vec4(inPosition, 1.0);
}
