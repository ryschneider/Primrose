#include <Primrose/core.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>

#include "../LevelEditor/player_movement.hpp"

const char* APP_NAME = "Betterays";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

typedef unsigned int uint;

float r = 80;
glm::vec3 planetPos(0, -r, 5);

#include <iostream>
void update() { // run once per frame
	static glm::vec3 velocity(0);
	bool onPlanet = glm::distance(uniforms.camPos, planetPos)-2.01 <= r;
	uniforms.camUp = onPlanet ? glm::normalize(uniforms.camPos - planetPos) : glm::vec3(0, 1, 0);

	updatePosition();

	//glm::mat4 transform = Scene::transformMatrix(
	//	glm::vec3(0.f, 0.f, 0.f), // translate
	//	glm::vec3(1.f), // scale
	//	Runtime::currentTime*0.3f, glm::vec3(1.f, 1.f, 1.f) // rotate
	//);
	//myPrim.invTransform = glm::inverse(transform);

//	{ // movement
//		glm::vec3 forward = glm::normalize(glm::vec3(playerDir.x, 0.f, playerDir.z));
//		glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
//		glm::vec3 right = glm::cross(up, forward);
//
//		glm::vec3 movement = glm::vec3(0);
//		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)  movement += forward;
//		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= forward;
//		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
//		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
//		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement += up;
//		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement -= up;
//
//		float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 30 : 10;
//
//		movement *= speed * deltaTime;
//
//		playerPos += movement;
//	}


//	{ // movement
//		glm::vec3 forward = onPlanet ? playerDir : glm::normalize(glm::vec3(playerDir.x, 0.f, playerDir.z));
//		glm::vec3 up = onPlanet ? glm::normalize(playerPos - planetPos) : glm::vec3(0, 1, 0);
//		glm::vec3 right = glm::cross(up, forward);
//
//		glm::vec3 movement = glm::vec3(0);
//		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)  movement += forward;
//		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= forward;
//		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
//		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
////		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement += up;
//		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement -= up;
//		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
//			movement += up;
//			if (onPlanet) playerPos += up * 0.02f;
//		}
//
//		float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 30 : 10;
//
//		movement *= speed * deltaTime;
//
//		playerPos += movement;
//		if (onPlanet && glfwGetKey(window, GLFW_KEY_SPACE) != GLFW_PRESS) playerPos -= up*25.f;
//	}



	{
		// planet
		glm::vec3 down = glm::normalize(planetPos - uniforms.camPos);
		float dist = glm::distance(uniforms.camPos, planetPos);

		float a = 200 / (dist*dist);
		velocity += down * a;

//		playerPos += velocity * deltaTime;

		if (dist-2 < r) {
			uniforms.camPos -= down * (r - (dist-2));
			float downComp = glm::dot(velocity, down);
//			if (downComp > 0) velocity -= down * downComp;
			velocity = glm::vec3(0);
		}

		if (onPlanet) uniforms.camUp = -down;
		else uniforms.camUp = glm::vec3(0, 1, 0);
	}
}

glm::mat4 fromTranslate(glm::vec3 translate) {
	return Primrose::transformMatrix(
		translate,
		glm::vec3(1.f),
		0.f, glm::vec3(1.f));
}
glm::mat4 fromScalar(float scalar) {
	return Primrose::transformMatrix(
		glm::vec3(0.f),
		glm::vec3(scalar),
		0.f, glm::vec3(1.f));
}
glm::mat4 fromScale(glm::vec3 scale) {
	return Primrose::transformMatrix(
		glm::vec3(0.f),
		scale,
		0.f, glm::vec3(1.f));
}
glm::mat4 fromRotation(float angle, glm::vec3 axis) {
	return Primrose::transformMatrix(
		glm::vec3(0.f),
		glm::vec3(1.f),
		angle, axis);
}

void planetScene() {
	uniforms.primitives[0] = Prim(PRIM_SPHERE);
	uniforms.primitives[4] = Prim(PRIM_4);

	addTranslate(planetPos.x, planetPos.y, planetPos.z);
	addScale(r);
	uint planet = addIdentity(0);

	addScale(1);
	uint rocks = addIdentity(4, false);

	addTranslate(planetPos.x, planetPos.y, planetPos.z);
	addScale(r + 1);
	uint rockLimit = addIdentity(0);

	uint rockSurface = addIntersection(rocks, rockLimit);
	addUnion(rockSurface, planet, true);



	uniforms.primitives[1] = Prim(PRIM_CYLINDER);
	addTranslate(0, 2, 5);
	addIdentity(1, true);

	uniforms.primitives[2] = Prim(PRIM_BOX);
	uniforms.primitives[3] = Prim(PRIM_CYLINDER);

	glm::mat4 csgModel = Primrose::transformMatrix(
		planetPos + glm::vec3(0, 0.7071, 0.7071) * (r+2),
		glm::vec3(1),
		3.1415/4, glm::vec3(1, 0, 0));

	addTransform(csgModel);
	unsigned int cube = addIdentity(2);
	addTransform(csgModel);
	addScale(0.5f);
	unsigned int yCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(0.5f);
	addRotate(3.1415f/2.f, {1.f, 0.f, 0.f});
	unsigned int zCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(0.5f);
	addRotate(3.1415f/2.f, {0.f, 0.f, 1.f});
	unsigned int xCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(1.2f);
	unsigned int sphere = addIdentity(0);

	uint xyCyl = addUnion(xCyl, yCyl);
	uint xyzCyl = addUnion(xyCyl, zCyl);
	uint sphCube = addIntersection(cube, sphere);
	addDifference(sphCube, xyzCyl, true);


	addTranslate(0,40, 5);
	addScale(10, 2, 10);
	addIdentity(2, true);

	uniforms.primitives[5] = Prim(PRIM_TORUS, 0.5);
	addTranslate(0,-2*r - 40, 5);
	addScale(10);
	addIdentity(5, true);
}

int main() {
	Primrose::setup(APP_NAME, APP_VERSION);
	Primrose::mouseMovementCallback = mouseCallback;

	planetScene();
	if (!Primrose::operationsValid()) {
		throw std::runtime_error("invalid operations list");
	}

	run(update);
}
