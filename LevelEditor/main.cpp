#include <Primrose/core.hpp>

#include "player_movement.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

void update() {
	updatePosition();
}

int main() {
	setup(APP_NAME, APP_VERSION);
	mouseMovementCallback = mouseCallback;

	uniforms.primitives[0] = Prim(PRIM_BOX);
	addTranslate(0, 0, 5);
	addIdentity(0, true);

	run(update);
}
