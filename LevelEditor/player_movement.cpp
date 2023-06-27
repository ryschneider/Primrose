#include "player_movement.hpp"

#include <Primrose/core.hpp>
#include <glm/gtx/rotate_vector.hpp>

using namespace Primrose;

void updatePosition() {
	glm::vec3 up = uniforms.camUp;
	glm::vec3 forward = glm::normalize(uniforms.camDir - up * glm::dot(uniforms.camDir, up));
	glm::vec3 right = glm::cross(up, forward);

	glm::vec3 movement(0);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += forward;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= forward;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement += up;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement -= up;

	float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 30 : 10;

	movement *= speed * deltaTime;

	uniforms.camPos += movement;
}

//void updateCamera() {
//	uniforms.camDir = glm::vec3(
//		cos(yaw) * cos(pitch),
//		sin(pitch),
//		sin(yaw) * cos(pitch)
//	);
//}

void mouseCallback(float xpos, float ypos, bool refocused) {
	static float lastX = 0;
	static float lastY = 0;

	if (refocused) {
		// stops cam from jumping on refocus
		lastX = xpos;
		lastY = ypos;
	}

	float yaw = Settings::mouseSens * (xpos - lastX);
	float pitch = Settings::mouseSens * (ypos - lastY);

	glm::vec3 newDir = glm::rotate(uniforms.camDir, pitch,
		glm::normalize(glm::cross(uniforms.camUp, uniforms.camDir)));
	if (abs(glm::dot(newDir, uniforms.camUp)) < 0.9) {
		uniforms.camDir = newDir; // only pitch if not getting to close to exactly up/down
	}
	uniforms.camDir = glm::rotate(uniforms.camDir, yaw, uniforms.camUp);

//	yaw -= Settings::mouseSens * (xpos - lastX);
//	pitch -= Settings::mouseSens * (ypos - lastY);
//
//	yaw = fmod(yaw, glm::radians(360.f));
//	pitch = std::clamp(pitch, glm::radians(-89.99f), glm::radians(89.99f));

	lastX = xpos;
	lastY = ypos;
}
