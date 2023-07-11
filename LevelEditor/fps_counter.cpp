#include "fps_counter.hpp"
#include <Primrose/ui/text.hpp>
#include <Primrose/state.hpp>

using namespace Primrose;

static UIText* fpsText;
void initFps() {
	fpsText = new UIText({-0.999, -0.88});
	fpsText->text = "0";
	fpsText->alphabet = UIText::NUMERIC;
	fpsText->init("resources/arial.ttf", 48);
	uiScene.emplace_back(fpsText);
}

void updateFps() {
	static float lastTime = glfwGetTime(); // seconds
	static int frames = 0;
	static int fps = 0;

	float time = glfwGetTime();

	if (time - lastTime > 1) {
		int newFps = frames / (float)(time - lastTime);

		if (newFps != fps) {
			fps = newFps;
			fpsText->text = std::to_string(fps);
			fpsText->updateBuffers();
		}

		lastTime = glfwGetTime();
		frames = 0;
	}

	++frames;

	//	if (update) {
//		std::cout << "FPS(15): " << longFps;
//		std::cout << " | FPS(5): " << mediumFps;
//		std::cout << " | FPS(1): " << shortFps;
//		//std::cout << " | Fra(1): " << frameTimeAvgUs << " us";
//		//std::cout << " | Prs(1): " << fenceTimeAvgUs << " us";
//		std::cout << " | CPU(1): " << (float)cpuUtil / 100.f << "%	";
//		// CPU over 90% indicates cpu bottleneck
//
//		std::cout << std::flush << "\r";
//	}
}
