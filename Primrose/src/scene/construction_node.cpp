#include <iostream>
#include "scene/construction_node.hpp"
#include "scene/node_visitor.hpp"

using namespace Primrose;

bool ConstructionNode::createOperations(const std::vector<Primitive>& prims,
	const std::vector<Transformation>& transforms, std::vector<Operation>& ops) {

	if (shouldHide()) return false;

	uint lastChildIndex = -1;
	for (const auto& child : getChildren()) {
		if (child->createOperations(prims, transforms, ops)) {
			if (lastChildIndex != -1) {
				ops.push_back(foldOperations(lastChildIndex, ops.size() - 1));
			}
			lastChildIndex = ops.size() - 1;
		}
	}

	return lastChildIndex != -1;
}

UnionNode::UnionNode(Primrose::Node* parent) : ConstructionNode(parent) { name = "Union"; }
Operation UnionNode::foldOperations(uint i, uint j) {
	return Operation::Union(i, j);
}
void UnionNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
}
void UnionNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("union");
	Node::serialize(writer);
}

IntersectionNode::IntersectionNode(Primrose::Node* parent) : ConstructionNode(parent) { name = "Intersection"; }
Operation IntersectionNode::foldOperations(uint i, uint j) {
	return Operation::Intersection(i, j);
}
void IntersectionNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
}
void IntersectionNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("intersection");
	Node::serialize(writer);
}

DifferenceNode::DifferenceNode(Primrose::Node* parent) : Node(parent) { name = "Difference"; }
bool DifferenceNode::createOperations(const std::vector<Primitive>& prims,
	const std::vector<Transformation>& transforms, std::vector<Operation>& ops) {

	if (shouldHide()) return false;

	std::vector<Operation> opsCopy = ops;

	uint lastBaseIndex = -1;
	uint lastSubtractIndex = -1;
	for (const auto& child : getChildren()) {
		if (child->createOperations(prims, transforms, ops)) {
			if (!subtractNodes.contains(child.get())) {
				if (lastBaseIndex != -1) {
					ops.push_back(Operation::Union(lastBaseIndex, ops.size() - 1));
				}

				lastBaseIndex = ops.size() - 1;
			} else {
				if (lastSubtractIndex != -1) {
					ops.push_back(Operation::Union(lastSubtractIndex, ops.size() - 1));
				}

				lastSubtractIndex = ops.size() - 1;
			}
		}
	}

	if (lastBaseIndex == -1) {
		ops = opsCopy; // get rid of any subtract nodes that were added to operations
		return false;
	};
	if (lastSubtractIndex == -1) return true;

	ops.push_back(Operation::Difference(lastBaseIndex, lastSubtractIndex));
	return true;
}
void DifferenceNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
}
void DifferenceNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("difference");
	if (subtractNodes.size() > 0) {
		writer.String("subtractIndices");
		writer.StartArray();
		for (Node* node : subtractNodes) {
			if (node->getParent() != this) continue; // no garuntee subtractNodes is all node's children
			writer.Int(node->locationInParent() - getChildren().begin());
		}
		writer.EndArray();
	}
	Node::serialize(writer);
}
