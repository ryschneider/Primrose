#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include "shader_structs.hpp"

namespace Primrose {
	bool operationsValid();

	Primitive Prim(unsigned int type, glm::mat4 transform = glm::mat4(1.f));
	Primitive Prim(unsigned int type, float a, glm::mat4 transform = glm::mat4(1.f));
	Primitive Prim(unsigned int type, float a, float b, glm::mat4 transform = glm::mat4(1.f));

	void addTransform(glm::mat4 matrix);
	void addTranslate(float x, float y, float z);
	void addScalar(float scalar);
	void addScale(float x, float y, float z);
	void addRotate(float angle, glm::vec3 axis);

	unsigned int addIdentity(unsigned int i, bool render = false);
	unsigned int addUnion(unsigned int i, unsigned int j, bool render = false);
	unsigned int addIntersection(unsigned int i, unsigned int j, bool render = false);
	unsigned int addDifference(unsigned int i, unsigned int j, bool render = false);

	glm::mat4 transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis);
}

#endif
