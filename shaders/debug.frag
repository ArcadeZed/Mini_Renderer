#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Unlit rendering - just output the vertex color at full brightness
    // No texture sampling, no lighting calculations
    outColor = vec4(fragColor, 1.0);
}
