#ifndef PRIMROSE_SHADER_STRUCTS_HPP
#define PRIMROSE_SHADER_STRUCTS_HPP

#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <fmt/format.h>
#include <glm/gtx/transform.hpp>
#include <map>
#include <vulkan/vulkan.hpp>

typedef unsigned int uint;

namespace Primrose {
	glm::vec3 getTranslate(glm::mat4 transform);
	glm::vec3 getScale(glm::mat4 transform);
	float getSmallScale(glm::mat4 matrix);
	glm::mat4 transformMatrix(glm::vec3 translate, glm::vec3 scale, float rotAngle, glm::vec3 rotAxis);

	enum class PRIM : uint {
		P1 = 101,
		P2 = 102,
		P3 = 103,
		P4 = 104,
		P5 = 105,
		P6 = 106,
		P7 = 107,
		P8 = 108,
		P9 = 109,
		SPHERE = 201,
		BOX = 202,
		TORUS = 203,
		LINE = 204,
		CYLINDER = 205,
	};

	enum class MAT : uint {
		M1 = 301,
		M2 = 302,
		M3 = 303,
	};

	enum class OP : uint {
		UNION = 901,
		INTERSECTION = 902,
		DIFFERENCE = 903,
		IDENTITY = 904,
		TRANSFORM = 905,
		RENDER = 906,
	};

	enum OP_FLAG : uint {
		NONE = 0b00,
	};

	enum class UI : uint {
		IMAGE = 501,
		PANEL = 502,
		TEXT = 503,
	};

	extern std::map<PRIM, std::string> PRIM_NAMES;
	extern std::map<MAT, std::string> MAT_NAMES;
	extern std::map<OP, std::string> OP_NAMES;
	extern std::map<OP_FLAG, std::string> OP_FLAG_NAMES;
	extern std::map<UI, std::string> UI_NAMES;

	struct Operation {
		alignas(4) OP type; // OP:: prefix
		alignas(4) uint i = 0; // index of first operand
		alignas(4) uint j = 0; // index of second operand
		alignas(4) OP_FLAG flags;

		static Operation Identity(uint i, uint j, OP_FLAG flags = OP_FLAG::NONE) { return {OP::IDENTITY, i, j, flags}; };
		static Operation Render(uint i, OP_FLAG flags = OP_FLAG::NONE) { return {OP::RENDER, i, 0, flags}; };
		static Operation Transform(uint i, OP_FLAG flags = OP_FLAG::NONE) { return {OP::TRANSFORM, i, 0, flags}; };
		static Operation Union(uint i, uint j, OP_FLAG flags = OP_FLAG::NONE) { return {OP::UNION, i, j, flags}; };
		static Operation Intersection(uint i, uint j, OP_FLAG flags = OP_FLAG::NONE)
			{ return {OP::INTERSECTION, i, j, flags}; };
		static Operation Difference(uint i, uint j, OP_FLAG flags = OP_FLAG::NONE)
			{ return {OP::DIFFERENCE, i, j, flags}; };
	};

	struct Primitive {
		alignas(4) PRIM type;
		alignas(4) float a; // first parameter
		alignas(4) float b; // second parameter
		alignas(4) uint mat; // material id

		static Primitive Sphere() { return {PRIM::SPHERE}; };
		static Primitive Box() { return {PRIM::BOX}; };
		static Primitive Torus(float ringRadius) { return {PRIM::TORUS, ringRadius}; };
		static Primitive Line(float halfHeight) { return {PRIM::LINE, halfHeight}; };
		static Primitive Cylinder() { return {PRIM::CYLINDER}; };

		bool operator==(const Primitive& p) const {
			if (type == PRIM::SPHERE || type == PRIM::BOX || type == PRIM::CYLINDER) {
				return type == p.type;
			} else if (type == PRIM::TORUS || type == PRIM::LINE) {
				return type == p.type && a == p.a;
			} else {
				throw std::runtime_error(fmt::format(
					"Primitive operator== not defined for type {}", static_cast<uint>(p.type)));
			}
		}
	};

	struct Transformation {
		alignas(16) glm::mat4 invMatrix;
		alignas(4) float smallScale;

		Transformation() = default;
		Transformation(glm::mat4 matrix) : invMatrix(glm::inverse(matrix)), smallScale(getSmallScale(matrix)) {}

		bool operator==(const Transformation& t) const {
			return invMatrix == t.invMatrix && smallScale == t.smallScale;
		}
	};

	struct ModelAttributes {
		glm::mat4 invMatrix;
		float invScale;
		glm::vec3 aabbMin;
		glm::vec3 aabbMax;
	};

	struct MarchUniforms {
		glm::vec3 camPos = glm::vec3(0);
		glm::vec3 camDir = glm::vec3(0, 0, 1);
		glm::vec3 camUp = glm::vec3(0, 1, 0);
		float screenHeight;

		float focalLength;
		float invZoom;

		ModelAttributes attributes[100];
		uint geometryAttributeOffset[100];

		uint numOperations;
		Operation operations[100];
		Primitive primitives[100];
		Transformation transformations[100];

		std::string toString();
	};

	struct PushConstants {
		float time;
	};

	struct UIVertex {
		glm::vec2 pos;
		glm::vec2 uv;

		static vk::VertexInputBindingDescription getBindingDescription() {
			vk::VertexInputBindingDescription bindingDesc{};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(UIVertex);
			bindingDesc.inputRate = vk::VertexInputRate::eVertex;

			return bindingDesc;
		}

		static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
			attributeDescriptions[0].offset = offsetof(UIVertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
			attributeDescriptions[1].offset = offsetof(UIVertex, uv);

			return attributeDescriptions;
		}
	};
}

#endif
