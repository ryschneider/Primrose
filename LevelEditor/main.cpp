#include <Primrose/core.hpp>
#include <Primrose/ui/text.hpp>
#include <iostream>

#include "player_movement.hpp"
#include "fps_counter.hpp"
#include "../PrimroseDemo/planet_scene.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

uint selectedTool = PRIM_BOX;
std::map<uint, std::string> toolNames = {
	{PRIM_1, "PRIM_1"},
	{PRIM_2, "PRIM_2"},
	{PRIM_3, "PRIM_3"},
	{PRIM_4, "PRIM_4"},
//	{PRIM_5, "PRIM_5"},
//	{PRIM_6, "PRIM_6"},
//	{PRIM_7, "PRIM_7"},
//	{PRIM_8, "PRIM_8"},
//	{PRIM_9, "PRIM_9"},
	{PRIM_SPHERE, "PRIM_SPHERE"},
	{PRIM_BOX, "PRIM_BOX"},
	{PRIM_TORUS, "PRIM_TORUS"},
	{PRIM_LINE, "PRIM_LINE"},
	{PRIM_CYLINDER, "PRIM_CYLINDER"}
};
uint nextTool(uint tool) {
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

//	primitives[previewPrim].type = selectedTool;
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

//	previewPrim = addPrim(selectedTool);
//	previewTransform = addScale(1);
//	addRender(addIdentity(previewPrim));

	uint sphere = addPrim(PRIM_SPHERE);
	uint id = addIdentity(sphere);
	addRender(id);

	planetScene();

	initFps();

	std::cout << "Selected " << toolNames[selectedTool] << std::endl;
	run(update);
}
