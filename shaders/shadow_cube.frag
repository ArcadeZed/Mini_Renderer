#version 450

layout(location = 0) in vec3 fragWorldPos;

layout(push_constant) uniform PushConstants {
    mat4 faceVP;              // Not used in fragment shader
    mat4 model;               // Not used in fragment shader
    vec4 lightPosAndFarPlane; // xyz = light world position, w = far plane distance
} pushConstants;

void main() {
    // Write linear distance from light, normalized by far plane
    // This makes cube map shadow comparison straightforward:
    // sample depth = closest surface distance / farPlane
    float dist = length(fragWorldPos - pushConstants.lightPosAndFarPlane.xyz);
    gl_FragDepth = dist / pushConstants.lightPosAndFarPlane.w;
}
