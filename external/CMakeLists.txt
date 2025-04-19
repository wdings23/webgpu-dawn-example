file(GLOB SOURCES "*.cpp" "*.h" "*.c") # Or other source file extensions
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})