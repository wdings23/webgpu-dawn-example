# Build the app with CMake.
cmake -B build && cmake --build build -j4

# Build the app with Emscripten.
emcmake cmake -B build-web && cmake --build build-web -j4

# Run a server.
npx http-server --cors -p 8080

