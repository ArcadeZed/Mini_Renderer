#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;  // xyz = tangent direction, w = handedness

// GPU-aligned light structure (matches C++ GPULight)
struct GPULight {
    vec4 positionAndType;   // xyz = position, w = type (0=Point, 1=Directional, 2=Spot)
    vec4 colorAndIntensity; // rgb = color, a = intensity
    vec4 directionAndRange; // xyz = direction (normalized), w = range
    vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = unused
};

layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    int lightCount;         // Number of active lights (0-8)
    float ambientStrength;
    float shininess;
    float metalness;        // PBR parameter
    float roughness;        // PBR parameter
    GPULight lights[8];     // Array of lights
    mat4 lightSpaceMatrix;  // Light space transform for shadow mapping
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int materialIndex;  // Index into material buffer (for PBR shaders)
} pushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 fragUV;
layout(location = 4) out vec3 fragTangent;
layout(location = 5) out vec3 fragBitangent;
layout(location = 6) flat out int fragMaterialIndex;  // Pass through material index
layout(location = 7) out vec4 fragPosLightSpace;  // Position in light space for shadow mapping

void main() {
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Transform to light space for shadow mapping
    fragPosLightSpace = ubo.lightSpaceMatrix * worldPos;

    fragColor = inColor;
    fragUV = inUV;
    fragMaterialIndex = pushConstants.materialIndex;

    // Transform normal to world space (inverse-transpose for non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(pushConstants.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    // Transform tangent to world space
    fragTangent = normalize(normalMatrix * inTangent.xyz);

    // Gram-Schmidt re-orthogonalization
    fragTangent = normalize(fragTangent - dot(fragTangent, fragNormal) * fragNormal);

    // Compute bitangent with handedness
    fragBitangent = cross(fragNormal, fragTangent) * inTangent.w;
}