# Model Viewer & Animation System (DirectX 12 implementation)

## Overview
A 3D model viewer & animation system built from scratch using DirectX 12.

This project focuses on low-level graphics programming.

## Features

### Implemented
- Model loading via Assimp (FBX, glTF, OBJ, etc.)
- Root signature design
- Descriptor heap management
- Shadow mapping

### Planned
- GPU skinning (compute shader)
- Deferred rendering
- Physically Based Rendering (PBR)
- Image-Based Lighting (IBL)

### Current progress (v0.1.0)
<img width="682" height="452" alt="2026-03-18 (5)" src="https://github.com/user-attachments/assets/6c161ce7-5d1c-42cf-824f-0f6982593a07" />

A skinned animated model rendered with:
- basic lighting
- shadow mapping

## How to build

### 0. Requirements
- Windows 10/11
- Visual Studio 2022
- CMake

### 1. Setup
```bash
git clone --recursive https://github.com/tonyu0/model-viewer-dx12.git
cd model-viewer-dx12
```

### 2. Build assimp
```bash
cd external/assimp
mkdir build
cmake -S . -B build
# cmake --build build --config Debug # when a debug build is necessary
cmake --build build --config Release
```

### 3. Move necessary files
* Copy ```external/assimp/include``` to ```external/assimp/build/include```
> [!NOTE]
assimp include directory of this project is external/assimp/build/include


* Move assimp-vc145-mtd.dll to the same location as the project's binary files.

### 4. Open the solution file (.sln) and build in Visual Studio.
