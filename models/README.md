# Mesh File Format

## Format

Das Mesh-Format ist ein einfaches Textformat, das du mit jedem Texteditor bearbeiten kannst.

### Struktur

```
VERTICES
x y z  r g b  nx ny nz  u v
x y z  r g b  nx ny nz  u v
...

INDICES
i0 i1 i2
i3 i4 i5
...
```

### Felder pro Vertex

- **Position (x, y, z)**: 3D-Position des Vertex
- **Color (r, g, b)**: RGB-Farbe (0.0 bis 1.0)
- **Normal (nx, ny, nz)**: Normalenvektor für Lighting
- **UV (u, v)**: Texturkoordinaten (0.0 bis 1.0)

### Indices

- Die Indices referenzieren die Vertices (0-basiert)
- Jeweils 3 Indices bilden ein Dreieck

## Verfügbare Meshes

- **triangle.txt** - Einfaches Dreieck
- **quad.txt** - Quadrat aus 2 Dreiecken
- **axes.txt** - 3D Koordinatensystem (X=Rot, Y=Grün, Z=Blau)
- **triangle_with_axes.txt** - Dreieck + Koordinatensystem zur Orientierung

## Beispiele

### Dreieck (triangle.txt)
```
VERTICES
-0.5  0.5 -0.5  1.0 0.0 0.0  0.371 -0.557 -0.743  0.0 0.0
 0.5  0.5  0.0  0.0 1.0 0.0  0.371 -0.557 -0.743  1.0 0.0
 0.0 -0.5  0.5  0.0 0.0 1.0  0.371 -0.557 -0.743  0.5 1.0

INDICES
0 1 2
```

### Quad (2 Dreiecke)
```
VERTICES
-0.5 -0.5  0.0  1.0 0.0 0.0  0.0 0.0 1.0  0.0 0.0
 0.5 -0.5  0.0  0.0 1.0 0.0  0.0 0.0 1.0  1.0 0.0
 0.5  0.5  0.0  0.0 0.0 1.0  0.0 0.0 1.0  1.0 1.0
-0.5  0.5  0.0  1.0 1.0 0.0  0.0 0.0 1.0  0.0 1.0

INDICES
0 1 2
2 3 0
```

### Koordinatensystem zur Orientierung

Lade `triangle_with_axes.txt` für ein Dreieck mit sichtbaren Achsen:

```
# In Renderer.cpp, Zeile 117:
loadMeshFromFile("models/triangle_with_axes.txt");
```

Die Achsen zeigen dir:
- **Rot** = X-Achse (nach rechts)
- **Grün** = Y-Achse (nach oben)
- **Blau** = Z-Achse (zur Kamera hin/von Kamera weg)

## Verwendung

### Mit Hot Reloading (Programm läuft):
1. Bearbeite `models/triangle.txt` während das Programm läuft
2. Speichern → Änderungen sind nach ~0.5s sichtbar!

### Anderes Mesh laden:
1. Ändere in `Renderer.cpp` → `initVulkan()` (Zeile 117):
   ```cpp
   loadMeshFromFile("models/triangle_with_axes.txt");  // Mit Koordinatensystem
   ```
2. Neu kompilieren und starten

## Tipps

- Nutze Kommentare mit `#` für Notizen
- Achte auf korrekte Formatierung (10 Zahlen pro Vertex-Zeile)
- Stelle sicher, dass alle Indices gültig sind (< Anzahl Vertices)
- Normals sollten normalisiert sein (Länge = 1.0)
