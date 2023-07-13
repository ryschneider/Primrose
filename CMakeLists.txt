cmake_minimum_required(VERSION 3.25)
project(Primrose)

set(CMAKE_CXX_STANDARD 17)

## add warnings
#if (MSVC)
#	add_compile_options(/W4)
#else()
#	add_compile_options(-Wall -Wextra -Wpedantic -Werror)
#endif()

# build shader files
add_custom_command(OUTPUT
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/flat_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/march_frag_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/ui_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/ui_frag_spv.h
	COMMAND bash -c "./shaders/buildshader.sh shaders/flat.vert"
	COMMAND bash -c "./shaders/buildshader.sh shaders/march.frag"
	COMMAND bash -c "./shaders/buildshader.sh shaders/ui.vert"
	COMMAND bash -c "./shaders/buildshader.sh shaders/ui.frag"
	DEPENDS Primrose/shaders/flat.vert Primrose/shaders/march.frag Primrose/shaders/ui.vert Primrose/shaders/ui.frag
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Primrose
	COMMENT "Compiling shaders")

# embed json schema
add_custom_command(OUTPUT
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/scene_schema_json.h
	COMMAND bash -c "./embed.sh scene.schema.json char"
	DEPENDS Primrose/scene.schema.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Primrose
	COMMENT "Embedding json schema")



set(CMAKE_EXE_LINKER_FLAGS "-static")

# build primrose library
add_library(Primrose
	Primrose/src/embed/flat_vert_spv.h
	Primrose/src/embed/march_frag_spv.h
	Primrose/src/embed/ui_vert_spv.h
	Primrose/src/embed/ui_frag_spv.h
	Primrose/src/embed/scene_schema_json.h
	Primrose/src/engine/runtime.cpp Primrose/src/engine/setup.cpp Primrose/src/scene.cpp Primrose/src/state.cpp
	Primrose/src/ui/element.cpp Primrose/src/ui/panel.cpp Primrose/src/ui/image.cpp Primrose/src/ui/text.cpp
	Primrose/src/shader_structs.cpp)

target_include_directories(Primrose PUBLIC C:/dev/include)
target_include_directories(Primrose PUBLIC C:/VulkanSDK/1.3.250.0/Include)
target_include_directories(Primrose PUBLIC C:/dev/include/freetype)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/include/Primrose)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/src)
target_include_directories(Primrose INTERFACE ${PROJECT_SOURCE_DIR}/Primrose/include)

target_link_libraries(Primrose C:/dev/lib_mingw/libglfw3.a)
target_link_libraries(Primrose C:/VulkanSDK/1.3.250.0/Lib/vulkan-1.lib)
target_link_libraries(Primrose C:/dev/lib_mingw/libfreetype.a)
target_link_libraries(Primrose C:/dev/lib_mingw/libfmt.a)



# build demo application
add_executable(PrimroseDemo PrimroseDemo/main.cpp LevelEditor/player_movement.cpp LevelEditor/fps_counter.cpp
	PrimroseDemo/planet_scene.cpp)
#target_include_directories(PrimroseDemo PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(PrimroseDemo LINK_PUBLIC Primrose)



# build level editor
add_executable(LevelEditor LevelEditor/main.cpp LevelEditor/player_movement.cpp LevelEditor/fps_counter.cpp)
#target_include_directories(LevelEditor PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(LevelEditor LINK_PUBLIC Primrose)
