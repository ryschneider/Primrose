cmake_minimum_required(VERSION 3.25)
project(Primrose)

set(CMAKE_CXX_STANDARD 17)

# build shader files
add_custom_target(build_shaders ALL
	COMMAND bash -c "./Primrose/shaders/buildshader.sh vert"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh frag"
	DEPENDS Primrose/shaders/main.vert Primrose/shaders/main.frag
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	COMMENT "Compiling shaders")
set_property(SOURCE ${PROJECT_SOURCE_DIR}/Primrose/shaders/vert_spv.c PROPERTY GENERATED TRUE)
set_property(SOURCE ${PROJECT_SOURCE_DIR}/Primrose/shaders/frag_spv.c PROPERTY GENERATED TRUE)

# build primrose library
add_library(Primrose
	Primrose/shaders/vert_spv.c Primrose/shaders/frag_spv.c
	Primrose/engine_runtime.cpp Primrose/engine.cpp Primrose/scene.cpp Primrose/state.cpp Primrose/shader_structs.cpp)
target_include_directories(Primrose PUBLIC C:/dev/include)
target_include_directories(Primrose PUBLIC C:/VulkanSDK/1.3.250.0/Include)
target_link_libraries(Primrose C:/dev/lib_mingw/libglfw3.a)
target_link_libraries(Primrose C:/VulkanSDK/1.3.250.0/Lib/vulkan-1.lib)
add_dependencies(Primrose build_shaders)

# delete generated shader files
add_custom_target(delete_shaders ALL
	COMMAND rm Primrose/shaders/frag.spv
	COMMAND rm Primrose/shaders/frag_spv.h
	COMMAND rm Primrose/shaders/frag_spv.c
	COMMAND rm Primrose/shaders/vert.spv
	COMMAND rm Primrose/shaders/vert_spv.h
	COMMAND rm Primrose/shaders/vert_spv.c
	DEPENDS Primrose
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	COMMENT "Deleting shaders")

# build demo application
add_executable(PrimroseDemo PrimroseDemo/main.cpp)
target_include_directories(PrimroseDemo PUBLIC Primrose/)
target_link_libraries(PrimroseDemo LINK_PUBLIC Primrose)
