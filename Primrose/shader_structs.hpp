#ifndef PRIMROSE_SHADER_STRUCTS_HPP
#define PRIMROSE_SHADER_STRUCTS_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <cstring>

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

	enum {
		MAT_1 = 301,
		MAT_2 = 302,
		MAT_3 = 303,
	};

	enum {
		OP_UNION = 901,
		OP_INTERSECTION = 902,
		OP_DIFFERENCE = 903,
		OP_IDENTITY = 904,
		OP_TRANSFORM = 905,
		OP_RENDER = 906,
	};

	enum {
		UI_NULL = 0,
		UI_IMAGE = 1,
		UI_PANEL = 2,
		UI_TEXT = 3,
	};
}

struct Operation {
	Operation() { memset(this, 0, sizeof(*this)); } // make sure padding is initialized to 0

	alignas(4) uint type; // OP_ prefix
	alignas(4) uint i = 0; // index of first operand
	alignas(4) uint j = 0; // index of second operand
	alignas(4) char _; // padding
};

struct Primitive {
	Primitive() { memset(this, 0, sizeof(*this)); } // make sure padding is initialized to 0

	alignas(4) uint type; // PRIM_ prefix
	alignas(4) float a; // first parameter
	alignas(4) float b; // second parameter
	alignas(4) uint mat; // material id
};

struct Transformation {
	Transformation() { memset(this, 0, sizeof(*this)); } // make sure padding is initialized to 0

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
};

struct MarchUniformsFull { // useful for knowing uniforms size/offsets
	MarchUniforms base; // offset from base uniforms

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
