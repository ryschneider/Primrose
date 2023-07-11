#ifndef PRIMROSE_SHADER_STRUCTS_HPP
#define PRIMROSE_SHADER_STRUCTS_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

typedef unsigned int uint;

namespace Primrose {

	enum {
		PRIM_1 = 101,
		PRIM_2 = 102,
		PRIM_3 = 103,
		PRIM_4 = 104,
		PRIM_5 = 105,
		PRIM_6 = 106,
		PRIM_7 = 107,
		PRIM_8 = 108,
		PRIM_9 = 109,
		PRIM_SPHERE = 201,
		PRIM_BOX = 202,
		PRIM_TORUS = 203,
		PRIM_LINE = 204,
		PRIM_CYLINDER = 205,
	};


//	extern const uint PRIM_1, PRIM_2, PRIM_3, PRIM_4, PRIM_5, PRIM_6, PRIM_7, PRIM_8, PRIM_9;
//	extern const uint PRIM_SPHERE, PRIM_BOX, PRIM_TORUS, PRIM_LINE, PRIM_CYLINDER;
	extern const uint MAT_1, MAT_2, MAT_3;
	extern const uint OP_UNION, OP_INTERSECTION, OP_DIFFERENCE, OP_IDENTITY, OP_TRANSFORM;

	extern const uint UI_NULL, UI_IMAGE, UI_PANEL, UI_TEXT;
}

struct Primitive {
	alignas(16) glm::mat4 invTransform; // inverse of transformation matrix
	alignas(4) uint type; // PRIM_ prefix
	alignas(4) float smallScale; // smallest scalar
	alignas(4) float a; // first parameter
	alignas(4) float b; // second parameter
	alignas(4) uint mat; // material id
};

struct Operation {
	alignas(4) uint type; // OP_ prefix
	alignas(4) uint i; // index of first operand
	alignas(4) uint j = 0; // index of second operand
	alignas(4) bool render = false;
};

struct Transformation {
	alignas(16) glm::mat4 invMatrix;
	alignas(4) float smallScale;
};

struct MarchUniforms {
	alignas(16) glm::vec3 camPos;
	alignas(16) glm::vec3 camDir = glm::vec3(0, 0, 1);
	alignas(16) glm::vec3 camUp = glm::vec3(0, 1, 0);
	alignas(4) float screenHeight;

	alignas(4) float focalLength;
	alignas(4) float invZoom;

	alignas(4) uint numOperations;
	alignas(16) Operation operations[100];
	alignas(16) Primitive primitives[100];
	alignas(16) Transformation transformations[100];
};

struct PushConstants {
	float time;
};

struct UIVertex {
	glm::vec2 pos;
	glm::vec2 uv;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDesc{};
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(UIVertex);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDesc;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(UIVertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(UIVertex, uv);

		return attributeDescriptions;
	}
};

struct UIUniforms {
	alignas(4) uint type;
};

#endif
