#include "scene.hpp"
#include "core.hpp"
#include "state.hpp"
#include "shader_structs.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>
#include <iostream>

bool Primrose::operationsValid() {
	for (int i = 0; i < operations.size(); ++i) {
		Operation op = operations[i];

		if (op.type == OP_IDENTITY || op.type == OP_TRANSFORM) {
			continue;
		} else if (op.type == OP_UNION || op.type == OP_INTERSECTION || op.type == OP_DIFFERENCE) {
			if (op.i >= i || op.j >= i) return false;
			Operation op1 = operations[op.i];
			Operation op2 = operations[op.j];
			if (op1.type == OP_TRANSFORM || op2.type == OP_TRANSFORM) return false;
		} else {
			return false;
		}
	}

	return true;
}

uint Primrose::addPrim(uint type, float a, float b) {
	primitives.push_back({});
	Primitive& prim = primitives.back();

	prim.type = type;
	prim.a = a;
	prim.b = b;

	return primitives.size() - 1;
}

uint Primrose::addTransform(glm::mat4 matrix) {
	if (operations.size() > 0) {
		const Operation& lastOp = operations.back();
		if (lastOp.type == OP_TRANSFORM) {
			// compound successive matrices
			Transformation& transform = transformations[lastOp.i];
			transform.invMatrix = glm::inverse(matrix) * transform.invMatrix;
			transform.smallScale = getSmallScale(glm::inverse(transform.invMatrix));
			return operations.size() - 1;
		}
	}

	transformations.push_back({});
	transformations.back().invMatrix = glm::inverse(matrix);
	transformations.back().smallScale = getSmallScale(matrix);

	operations.push_back({});
	operations.back().type = OP_TRANSFORM;
	operations.back().i = transformations.size() - 1;

	return transformations.size() - 1;
}
uint Primrose::addTranslate(float x, float y, float z) {
	return addTransform(transformMatrix(
		glm::vec3(x, y, z),
		glm::vec3(1.f),
		0.f, glm::vec3(1.f)));
}
uint Primrose::addScale(float scalar) {
	return addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(scalar),
		0.f, glm::vec3(1.f)));
}
uint Primrose::addScale(float x, float y, float z) {
	return addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(x, y, z),
		0.f, glm::vec3(1.f)));
}
uint Primrose::addRotate(float angle, glm::vec3 axis) {
	return addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(1.f),
		angle, axis));
}

uint Primrose::addIdentity(uint i) {
	operations.push_back({});
	operations.back().type = OP_IDENTITY;
	operations.back().i = i;
	return operations.size() - 1;
}
uint Primrose::addUnion(uint i, uint j) {
	operations.push_back({});
	operations.back().type = OP_UNION;
	operations.back().i = i;
	operations.back().j = j;
	return operations.size() - 1;
}
uint Primrose::addIntersection(uint i, uint j) {
	operations.push_back({});
	operations.back().type = OP_INTERSECTION;
	operations.back().i = i;
	operations.back().j = j;
	return operations.size() - 1;
}
uint Primrose::addDifference(uint i, uint j) {
	operations.push_back({});
	operations.back().type = OP_DIFFERENCE;
	operations.back().i = i;
	operations.back().j = j;
	return operations.size() - 1;
}
uint Primrose::addRender(uint i) {
	operations.push_back({});
	operations.back().type = OP_RENDER;
	operations.back().i = i;
	return operations.size() - 1;
}

glm::vec3 Primrose::getScale(glm::mat4 transform) {
	return glm::vec3(
		glm::length(transform[0]),
		glm::length(transform[1]),
		glm::length(transform[2]));
}

float Primrose::getSmallScale(glm::mat4 matrix) {
	glm::vec3 scale = getScale(matrix);
	return std::min(scale.x, std::min(scale.y, scale.z));
}

glm::mat4 Primrose::transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis) {
	return glm::translate(translate)
		   * glm::scale(scale)
		   * glm::rotate(rotAngle, glm::normalize(rotAxis));
}
