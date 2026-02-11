#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in vec3 fragTangent;
layout(location = 5) in vec3 fragBitangent;
layout(location = 6) flat in int fragMaterialIndex;

// Global uniform buffer (Set 0, Binding 0)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float ambientStrength;
    float shininess;
    float lightIntensity;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
    float metalness;   // Fallback if no material buffer
    float roughness;   // Fallback if no material buffer
} ubo;

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

    // View and light directions
    vec3 V = normalize(ubo.viewPos - fragPos);
    vec3 L = normalize(ubo.lightPos - fragPos);
    vec3 H = normalize(V + L);

    // Distance-based attenuation
    float distance = length(ubo.lightPos - fragPos);
    float attenuation = 1.0 / (ubo.attenuationConstant +
                               ubo.attenuationLinear * distance +
                               ubo.attenuationQuadratic * (distance * distance));
    vec3 radiance = ubo.lightColor * ubo.lightIntensity * attenuation;

    // Calculate F0 (base reflectivity)
    // Dielectrics: ~0.04, Metals: baseColor
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, metallic);

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

    // Final lighting
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient + emissive
    vec3 ambient = vec3(0.03) * baseColor;
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
