#ifndef PRIMROSE_NODE_HPP
#define PRIMROSE_NODE_HPP

#include "../shader_structs.hpp"
#include "aabb.hpp"

#include <variant>
#include <vector>
#include <memory>
#include <set>
#include <glm/vec3.hpp>
#include <rapidjson/writer.h>
#include <rapidjson/ostreamwrapper.h>

namespace Primrose {
	class NodeVisitor;

	class Node {
	public:
		Node(Node* parent);
		~Node();

		virtual bool isDescendantOf(Node* ancestor);
		std::vector<std::unique_ptr<Node>>::iterator locationInParent();

		void reparent(Node* newParent);
		void swap(Node* node);
		void duplicate();

		Node* getParent();
		const std::vector<std::unique_ptr<Node>>& getChildren();

		virtual std::string toString(std::string prefix = "");
		virtual glm::mat4 modelMatrix();
		bool shouldHide();

		virtual std::string generateIntersectionGlsl() = 0;
		virtual AABB generateAabb() = 0;

		virtual void accept(NodeVisitor* visitor) = 0;

		virtual void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		virtual std::vector<Primitive> extractPrims();
		virtual std::vector<Transformation> extractTransforms();
		virtual bool createOperations(const std::vector<Primitive>& prims,
			const std::vector<Transformation>& transforms, std::vector<Operation>& ops) = 0;

		std::string name = "Node";
		bool hide = false;

		glm::vec3 translate = glm::vec3(0);
		glm::vec3 scale = glm::vec3(1);
		float angle = 0; // in degrees
		glm::vec3 axis = glm::vec3(0, 0, 1);

	private:
		Node() = default;

		Node* parent;
		std::vector<std::unique_ptr<Node>> children;

		friend class RootNode;
	};

	class RootNode : private Node {
	public:
		RootNode();
		using Node::getChildren;

		void clear();

		using Node::extractPrims;
		using Node::extractTransforms;
		bool createOperations(const std::vector<Primitive>& prims, const std::vector<Transformation>& transforms,
			std::vector<Operation>& ops) override;
		glm::mat4 modelMatrix() override;

		std::string generateIntersectionGlsl() override;
		AABB generateAabb() override;

		bool isDescendantOf(Primrose::Node *ancestor) override;

		void accept(Primrose::NodeVisitor *visitor) override;
	};
}

#endif
