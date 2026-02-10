#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 fragUV;

layout(binding = 0) uniform UniformBufferObject {
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
    float metalness;
    float roughness;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

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
    // Sample texture (ignore vertex colors, use texture only)
    vec3 baseColor = texture(texSampler, fragUV).rgb;

    // Normalize vectors
    vec3 N = normalize(fragNormal);
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
    // Dielectrics: ~0.04 (4% reflection), Metals: use baseColor
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, ubo.metalness);

    // Cook-Torrance BRDF
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Calculate D, F, G terms
    float D = DistributionGGX(N, H, ubo.roughness);
    vec3 F = FresnelSchlick(HdotV, F0);
    float G = GeometrySmith(N, V, L, ubo.roughness);

    // Cook-Torrance specular BRDF
    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.0001; // Avoid divide by zero
    vec3 specular = numerator / denominator;

    // Energy conservation: kS (specular) + kD (diffuse) = 1
    vec3 kS = F;  // Fresnel tells us how much is reflected
    vec3 kD = vec3(1.0) - kS;  // Remaining is refracted (diffuse)
    kD *= (1.0 - ubo.metalness);  // Metals have no diffuse

    // Lambert diffuse
    vec3 diffuse = kD * baseColor / PI;

    // Final lighting (simplified - single light)
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient (very simple - replace with IBL for full PBR)
    vec3 ambient = vec3(0.03) * baseColor;

    vec3 color = ambient + Lo;

    // Gamma correction (simple)
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
