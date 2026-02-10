#version 450

// Input: Lines (2 vertices per line segment)
layout(lines) in;

// Output: Triangle strip (4 vertices = quad with constant screen-space thickness)
layout(triangle_strip, max_vertices = 4) out;

// Input from Vertex Shader
layout(location = 0) in vec3 inColor[];

// Output to Fragment Shader
layout(location = 0) out vec3 fragColor;

// Push constants for viewport size
layout(push_constant) uniform PushConstants {
    vec2 viewportSize;  // e.g. (1920.0, 1080.0)
    float lineWidth;    // Line width in pixels (e.g. 3.0)
} pushConstants;

void main() {
    // Get the two line endpoints in clip space
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    vec3 c0 = inColor[0];
    vec3 c1 = inColor[1];

    // Clip line to w >= W_NEAR before any screen-space calculations.
    // Without this, vertices behind the camera (w < 0) cause a "bowtie" quad:
    // per-vertex W flips the offset direction, putting two quad vertices on the
    // SAME side of the line. Hardware near-plane clipping then interpolates between
    // two large same-direction offsets, producing lines thousands of pixels wide.
    const float W_NEAR = 0.001;

    // Both behind camera -> invisible
    if (p0.w < W_NEAR && p1.w < W_NEAR) {
        return;
    }

    // Clip the behind-camera vertex to w = W_NEAR
    if (p0.w < W_NEAR) {
        float t = (W_NEAR - p0.w) / (p1.w - p0.w);
        p0 = mix(p0, p1, t);
        c0 = mix(c0, c1, t);
    } else if (p1.w < W_NEAR) {
        float t = (W_NEAR - p1.w) / (p0.w - p1.w);
        p1 = mix(p1, p0, t);
        c1 = mix(c1, c0, t);
    }

    // Now both vertices have w > 0: safe for perspective division
    vec2 p0_ndc = p0.xy / p0.w;
    vec2 p1_ndc = p1.xy / p1.w;

    // Convert NDC to screen-space pixels
    vec2 p0_screen = (p0_ndc * 0.5 + 0.5) * pushConstants.viewportSize;
    vec2 p1_screen = (p1_ndc * 0.5 + 0.5) * pushConstants.viewportSize;

    // Calculate direction in screen space (pixels)
    vec2 dir_screen = p1_screen - p0_screen;
    float dirLength = length(dir_screen);

    // Skip degenerate lines (division by zero guard)
    if (dirLength < 0.00001) {
        return;
    }

    // Perpendicular normal (90 degree rotation), normalized
    vec2 normal_screen = vec2(-dir_screen.y, dir_screen.x) / dirLength;

    // Scale by line width (in pixels), convert to clip space per-vertex
    vec2 offset_screen = normal_screen * pushConstants.lineWidth * 0.5;
    vec2 offset_ndc = (offset_screen / pushConstants.viewportSize) * 2.0;
    vec2 offset_p0 = offset_ndc * p0.w;
    vec2 offset_p1 = offset_ndc * p1.w;

    // Emit 4 vertices to form a quad (triangle strip)
    gl_Position = vec4(p0.xy + offset_p0, p0.z, p0.w);
    fragColor = c0;
    EmitVertex();

    gl_Position = vec4(p0.xy - offset_p0, p0.z, p0.w);
    fragColor = c0;
    EmitVertex();

    gl_Position = vec4(p1.xy + offset_p1, p1.z, p1.w);
    fragColor = c1;
    EmitVertex();

    gl_Position = vec4(p1.xy - offset_p1, p1.z, p1.w);
    fragColor = c1;
    EmitVertex();

    EndPrimitive();
}
