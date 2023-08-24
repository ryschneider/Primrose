cmake_minimum_required(VERSION 3.25)
project(Primrose)

set(CMAKE_CXX_STANDARD 20)

## add warnings
#if (MSVC)
#	add_compile_options(/W4)
#else()
#	add_compile_options(-Wall -Wextra -Wpedantic -Werror)
#endif()

add_compile_options(-Wswitch-enum)
add_compile_options(-Wold-style-cast)

# build shader files
add_custom_command(OUTPUT
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/main_rgen_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/main_rint_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/main_rahit_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/main_rchit_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/main_rmiss_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/flat_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/march_frag_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/ui_vert_spv.h
	${PROJECT_SOURCE_DIR}/Primrose/src/embed/ui_frag_spv.h

	COMMAND bash -c "./shaders/buildshader.sh shaders/accelerated/main.rgen"
	COMMAND bash -c "./shaders/buildshader.sh shaders/accelerated/main.rint"
	COMMAND bash -c "./shaders/buildshader.sh shaders/accelerated/main.rahit"
	COMMAND bash -c "./shaders/buildshader.sh shaders/accelerated/main.rchit"
	COMMAND bash -c "./shaders/buildshader.sh shaders/accelerated/main.rmiss"
	COMMAND bash -c "./shaders/buildshader.sh shaders/raster/flat.vert"
	COMMAND bash -c "./shaders/buildshader.sh shaders/raster/march.frag"
	COMMAND bash -c "./shaders/buildshader.sh shaders/ui/ui.vert"
	COMMAND bash -c "./shaders/buildshader.sh shaders/ui/ui.frag"

	DEPENDS
	Primrose/shaders/raster/flat.vert Primrose/shaders/raster/march.frag
	Primrose/shaders/ui/ui.vert Primrose/shaders/ui/ui.frag
	Primrose/shaders/accelerated/main.rgen Primrose/shaders/accelerated/main.rint
	Primrose/shaders/accelerated/main.rahit Primrose/shaders/accelerated/main.rchit
	Primrose/shaders/accelerated/main.rmiss

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
	Primrose/src/embed/main_rgen_spv.h
	Primrose/src/embed/main_rint_spv.h
	Primrose/src/embed/main_rahit_spv.h
	Primrose/src/embed/main_rchit_spv.h
	Primrose/src/embed/main_rmiss_spv.h
	Primrose/src/embed/flat_vert_spv.h
	Primrose/src/embed/march_frag_spv.h
	Primrose/src/embed/ui_vert_spv.h
	Primrose/src/embed/ui_frag_spv.h
	Primrose/src/embed/scene_schema_json.h

	Primrose/src/engine/runtime.cpp Primrose/include/Primrose/engine/runtime.hpp
	Primrose/src/engine/setup.cpp Primrose/include/Primrose/engine/setup.hpp
	Primrose/src/engine/pipeline_accelerated.cpp Primrose/include/Primrose/engine/pipeline_accelerated.hpp
	Primrose/src/engine/pipeline_raster.cpp Primrose/include/Primrose/engine/pipeline_raster.hpp

	Primrose/src/ui/element.cpp Primrose/include/Primrose/ui/element.hpp
	Primrose/src/ui/image.cpp Primrose/include/Primrose/ui/image.hpp
	Primrose/src/ui/panel.cpp Primrose/include/Primrose/ui/panel.hpp
	Primrose/src/ui/text.cpp Primrose/include/Primrose/ui/text.hpp

	Primrose/src/state.cpp Primrose/include/Primrose/state.hpp
	Primrose/src/shader_structs.cpp Primrose/include/Primrose/shader_structs.hpp

	Primrose/src/scene/scene.cpp Primrose/include/Primrose/scene/scene.hpp
	Primrose/src/scene/node.cpp Primrose/include/Primrose/scene/node.hpp
	Primrose/src/scene/primitive_node.cpp Primrose/include/Primrose/scene/primitive_node.hpp
	Primrose/src/scene/construction_node.cpp Primrose/include/Primrose/scene/construction_node.hpp

	Primrose/include/Primrose/core.hpp
	Primrose/include/Primrose/scene/node_visitor.hpp

	Primrose/src/log.cpp Primrose/include/Primrose/log.hpp)

target_include_directories(Primrose SYSTEM PUBLIC C:/dev/include)
target_include_directories(Primrose SYSTEM PUBLIC C:/VulkanSDK/1.3.250.0/Include)
target_include_directories(Primrose SYSTEM PUBLIC C:/dev/include/freetype)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/include/Primrose)
target_include_directories(Primrose PRIVATE ${PROJECT_SOURCE_DIR}/Primrose/src)
target_include_directories(Primrose INTERFACE ${PROJECT_SOURCE_DIR}/Primrose/include)

target_link_libraries(Primrose C:/dev/lib_mingw/libglfw3.a)
target_link_libraries(Primrose C:/VulkanSDK/1.3.250.0/Lib/vulkan-1.lib)
target_link_libraries(Primrose C:/dev/lib_mingw/libfreetype.a)
target_link_libraries(Primrose C:/dev/lib_mingw/libfmt.a)
target_link_libraries(Primrose C:/dev/lib_mingw/libshaderc.a)



# build demo application
add_executable(PrimroseDemo PrimroseDemo/main.cpp LevelEditor/player_movement.cpp LevelEditor/fps_counter.cpp
	PrimroseDemo/planet_scene.cpp)
#target_include_directories(PrimroseDemo PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(PrimroseDemo LINK_PUBLIC Primrose)



set(IMGUI_DIR C:/dev/include/imgui)
set(TINYFD_DIR C:/dev/include/tinyfiledialogs)

# build level editor
add_executable(LevelEditor
	${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp

	${TINYFD_DIR}/tinyfiledialogs.c

	LevelEditor/player_movement.cpp LevelEditor/player_movement.hpp
	LevelEditor/fps_counter.cpp LevelEditor/fps_counter.hpp
	LevelEditor/gui.cpp LevelEditor/gui.hpp

	LevelEditor/main.cpp
	)
target_include_directories(LevelEditor SYSTEM PRIVATE ${IMGUI_DIR})
target_include_directories(LevelEditor SYSTEM PRIVATE ${TINYFD_DIR})
target_link_libraries(LevelEditor LINK_PUBLIC Primrose)
