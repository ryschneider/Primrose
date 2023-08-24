#ifndef PRIMROSE_CONSTRUCTION_NODE_HPP
#define PRIMROSE_CONSTRUCTION_NODE_HPP

#include "node.hpp"

#include <set>

namespace Primrose {
	class ConstructionNode : public Node {
	protected:
		using Node::Node;

		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override = 0;

		virtual Operation foldOperations(uint i, uint j) = 0;
		bool createOperations(const std::vector<Primitive>& prims, const std::vector<Transformation>& transforms,
			std::vector<Operation>& ops) override;
	};

	class UnionNode : public ConstructionNode {
	public:
		UnionNode(Node* parent);
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::string generateIntersectionGlsl() override;
		std::pair<glm::vec3, glm::vec3> generateAabb() override;

	private:
		Operation foldOperations(uint i, uint j) override;
	};

	class IntersectionNode : public ConstructionNode {
	public:
		IntersectionNode(Node* parent);
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::string generateIntersectionGlsl() override;
		std::pair<glm::vec3, glm::vec3> generateAabb() override;

	private:
		Operation foldOperations(uint i, uint j) override;
	};

	class DifferenceNode : public Node {
	public:
		DifferenceNode(Node* parent);
		void accept(NodeVisitor* visitor) override;
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) override;

		std::string generateIntersectionGlsl() override;
		std::pair<glm::vec3, glm::vec3> generateAabb() override;

		std::set<Node*> subtractNodes;

	private:
		bool createOperations(const std::vector<Primitive>& prims, const std::vector<Transformation>& transforms,
			std::vector<Operation>& ops) override;
	};
}

#endif
