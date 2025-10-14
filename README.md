# MiniRenderer (Windows Vulkan)

## Setup
### 1. Vulkan SDK installieren:  
   https://vulkan.lunarg.com/sdk/home#windows

### 2. Repo clonen:
```bash
git clone --recursive https://github.com/ArcadeZed/Mini_Renderer.git
cd mini-renderer
```

### 3. CMake Projekt erstellen: 
```bash
mkdir build  
cd build  
cmake ..  
cmake --build . --config Release
```

### 4. Ausführen:
```bash
./Release/MiniRenderer.exe
```

## Roadmap
- Phase 1: Hello Triangle
- Phase 2: Mesh + Camera
- Phase 3: PBR + IBL
- Phase 4: Shadows + Post
- Phase 5: Polishing