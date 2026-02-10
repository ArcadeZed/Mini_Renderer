#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 2) in mat4 fragView;
layout(location = 6) in mat4 fragProj;

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float ambientStrength;
    float shininess;
} ubo;

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = fragProj * fragView * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

// Returns grid line intensity [0, 1] for a given cell size
float gridLine(vec3 fragPos3D, float cellSize) {
    vec2 coord = fragPos3D.xz / cellSize;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    return 1.0 - min(line, 1.0);
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    if (t < 0.0) discard;

    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);
    gl_FragDepth = computeDepth(fragPos3D);

    // Camera height above grid determines the active mipmap level
    float camHeight = max(abs(ubo.viewPos.y), 0.001);
    float logHeight = log(camHeight) / log(10.0) - 0.4;

    float level = floor(logHeight);
    float blend = fract(logHeight);

    // Two adjacent grid cell sizes (powers of 10)
    float cellSize0 = pow(10.0, level);
    float cellSize1 = pow(10.0, level + 1.0);

    // Grid line intensity at both scales
    float line0 = gridLine(fragPos3D, cellSize0);
    float line1 = gridLine(fragPos3D, cellSize1);

    // Tight crossfade: predominantly one level visible at a time
    float fade0 = 1.0 - smoothstep(0.3, 0.7, blend);
    float fade1 = smoothstep(0.3, 0.7, blend);
    float alpha = max(line0 * fade0, line1 * fade1);

    // Grid base color
    vec3 gridColor = vec3(0.3);

    // Axis highlighting (scale-independent using screen-space derivatives)
    vec2 axisWidth = fwidth(fragPos3D.xz);
    if (abs(fragPos3D.x) < 1.5 * axisWidth.x)
        gridColor = vec3(0.0, 0.2, 1.0);  // Z-axis blue
    if (abs(fragPos3D.z) < 1.5 * axisWidth.y)
        gridColor = vec3(1.0, 0.2, 0.0);  // X-axis red

    // Distance fade scales with current grid level
    float dist = length(fragPos3D - ubo.viewPos);
    float fadeRange = cellSize1 * 30.0;
    float distFade = 1.0 - smoothstep(fadeRange * 0.5, fadeRange, dist);
    alpha *= distFade;

    if (alpha < 0.01) discard;

    outColor = vec4(gridColor, alpha);
}
