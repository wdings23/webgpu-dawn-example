file(GLOB SOURCES "*.c") # Or other source file extensions
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})