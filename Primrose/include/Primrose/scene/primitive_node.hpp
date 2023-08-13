#ifndef PRIMROSE_PRIMITIVE_NODE_HPP
#define PRIMROSE_PRIMITIVE_NODE_HPP

#include "../shader_structs.hpp"
#include "node.hpp"
#include "construction_node.hpp"

namespace Primrose {
	class PrimitiveNode : public UnionNode {
	public:
		using UnionNode::UnionNode;

		glm::mat4 modelMatrix() override = 0;

		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override = 0;

		std::pair<glm::vec3, glm::vec3> generateAabb() override = 0;

		std::vector<Primitive> extractPrims() override;
		std::vector<Transformation> extractTransforms() override;
		bool createOperations(
			const std::vector<Primitive>& prims, const std::vector<Transformation>& transforms,
			std::vector<Operation>& ops) override;

	private:
		virtual Primitive toPrimitive() = 0;
	};

	class SphereNode : public PrimitiveNode {
	public:
		SphereNode(Node* parent, float radius);

		std::string toString(std::string prefix = "") override;
		glm::mat4 modelMatrix() override;
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		float radius;

	private:
		Primitive toPrimitive() override;
	};

	class BoxNode : public PrimitiveNode {
	public:
		BoxNode(Node* parent, glm::vec3 size);

		std::string toString(std::string prefix = "") override;
		glm::mat4 modelMatrix() override;
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		glm::vec3 size;

	private:
		Primitive toPrimitive() override;
	};

	class TorusNode : public PrimitiveNode {
	public:
		TorusNode(Node* parent, float ringRadius, float majorRadius);

		std::string toString(std::string prefix = "") override;
		glm::mat4 modelMatrix() override;
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		float ringRadius;
		float majorRadius;

	private:
		Primitive toPrimitive() override;
	};

	class LineNode : public PrimitiveNode {
	public:
		LineNode(Node* parent, float height, float radius);

		std::string toString(std::string prefix = "") override;
		glm::mat4 modelMatrix() override;
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		float height;
		float radius;

	private:
		Primitive toPrimitive() override;
	};

	class CylinderNode : public PrimitiveNode {
	public:
		CylinderNode(Node* parent, float radius);

		std::string toString(std::string prefix = "") override;
		glm::mat4 modelMatrix() override;
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		float radius;

	private:
		Primitive toPrimitive() override;
	};
}

#endif
