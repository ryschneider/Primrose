#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include "shader_structs.hpp"

namespace Primrose {
	bool operationsValid();

	uint addPrim(uint type, float a = 0, float b = 0);

	uint addTransform(glm::mat4 matrix);
	uint addTranslate(float x, float y, float z);
	uint addScale(float scalar);
	uint addScale(float x, float y, float z);
	uint addRotate(float angle, glm::vec3 axis);

	uint addIdentity(uint i);
	uint addUnion(uint i, uint j);
	uint addIntersection(uint i, uint j);
	uint addDifference(uint i, uint j);
	uint addRender(uint i);

	glm::vec3 getScale(glm::mat4 matrix);
	float getSmallScale(glm::mat4 matrix);
	glm::mat4 transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis);
}

#endif
