#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in vec3 fragTangent;
layout(location = 5) in vec3 fragBitangent;
layout(location = 6) flat in int fragMaterialIndex;
layout(location = 7) in vec4 fragPosLightSpace;  // Position in light space

// GPU-aligned light structure (matches C++ GPULight)
struct GPULight {
    vec4 positionAndType;   // xyz = position, w = type (0=Point, 1=Directional, 2=Spot)
    vec4 colorAndIntensity; // rgb = color, a = intensity
    vec4 directionAndRange; // xyz = direction (normalized), w = range
    vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = unused
};

// Global uniform buffer (Set 0, Binding 0)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    int lightCount;         // Number of active lights (0-8)
    float ambientStrength;
    float shininess;
    float metalness;        // Fallback if no material buffer
    float roughness;        // Fallback if no material buffer
    GPULight lights[8];     // Array of lights
    mat4 lightSpaceMatrix;  // Light space transform for shadow mapping
} ubo;

// Shadow map sampler (Set 0, Binding 2)
layout(set = 0, binding = 2) uniform sampler2D shadowMap;

// Material storage buffer (Set 1, Binding 0)
struct GPUMaterial {
    vec4 baseColorFactor;            // RGB + Alpha
    vec4 metallicRoughnessEmissive;  // x=metallic, y=roughness, z=normalScale, w=alphaMode
    vec3 emissiveFactor;             // RGB
    float alphaCutoff;               // Alpha cutoff for Alpha_Mask mode
    ivec4 textureIndices;            // x=baseColor, y=normal, z=metallicRoughness, w=emissive
};

layout(set = 1, binding = 0, std430) readonly buffer MaterialBuffer {
    GPUMaterial materials[];
} materialBuffer;

// Texture array (Set 1, Binding 1)
layout(set = 1, binding = 1) uniform sampler2D textures[128];  // Max 128 textures

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// D (Distribution): Trowbridge-Reitz GGX
// Models the distribution of microfacet orientations
// Rougher surface → wider highlight distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

// F (Fresnel): Schlick approximation
// Models how much light is reflected vs refracted
// At grazing angles (cosTheta → 0), more reflection
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// G (Geometry): Schlick-GGX
// Models self-shadowing and masking of microfacets
// Rougher surface → more self-occlusion
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;

    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

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

void main() {
    // Fetch material from buffer
    GPUMaterial material = materialBuffer.materials[fragMaterialIndex];

    // Sample textures
    vec3 baseColor = material.baseColorFactor.rgb;
    float metallic = material.metallicRoughnessEmissive.x;
    float roughness = material.metallicRoughnessEmissive.y;
    float normalScale = material.metallicRoughnessEmissive.z;

    // Sample base color texture if available (including alpha channel)
    float alpha = material.baseColorFactor.a;
    if (material.textureIndices.x >= 0) {
        vec4 texColor = texture(textures[material.textureIndices.x], fragUV);
        baseColor *= texColor.rgb;
        alpha *= texColor.a;  // Multiply texture alpha with material alpha
    }

    // Sample metallic/roughness texture if available (G=Roughness, B=Metallic)
    if (material.textureIndices.z >= 0) {
        vec4 mrTex = texture(textures[material.textureIndices.z], fragUV);
        roughness *= mrTex.g;  // Green channel = Roughness
        metallic *= mrTex.b;   // Blue channel = Metallic
    }

    // Construct TBN matrix for normal mapping
    mat3 TBN = mat3(normalize(fragTangent), normalize(fragBitangent), normalize(fragNormal));

    // Sample normal map and transform to world space
    vec3 N = normalize(fragNormal);
    if (material.textureIndices.y >= 0) {
        vec3 tangentNormal = texture(textures[material.textureIndices.y], fragUV).xyz * 2.0 - 1.0;
        tangentNormal.xy *= normalScale;
        N = normalize(TBN * tangentNormal);
    }

    // View direction (same for all lights)
    vec3 V = normalize(ubo.viewPos - fragPos);

    // Calculate F0 (base reflectivity)
    // Dielectrics: ~0.04, Metals: baseColor
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, metallic);

    // Accumulate lighting from all lights
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < ubo.lightCount && i < 8; ++i) {
        GPULight light = ubo.lights[i];
        int lightType = int(light.positionAndType.w);

        // Light direction based on type
        vec3 L;
        float attenuation = 1.0;

        if (lightType == 0) {  // Point Light
            vec3 lightPos = light.positionAndType.xyz;
            L = normalize(lightPos - fragPos);

            // Distance-based attenuation
            float distance = length(lightPos - fragPos);
            if (distance > light.directionAndRange.w) {
                continue;  // Beyond light range, skip this light
            }
            attenuation = 1.0 / (light.attenuation.x +
                                 light.attenuation.y * distance +
                                 light.attenuation.z * distance * distance);
        } else if (lightType == 1) {  // Directional Light
            L = normalize(-light.directionAndRange.xyz);
            attenuation = 1.0;  // No attenuation for directional lights
        } else {  // Spot Light (future)
            continue;  // Skip for now
        }

        vec3 H = normalize(V + L);

        // Radiance for this light
        vec3 radiance = light.colorAndIntensity.rgb * light.colorAndIntensity.a * attenuation;

        // Cook-Torrance BRDF
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        // D, F, G terms
        float D = DistributionGGX(N, H, roughness);
        vec3 F = FresnelSchlick(HdotV, F0);
        float G = GeometrySmith(N, V, L, roughness);

        // Specular BRDF
        vec3 numerator = D * F * G;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        // Energy conservation
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);  // Metals have no diffuse

        // Lambert diffuse
        vec3 diffuse = kD * baseColor / PI;

        // Shadow calculation (only for directional lights for now)
        float shadow = 1.0;  // Default: no shadow (fully lit)
        if (lightType == 1) {  // Directional Light
            shadow = calculateShadow(fragPosLightSpace, N, L);
        }

        // Accumulate this light's contribution (attenuated by shadow)
        Lo += (diffuse + specular) * radiance * NdotL * shadow;
    }

    // Ambient + emissive (independent of lights)
    vec3 ambient = ubo.ambientStrength * baseColor;
    vec3 emissive = material.emissiveFactor;
    if (material.textureIndices.w >= 0) {
        emissive *= texture(textures[material.textureIndices.w], fragUV).rgb;
    }

    vec3 color = ambient + Lo + emissive;

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    // Alpha handling based on alphaMode
    int alphaMode = int(material.metallicRoughnessEmissive.w);
    if (alphaMode == 1) {  // Alpha_Mask
        if (alpha < material.alphaCutoff) {
            discard;  // Discard fragments below cutoff
        }
        alpha = 1.0;  // Surviving fragments are fully opaque
    } else if (alphaMode == 2) {  // Alpha_Blend
        // Use alpha as-is for blending
    } else {  // Alpha_Opaque (0)
        alpha = 1.0;  // Fully opaque
    }

    outColor = vec4(color, alpha);
}
