#include <Primrose/core.hpp>
#include <Primrose/ui/text.hpp>
#include <Primrose/ui/panel.hpp>
#include <iostream>
#include <glm/gtx/rotate_vector.hpp>

#include "player_movement.hpp"
#include "fps_counter.hpp"
#include "gui.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

Scene mainScene = Scene();

bool mouseMiddleDown = false;
bool shiftHeld = false;

static float orbitDist = 10;
static glm::vec3 pivot = glm::vec3(0);

void update(float dt) {
	if (doubleClickedNode != nullptr) {
		glm::mat4 transform = doubleClickedNode->modelMatrix();

		pivot = getTranslate(transform);
		uniforms.camPos = pivot - uniforms.camDir * orbitDist;

		doubleClickedNode = nullptr;
	}

	bool updatedScene = updateGui(mainScene, dt);
	if (updatedScene) {
		mainScene.generateUniforms();
	}
}

void mouseButtonCb(int button, int action) {
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		mouseMiddleDown = action == GLFW_PRESS;
	}
}

void mouseOrbitCb(float xpos, float ypos, bool refocused) {
	static bool justPressed = true;

	if (mouseMiddleDown) {
		static float lastX = 0;
		static float lastY = 0;

		if (justPressed || refocused) {
			lastX = xpos;
			lastY = ypos;
		}

		float dx = xpos - lastX; // in px
		float dy = ypos - lastY;

		if (shiftHeld) {
			float w = 2 * orbitDist * tan(fov / 2);
			float panSens = w / static_cast<float>(windowWidth);

			glm::vec3 right = glm::normalize(glm::cross(uniforms.camUp, uniforms.camDir));
			glm::vec3 screenUp = glm::normalize(glm::cross(uniforms.camDir, right));

			pivot += screenUp * dy * panSens;
			pivot -= right * dx * panSens;
			uniforms.camPos = pivot - uniforms.camDir * orbitDist;
		} else {
			float yaw = Settings::mouseSens * dx;
			float pitch = Settings::mouseSens * dy;

			glm::vec3 right = glm::normalize(glm::cross(uniforms.camUp, uniforms.camDir));

			glm::vec3 newPos = glm::rotate(uniforms.camPos - pivot, pitch, right) + pivot;

			if (abs(glm::dot(glm::normalize(pivot - newPos), uniforms.camUp)) < 0.999) {
				uniforms.camPos = newPos; // if direction isnt too vertical
			}
			uniforms.camPos = glm::rotate(uniforms.camPos - pivot, yaw, uniforms.camUp) + pivot;
			uniforms.camDir = glm::normalize(pivot - uniforms.camPos);
		}

		lastX = xpos;
		lastY = ypos;
		justPressed = false;
	} else {
		justPressed = true;
	}
}

void myMouseCb(float xpos, float ypos, bool refocused) {
	static float relX = 0;
	static float relY = 0;
	static bool justPressed = true;

	if (mouseMiddleDown) {
		if (justPressed) {
			relX = xpos;
			relY = ypos;
		}

		mouseCallback(xpos - relX, ypos - relY, justPressed || refocused);

		justPressed = false;
	} else {
		justPressed = true;
	}
}

void scrollZoomCb(float scroll) {
	float co = 1.05;
	setZoom(zoom * pow(co, scroll));
}

void scrollOrbitCb(float scroll) {
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse) { // only zoom if not captured by imgui
		orbitDist *= pow(0.95, scroll);
		if (orbitDist < 1) orbitDist = 1;
		uniforms.camPos = pivot - uniforms.camDir * orbitDist;
	}
}

void keyCb(int key, int action, int mods) {
	if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
		shiftHeld = action == GLFW_PRESS;
	}

	guiKeyCb(key, action, mods);
}

int main() {
	setup(APP_NAME, APP_VERSION);

	// setup gui
	endCallback = cleanupGui;
	renderPassCallback = renderGui;
	setupGui();

	// mouse orbit
	uniforms.camPos = pivot - uniforms.camDir * orbitDist;
	mouseMovementCallback = mouseOrbitCb;
	mouseButtonCallback = mouseButtonCb;
	scrollCallback = scrollOrbitCb;
	keyCallback = keyCb;

	// load scene
	mainScene.importScene("scenes/perftest.json");
	mainScene.generateUniforms();

//	std::cout << uniforms.toString() << std::endl;

//	initFps();

	run(update);
}
