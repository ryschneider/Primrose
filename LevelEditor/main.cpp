#include <Primrose/core.hpp>
#include <Primrose/ui.hpp>

#include "player_movement.hpp"
#include "fps_counter.hpp"
#include "../PrimroseDemo/planet_scene.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

void update(float dt) {
	updatePosition();
	updateFps();
}

uint selectedTool = PRIM_BOX;

//uint nextTool(uint tool) {
//	switch(tool) {
//		case PRIM_1:
//			return PRIM_2;
//	}
//}

int main() {
	setup(APP_NAME, APP_VERSION);
	mouseMovementCallback = mouseCallback;

	planetScene();

	initFps();

	run(update);
}
