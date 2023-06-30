#ifndef PRIMROSE_SHADER_STRUCTS_HPP
#define PRIMROSE_SHADER_STRUCTS_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

namespace Primrose {
	extern const unsigned int PRIM_1, PRIM_2, PRIM_3, PRIM_4, PRIM_5, PRIM_6, PRIM_7, PRIM_8, PRIM_9;
	extern const unsigned int PRIM_SPHERE, PRIM_BOX, PRIM_TORUS, PRIM_LINE, PRIM_CYLINDER;
	extern const unsigned int MAT_1, MAT_2, MAT_3;
	extern const unsigned int OP_UNION, OP_INTERSECTION, OP_DIFFERENCE, OP_IDENTITY, OP_TRANSFORM;
}

struct Primitive {
	alignas(16) glm::mat4 invTransform; // inverse of transformation matrix
	alignas(4) unsigned int type; // PRIM_ prefix
	alignas(4) float smallScale; // smallest scalar
	alignas(4) float a; // first parameter
	alignas(4) float b; // second parameter
	alignas(4) unsigned int mat; // material id
};

struct Operation {
	alignas(4) unsigned int type; // OP_ prefix
	alignas(4) unsigned int i; // index of first operand
	alignas(4) unsigned int j = 0; // index of second operand
	alignas(4) bool render = false;
};

struct Transformation {
	alignas(16) glm::mat4 invMatrix;
	alignas(4) float smallScale;
};

struct EngineUniforms {
	alignas(16) glm::vec3 camPos;
	alignas(16) glm::vec3 camDir = glm::vec3(0, 0, 1);
	alignas(16) glm::vec3 camUp = glm::vec3(0, 1, 0);
	alignas(4) float screenHeight;

	alignas(4) float focalLength;
	alignas(4) float invZoom;

	alignas(4) unsigned int numOperations;
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


#endif
