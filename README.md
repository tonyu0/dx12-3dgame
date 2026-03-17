# dx12-3dgame

## Build setup


* build assimp
cd external/assimp
mkdir build
cmake -S . -B build
cmake --build build

* copy external/assimp/include to external/assimp/build/include
assimp include directory of this project is external/assimp/build/include
