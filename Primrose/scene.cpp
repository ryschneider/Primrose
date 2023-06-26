#include "scene.hpp"
#include "core.hpp"
#include "state.hpp"
#include "shader_structs.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>

bool Primrose::operationsValid() {
	if (uniforms.numOperations > Settings::MAX_NUM_OPERATIONS) return false;
	for (int i = 0; i < uniforms.numOperations; ++i) {
		Operation op = uniforms.operations[i];

		if (op.type == OP_IDENTITY || op.type == OP_TRANSFORM) {
			continue;
		} else if (op.type == OP_UNION || op.type == OP_INTERSECTION || op.type == OP_DIFFERENCE) {
			if (op.i >= i || op.j >= i) return false;
			Operation op1 = uniforms.operations[op.i];
			Operation op2 = uniforms.operations[op.j];
			if (op1.type == OP_TRANSFORM || op2.type == OP_TRANSFORM) return false;
		} else {
			return false;
		}
	}

	return true;
}

glm::vec3 getScale(glm::mat4 transform) {
	return glm::vec3(
		glm::length(transform[0]),
		glm::length(transform[1]),
		glm::length(transform[2]));
}

glm::mat4 Primrose::transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis) {
	return glm::translate(translate)
		* glm::scale(scale)
		* glm::rotate(rotAngle, glm::normalize(rotAxis));
}

Primitive Primrose::Prim(unsigned int type, glm::mat4 transform) {
	Primitive prim{};
	prim.type = type;
	prim.invTransform = glm::inverse(transform);
	glm::vec3 scale = getScale(transform);
	prim.smallScale = std::min(scale.x, std::min(scale.y, scale.z));

	return prim;
}
Primitive Primrose::Prim(unsigned int type, float a, glm::mat4 transform) {
	Primitive prim = Prim(type, transform);
	prim.a = a;

	return prim;
}
Primitive Primrose::Prim(unsigned int type, float a, float b, glm::mat4 transform) {
	Primitive prim = Prim(type, transform);
	prim.a = a;
	prim.b = b;

	return prim;
}


float getSmallScale(glm::mat4 matrix) {
	glm::vec3 scale = getScale(matrix);
	return std::min(scale.x, std::min(scale.y, scale.z));
}

void Primrose::addTransform(glm::mat4 matrix) {
	if (uniforms.numOperations > 0) {
		Operation lastOp = uniforms.operations[uniforms.numOperations-1];
		if (lastOp.type == OP_TRANSFORM) {
			// compound successive matrices
			Transformation& transform = uniforms.transformations[lastOp.i];
			transform.invMatrix = glm::inverse(matrix) * transform.invMatrix;
			transform.smallScale = getSmallScale(glm::inverse(transform.invMatrix));
			return;
		}
	}

	static unsigned int numTransformations = 0;
	uniforms.transformations[numTransformations].invMatrix = glm::inverse(matrix);
	uniforms.transformations[numTransformations].smallScale = getSmallScale(matrix);

	uniforms.operations[uniforms.numOperations++] = {OP_TRANSFORM, numTransformations++};
}
void Primrose::addTranslate(float x, float y, float z) {
	addTransform(transformMatrix(
		glm::vec3(x, y, z),
		glm::vec3(1.f),
		0.f, glm::vec3(1.f)));
}
void Primrose::addScale(float scalar) {
	addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(scalar),
		0.f, glm::vec3(1.f)));
}
void Primrose::addScale(float x, float y, float z) {
	addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(x, y, z),
		0.f, glm::vec3(1.f)));
}
void Primrose::addRotate(float angle, glm::vec3 axis) {
	addTransform(transformMatrix(
		glm::vec3(0.f),
		glm::vec3(1.f),
		angle, axis));
}

unsigned int Primrose::addIdentity(unsigned int i, bool render) {
	uniforms.operations[uniforms.numOperations] = {OP_IDENTITY, i, 0, render};
	return uniforms.numOperations++;
}
unsigned int Primrose::addUnion(unsigned int i, unsigned int j, bool render) {
	uniforms.operations[uniforms.numOperations] = {OP_UNION, i, j, render};
	return uniforms.numOperations++;
}
unsigned int Primrose::addIntersection(unsigned int i, unsigned int j, bool render) {
	uniforms.operations[uniforms.numOperations] = {OP_INTERSECTION, i, j, render};
	return uniforms.numOperations++;
}
unsigned int Primrose::addDifference(unsigned int i, unsigned int j, bool render) {
	uniforms.operations[uniforms.numOperations] = {OP_DIFFERENCE, i, j, render};
	return uniforms.numOperations++;
}
