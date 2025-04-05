file(GLOB SOURCES "*.cpp" "*.h") # Or other source file extensions
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})