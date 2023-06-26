#include <Primrose/core.hpp>

#include "player_movement.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

void update() {
	updatePosition();
	updateCamera();
}

int main() {
	Primrose::setup(APP_NAME, APP_VERSION);
	Primrose::mouseMovementCallback = mouseCallback;
	Primrose::run(update);
}


