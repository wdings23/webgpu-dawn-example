# webgpu-dawn-example

# WebGPU implementation of simple mesh viewer with zooming, panning, rotation, and hide/reveal functionalty. Native WebGPU uses Chromium's Dawn that has support for MultiDrawIndirect, rendering meshes in one call. Fallback for emscripten build is loop through drawIndexedIndirect with the number of meshes.

# Build the app with CMake.
cmake -B build && cmake --build build -j4

# Build the app with Emscripten.
emcmake cmake -B build-web && cmake --build build-web -j4

# Mesh Assets are built separately, converting OBJ files to binary format for faster loading. It uses a local version of obj2binary application. Will post that here soon.

# Controls
Keyboard Button
    W - Move forward
    A - Strafe left
    S - Move backward
    D - Strafe right
    H - Hide selected mesh (mesh with yellow highlight)
    J - Reveal last hidden mesh

Mouse
    Left click - selects mesh, rotate camera if held down with mouse movement 
    Right Click - pans the camera if held down with mouse movement 

# http server for assets
npx http-server --cors -p 8080

# http server for web version
npx http-server -p 8000

# local url
http://localhost:8000/app.html

