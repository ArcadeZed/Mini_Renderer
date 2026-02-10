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
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture (ignore vertex colors, use texture only)
    vec3 baseColor = texture(texSampler, fragUV).rgb;

    // Normalize vectors
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(ubo.lightPos - fragPos);
    vec3 viewDir = normalize(ubo.viewPos - fragPos);

    // Distance-based attenuation
    float distance = length(ubo.lightPos - fragPos);
    float attenuation = 1.0 / (ubo.attenuationConstant +
                               ubo.attenuationLinear * distance +
                               ubo.attenuationQuadratic * (distance * distance));

    // Ambient (no attenuation - global light)
    vec3 ambient = ubo.ambientStrength * ubo.lightColor;

    // Diffuse (with attenuation)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * ubo.lightColor * ubo.lightIntensity * attenuation;

    // Specular (with attenuation) - Blinn-Phong
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), ubo.shininess);
    vec3 specular = spec * ubo.lightColor * ubo.lightIntensity * attenuation;

    // Combine lighting with base color (vertex color * texture)
    vec3 result = (ambient + diffuse + specular) * baseColor;

    outColor = vec4(result, 1.0);
}