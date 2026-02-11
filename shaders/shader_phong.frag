#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in vec3 fragTangent;      // Unused in Phong (for compatibility)
layout(location = 5) in vec3 fragBitangent;    // Unused in Phong (for compatibility)
layout(location = 6) flat in int fragMaterialIndex;  // Unused in Phong (for compatibility)
layout(location = 7) in vec4 fragPosLightSpace;  // Position in light space

// GPU-aligned light structure (matches C++ GPULight)
struct GPULight {
    vec4 positionAndType;   // xyz = position, w = type (0=Point, 1=Directional, 2=Spot)
    vec4 colorAndIntensity; // rgb = color, a = intensity
    vec4 directionAndRange; // xyz = direction (normalized), w = range
    vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = shadowFarPlane (0 = no shadow)
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    int lightCount;         // Number of active lights (0-8)
    float ambientStrength;
    float shininess;
    float metalness;        // PBR parameter (unused in Phong)
    float roughness;        // PBR parameter (unused in Phong)
    GPULight lights[8];     // Array of lights
    mat4 lightSpaceMatrix;  // Light space transform for shadow mapping
} ubo;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D shadowMap;
layout(binding = 3) uniform samplerCube shadowCubeMap;

layout(location = 0) out vec4 outColor;

// PCF (Percentage Closer Filtering) for soft shadows
// Returns 0.0 (fully shadowed) to 1.0 (fully lit)
float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform XY to [0,1] range (from [-1,1] NDC)
    // Z is already in [0,1] due to GLM_FORCE_DEPTH_ZERO_TO_ONE in Vulkan
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Outside shadow map bounds → fully lit
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    // Current fragment depth
    float currentDepth = projCoords.z;

    // Bias to prevent shadow acne (angle-dependent)
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    // PCF (sample 3x3 kernel)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;  // Average of 9 samples

    return shadow;
}

// Point light shadow using cube map with 20-sample PCF
// Returns 0.0 (fully shadowed) to 1.0 (fully lit)
float calculatePointShadow(vec3 fragPos, vec3 lightPos, float farPlane) {
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) / farPlane;

    // 20 offset directions for PCF on cube maps
    vec3 offsets[20] = vec3[](
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0,  1, -1), vec3( 0, -1, -1)
    );

    float shadow = 0.0;
    float bias = 0.002;
    float diskRadius = 0.01;
    for (int i = 0; i < 20; ++i) {
        float closestDepth = texture(shadowCubeMap, fragToLight + offsets[i] * diskRadius).r;
        shadow += currentDepth - bias > closestDepth ? 0.0 : 1.0;
    }
    shadow /= 20.0;

    return shadow;
}

void main() {
    // Sample texture (ignore vertex colors, use texture only)
    vec3 baseColor = texture(texSampler, fragUV).rgb;

    // Normalize normal and view direction (same for all lights)
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(ubo.viewPos - fragPos);

    // Ambient (global, not per-light)
    vec3 ambient = ubo.ambientStrength * baseColor;

    // Accumulate lighting from all lights
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < ubo.lightCount && i < 8; ++i) {
        GPULight light = ubo.lights[i];
        int lightType = int(light.positionAndType.w);

        // Light direction based on type
        vec3 lightDir;
        float attenuation = 1.0;

        if (lightType == 0) {  // Point Light
            vec3 lightPos = light.positionAndType.xyz;
            lightDir = normalize(lightPos - fragPos);

            // Distance-based attenuation
            float distance = length(lightPos - fragPos);
            if (distance > light.directionAndRange.w) {
                continue;  // Beyond light range, skip this light
            }
            attenuation = 1.0 / (light.attenuation.x +
                                 light.attenuation.y * distance +
                                 light.attenuation.z * distance * distance);
        } else if (lightType == 1) {  // Directional Light
            lightDir = normalize(-light.directionAndRange.xyz);
            attenuation = 1.0;  // No attenuation for directional lights
        } else {  // Spot Light (future)
            continue;  // Skip for now
        }

        // Light color and intensity
        vec3 lightColor = light.colorAndIntensity.rgb;
        float lightIntensity = light.colorAndIntensity.a;

        // Shadow calculation
        float shadow = 1.0;  // Default: no shadow (fully lit)
        if (lightType == 1) {  // Directional Light
            shadow = calculateShadow(fragPosLightSpace, norm, lightDir);
        } else if (lightType == 0 && light.attenuation.w > 0.0) {  // Point Light with shadow
            shadow = calculatePointShadow(fragPos, light.positionAndType.xyz, light.attenuation.w);
        }

        // Diffuse (Lambert)
        float diff = max(dot(norm, lightDir), 0.0);
        diffuse += diff * lightColor * lightIntensity * attenuation * shadow;

        // Specular (Phong reflection model)
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), ubo.shininess);
        specular += spec * lightColor * lightIntensity * attenuation * shadow;
    }

    // Combine lighting with base color
    vec3 result = (ambient + diffuse + specular) * baseColor;

    outColor = vec4(result, 1.0);
}
