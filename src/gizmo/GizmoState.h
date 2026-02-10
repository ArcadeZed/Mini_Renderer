#pragma once

// Transform gizmo operation modes
enum class GizmoMode {
    Translate,  // Move object (W key)
    Rotate,     // Rotate object (E key)
    Scale       // Scale object (R key)
};

// Gizmo interaction state
struct GizmoState {
    GizmoMode mode = GizmoMode::Translate;
    bool isActive = false;           // True when user is dragging the gizmo
    int selectedAxis = -1;           // -1=none, 0=X, 1=Y, 2=Z
    bool isDragging = false;         // True during drag operation
};
