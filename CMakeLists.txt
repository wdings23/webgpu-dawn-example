cmake_minimum_required(VERSION 3.13) # CMake version check
project(app)                         # Create project "app"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

add_executable(app "main.cpp")

add_subdirectory(loader)
add_subdirectory(math)
add_subdirectory(render)
add_subdirectory(utils)
add_subdirectory(external/tinyexr)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/external)

file(GLOB_RECURSE LOADER_DIR 
  loader/*.cpp*
  loader/*.h*
)
source_group("loader" FILES ${LOADER_DIR})

file(GLOB_RECURSE MATH_DIR 
  math/*.cpp*
  math/*.h*
)
source_group("math" FILES ${MATH_DIR})

file(GLOB_RECURSE RENDER_DIR 
  render/*.cpp*
  render/*.h*
)
source_group("render" FILES ${RENDER_DIR})

file(GLOB_RECURSE UTILS_DIR 
  utils/*.cpp*
  utils/*.h*
)
source_group("utils" FILES ${UTILS_DIR})

file(GLOB_RECURSE EXTERNAL_DIR 
  external/*.cpp*
  external/*.h*
  external/*.c*
)
source_group("external" FILES ${EXTERNAL_DIR})

if(EMSCRIPTEN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -g -O3"
  )
  target_compile_definitions(app PUBLIC EMBEDDED_FILES=1)
  set_target_properties(app PROPERTIES LINK_FLAGS "--embed-file ../shaders --embed-file ../render-jobs --embed-file ../assets")
  set_target_properties(app PROPERTIES SUFFIX ".html")
  target_link_options(app PRIVATE 
    "-sUSE_WEBGPU=1" 
    "-sUSE_GLFW=3" 
    "-sASYNCIFY" 
    #"-sMODULARIZE" 
    #"-sEXPORTED_RUNTIME_METHODS=['emscripten_fetch']"
    #"-sEXPORT_ES6=1" 
    "-sALLOW_MEMORY_GROWTH"
    "-sINITIAL_MEMORY=327680000"
    "-sENVIRONMENT='worker'" 
    "-sFETCH=1"
    #"-sUSE_PTHREADS=1"
    "-sASSERTIONS"
    "-sSTACK_SIZE=1048576"
    "-sEXPORTED_FUNCTIONS=_main,_toggleOutlineRender,_setSwapChainRender,_setAmbientOcclusionSampleRadius,_setAmbientOcclusionThickness,_setExplodePct,_hideSelection,_showLastHidden,_setCrossSectionPlane,_toggleProjectionType"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
  )
else()
  find_package(CURL REQUIRED)

  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("dawn" EXCLUDE_FROM_ALL)
  target_link_libraries(app PRIVATE dawn::webgpu_dawn glfw webgpu_glfw CURL::libcurl)
endif()
