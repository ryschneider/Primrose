cmake_minimum_required(VERSION 3.25)
project(Primrose)

set(CMAKE_CXX_STANDARD 17)

# build shader files
add_custom_command(OUTPUT
	${PROJECT_SOURCE_DIR}/Primrose/embed/flat_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/embed/march_frag_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/embed/ui_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/embed/ui_frag_spv.h
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/flat.vert"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/march.frag"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/ui.vert"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/ui.frag"
	DEPENDS Primrose/shaders/flat.vert Primrose/shaders/march.frag Primrose/shaders/ui.vert Primrose/shaders/ui.frag
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	COMMENT "Compiling shaders")

# build primrose library
add_library(Primrose
	Primrose/embed/flat_vert_spv.h
	Primrose/embed/march_frag_spv.h
	Primrose/embed/ui_vert_spv.h
	Primrose/embed/ui_frag_spv.h
	Primrose/engine_runtime.cpp Primrose/engine.cpp Primrose/scene.cpp
	Primrose/state.cpp Primrose/shader_structs.cpp)
target_include_directories(Primrose PUBLIC C:/dev/include)
target_include_directories(Primrose PUBLIC C:/VulkanSDK/1.3.250.0/Include)
target_link_libraries(Primrose C:/dev/lib_mingw/libglfw3.a)
target_link_libraries(Primrose C:/VulkanSDK/1.3.250.0/Lib/vulkan-1.lib)

# build demo application
add_executable(PrimroseDemo PrimroseDemo/main.cpp LevelEditor/player_movement.cpp)
target_include_directories(PrimroseDemo PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(PrimroseDemo LINK_PUBLIC Primrose)

# build level editor
add_executable(LevelEditor LevelEditor/main.cpp LevelEditor/player_movement.cpp)
target_include_directories(LevelEditor PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(LevelEditor LINK_PUBLIC Primrose)
