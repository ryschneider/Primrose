#include <core.hpp>
#include <glm/glm.hpp>
#include <stdexcept>

const char* APP_NAME = "Betterays";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

typedef unsigned int uint;

void update() { // run once per frame
	//glm::mat4 transform = Scene::transformMatrix(
	//	glm::vec3(0.f, 0.f, 0.f), // translate
	//	glm::vec3(1.f), // scale
	//	Runtime::currentTime*0.3f, glm::vec3(1.f, 1.f, 1.f) // rotate
	//);
	//myPrim.invTransform = glm::inverse(transform);

	{ // movement
		glm::vec3 forward = glm::normalize(glm::vec3(playerDir.x, 0.f, playerDir.z));
		glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 right = glm::cross(up, forward);

		glm::vec3 movement = glm::vec3(0);
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)  movement += forward;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= forward;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement += up;
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement -= up;

		float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 30 : 10;

		movement *= speed * deltaTime;

		playerPos += movement;
	}

	{ // camera
		playerDir = glm::vec3(
			cos(yaw) * cos(pitch),
			sin(pitch),
			sin(yaw) * cos(pitch)
		);
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

int main() {
	Primrose::setup(APP_NAME, APP_VERSION);

	uniforms.primitives[0] = Prim(PRIM_BOX);
	uniforms.primitives[1] = Prim(PRIM_SPHERE);
	uniforms.primitives[2] = Prim(PRIM_CYLINDER);

	addScalar(10);
	unsigned int cube = addIdentity(0);
	addScalar(0.5f);
	addScalar(10);
	unsigned int yCyl = addIdentity(2);
	addScalar(0.5f);
	addRotate(3.1415f/2.f, {1.f, 0.f, 0.f});
	addScalar(10);
	unsigned int zCyl = addIdentity(2);
	addScalar(0.5f);
	addRotate(3.1415f/2.f, {0.f, 0.f, 1.f});
	addScalar(10);
	unsigned int xCyl = addIdentity(2);
	addScalar(1.2f);
	addScalar(10);
	unsigned int sphere = addIdentity(1);

	uint xyCyl = addUnion(xCyl, yCyl);
	uint xyzCyl = addUnion(xyCyl, zCyl);
	uint sphCube = addIntersection(cube, sphere);
	addDifference(sphCube, xyzCyl, true);

	if (!Primrose::operationsValid()) {
		throw std::runtime_error("invalid operations list");
	}

//	uniforms.primitives[0] = Prim(PRIM_1, 5.f, transformMatrix(
//		glm::vec3(50.f, 0.f, 0.f),
//		glm::vec3(1.f),
//		-3.f, glm::vec3(1.f, 0.5f, -3.f)
//	));

//	uniforms.primitives[uniforms.numPrimitives++] = Prim(PRIM_BOX, transformMatrix(
//		glm::vec3(10.f, -10.f, 0.f),
//		glm::vec3(1.f, 100.f, 100.f),
////		0.f, glm::vec3(1.f)
//		5.f, glm::vec3(1.f)
//	));

//	uniforms.primitives[uniforms.numPrimitives++] = Prim(PRIM_TORUS, 0.5f);
//	uniforms.primitives[uniforms.numPrimitives++] = Prim(PRIM_2);
	
	//uniforms.primitives[1] = Prim(PRIM_LINE, 5.f, 0.1f, transformMatrix(
	//	glm::vec3(0.f), // translate
	//	glm::vec3(1.f), // scale
	//	glm::radians(45.f), glm::cross(glm::vec3(0.f, 1.f, 0.f), glm::normalize(glm::vec3(1.f, 1.f, 1.f))) // rotate
	//));

	//uniforms.primitives[uniforms.numPrimitives++] = Prim(PRIM_2, 0.02f); // axes

	run(update);
}
