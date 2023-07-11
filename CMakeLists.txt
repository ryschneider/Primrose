cmake_minimum_required(VERSION 3.25)
project(Primrose)

set(CMAKE_CXX_STANDARD 17)

# build shader files
add_custom_command(OUTPUT
	Primrose/src/embed/flat_vert_spv.h
	Primrose/src/embed/march_frag_spv.h
	Primrose/src/embed/ui_vert_spv.h
	Primrose/src/embed/ui_frag_spv.h
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/flat.vert"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/march.frag"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/ui.vert"
	COMMAND bash -c "./Primrose/shaders/buildshader.sh Primrose/shaders/ui.frag"
	DEPENDS Primrose/shaders/flat.vert Primrose/shaders/march.frag Primrose/shaders/ui.vert Primrose/shaders/ui.frag
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	COMMENT "Compiling shaders")

set(CMAKE_EXE_LINKER_FLAGS "-static")
#target_link_libraries(your_target_name -static)

# build primrose library
add_library(Primrose
	Primrose/src/engine/runtime.cpp Primrose/src/engine/setup.cpp Primrose/src/scene.cpp Primrose/src/state.cpp
	Primrose/src/ui/element.cpp Primrose/src/ui/panel.cpp Primrose/src/ui/image.cpp Primrose/src/ui/text.cpp)
target_include_directories(Primrose PUBLIC C:/dev/include)
target_include_directories(Primrose PUBLIC C:/VulkanSDK/1.3.250.0/Include)
target_include_directories(Primrose PUBLIC C:/dev/include/freetype)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/include/Primrose)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/src)
target_include_directories(Primrose INTERFACE ${PROJECT_SOURCE_DIR}/Primrose/include)

target_link_libraries(Primrose C:/dev/lib_mingw/libglfw3.a)
target_link_libraries(Primrose C:/VulkanSDK/1.3.250.0/Lib/vulkan-1.lib)
target_link_libraries(Primrose C:/dev/lib_mingw/libfreetype.a)

# build demo application
add_executable(PrimroseDemo PrimroseDemo/main.cpp LevelEditor/player_movement.cpp LevelEditor/fps_counter.cpp
	PrimroseDemo/planet_scene.cpp)
#target_include_directories(PrimroseDemo PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(PrimroseDemo LINK_PUBLIC Primrose)

# build level editor
add_executable(LevelEditor LevelEditor/main.cpp LevelEditor/player_movement.cpp LevelEditor/fps_counter.cpp
	PrimroseDemo/planet_scene.cpp)
#target_include_directories(LevelEditor PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(LevelEditor LINK_PUBLIC Primrose)
