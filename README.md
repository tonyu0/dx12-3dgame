# DirectX12 renderer implementation from scratch.

## Features
- FBX model loading
- Root signature design
- Descriptor heap management
- Shadow mapping
- GPU skinning (planned)
- Deferred rendering (planned)
- PBR (planned)
- IBL (planned)

## Build setup


* build assimp
cd external/assimp
mkdir build
cmake -S . -B build
cmake --build build

* copy external/assimp/include to external/assimp/build/include
assimp include directory of this project is external/assimp/build/include
