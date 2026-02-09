# Textures Directory

This directory contains texture files used by the renderer.

## Supported Formats
- PNG (recommended)
- JPG
- BMP
- TGA

## Usage
Place your texture files here and load them using the `TextureLoader` class:

```cpp
TextureLoader loader;
auto texture = loader.load("textures/my_texture.png");
```

## Example Textures
For testing purposes, you can use procedural textures or create simple checkerboard patterns.

## Note
Texture files are not tracked in git by default. Add specific textures to the repository using:
```bash
git add -f textures/your_texture.png
```
