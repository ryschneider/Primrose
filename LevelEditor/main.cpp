#include <Primrose/core.hpp>
#include <Primrose/ui/text.hpp>
#include <iostream>

#include "player_movement.hpp"
#include "fps_counter.hpp"
//#include "../PrimroseDemo/planet_scene.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

PRIM selectedTool = PRIM::BOX;
std::map<PRIM, std::string> toolNames = {
	{PRIM::P1, "PRIM::P1"},
	{PRIM::P2, "PRIM::P2"},
	{PRIM::P3, "PRIM::P3"},
	{PRIM::P4, "PRIM::P4"},
//	{PRIM::P5, "PRIM::P5"},
//	{PRIM::P6, "PRIM::P6"},
//	{PRIM::P7, "PRIM::P7"},
//	{PRIM::P8, "PRIM::P8"},
//	{PRIM::P9, "PRIM::P9"},
	{PRIM::SPHERE, "PRIM::SPHERE"},
	{PRIM::BOX, "PRIM::BOX"},
	{PRIM::TORUS, "PRIM::TORUS"},
	{PRIM::LINE, "PRIM::LINE"},
	{PRIM::CYLINDER, "PRIM::CYLINDER"}
};
PRIM nextTool(PRIM tool) {
	bool next = false;
	for (const auto& pair : toolNames) {
		if (next) return pair.first;
		if (pair.first == tool) next = true;
	}
	return (*(toolNames.begin())).first;
}

float placeDist = 10;
unsigned int previewPrim;
unsigned int previewTransform;

void update(float dt) {
	updatePosition();
	updateFps();

//	primitives[previewPrim].nodeType = selectedTool;
//
//	glm::vec3 position = uniforms.camPos + uniforms.camDir * placeDist;
//	glm::mat4 transform = transformMatrix(position, glm::vec3(1), 0, glm::vec3(1));
//	transformations[previewTransform].invMatrix = glm::inverse(transform);
//	transformations[previewTransform].smallScale = getSmallScale(transform);
}

void myKeyCallback(int key, bool pressed) {
	if (key == GLFW_KEY_T && pressed) {
		selectedTool = nextTool(selectedTool);
		std::cout << "Selected " << toolNames[selectedTool] << std::endl;
	}
}

int main() {
	setup(APP_NAME, APP_VERSION);
	mouseMovementCallback = mouseCallback;
	keyCallback = myKeyCallback;

	Scene scene("scenes/test.json");
	scene.generateUniforms();

	std::cout << std::endl << "SCENE:" << std::endl << scene.toString() << std::endl;
	std::cout << std::endl << "UNIFORMS:" << std::endl << uniforms.toString() << std::endl;

	initFps();

	std::cout << "Selected " << toolNames[selectedTool] << std::endl;
	run(update);
}
