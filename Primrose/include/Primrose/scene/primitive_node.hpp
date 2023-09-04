#ifndef PRIMROSE_PRIMITIVE_NODE_HPP
#define PRIMROSE_PRIMITIVE_NODE_HPP

#include "../shader_structs.hpp"
#include "node.hpp"
#include "construction_node.hpp"

#define PRIM_OVERRIDES \
public: \
std::string toString(std::string prefix = "") override; \
void accept(NodeVisitor* visitor) override; \
void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override; \
std::string generateIntersectionGlsl() override; \
AABB generateAabb() override; \
private: \
Primitive toPrimitive() override;

namespace Primrose {
	class PrimitiveNode : public UnionNode {
	public:
		using UnionNode::UnionNode;

		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override = 0;

		std::string generateIntersectionGlsl() override = 0;
		AABB generateAabb() override = 0;

		std::vector<Primitive> extractPrims() override;
		std::vector<Transformation> extractTransforms() override;
		bool createOperations(
			const std::vector<Primitive>& prims, const std::vector<Transformation>& transforms,
			std::vector<Operation>& ops) override;

	private:
		virtual Primitive toPrimitive() = 0;
	};

	class SphereNode : public PrimitiveNode { PRIM_OVERRIDES
	public:
		SphereNode(Node* parent, float radius);
		float radius;
	};

	class BoxNode : public PrimitiveNode { PRIM_OVERRIDES
	public:
		BoxNode(Node* parent, glm::vec3 size);
		glm::vec3 size;
	};

	class TorusNode : public PrimitiveNode { PRIM_OVERRIDES
	public:
		TorusNode(Node* parent, float ringRadius, float majorRadius);
		float ringRadius;
		float majorRadius;
	};

	class LineNode : public PrimitiveNode { PRIM_OVERRIDES
	public:
		LineNode(Node* parent, float height, float radius);
		float height;
		float radius;
	};

	class CylinderNode : public PrimitiveNode { PRIM_OVERRIDES
	public:
		CylinderNode(Node* parent, float radius);
		float radius;
	};
}

#endif
