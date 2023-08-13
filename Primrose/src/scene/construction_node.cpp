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
std::pair<glm::vec3, glm::vec3> UnionNode::generateAabb() {
	glm::vec3 lesser(0);
	glm::vec3 greater(0);
	for (const auto& child : getChildren()) {
		auto[l, g] = child->generateAabb();

		lesser = glm::vec3(std::min(lesser.x, l.x), std::min(lesser.y, l.y), std::min(lesser.z, l.z));
		greater = glm::vec3(std::max(greater.x, g.x), std::max(greater.y, g.y), std::max(greater.z, g.z));
	}
	return {lesser, greater};
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
std::pair<glm::vec3, glm::vec3> IntersectionNode::generateAabb() {
	glm::vec3 lesser(0);
	glm::vec3 greater(0);

	if (getChildren().size() > 0) {
		std::tie(lesser, greater) = getChildren()[0]->generateAabb();

		const auto& children = getChildren();
		for (auto it = children.begin()+1; it != children.end(); ++it) {
			const auto& child = *it;

			auto[l, g] = child->generateAabb();
			// intersect l,g and lesser,greater
			lesser = glm::vec3(std::max(lesser.x, l.x), std::max(lesser.y, l.y), std::max(lesser.z, l.z));
			greater = glm::vec3(std::min(greater.x, g.x), std::min(greater.y, g.y), std::min(greater.z, g.z));

			if (lesser.x > greater.x || lesser.y > greater.y || lesser.z > greater.z) {
				lesser = glm::vec3(0);
				greater = glm::vec3(0);
			}
		}
	}

	return {lesser, greater};
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
std::pair<glm::vec3, glm::vec3> DifferenceNode::generateAabb() {
	glm::vec3 baseLesser(0);
	glm::vec3 baseGreater(0);
	glm::vec3 subLesser(0);
	glm::vec3 subGreater(0);
	for (const auto& child : getChildren()) {
		auto[l, g] = child->generateAabb();

		// union with correct aabb
		if (subtractNodes.contains(child.get())) {
			subLesser = glm::vec3(std::min(baseLesser.x, l.x), std::min(baseLesser.y, l.y), std::min(baseLesser.z, l.z));
			subGreater = glm::vec3(std::max(baseGreater.x, g.x), std::max(baseGreater.y, g.y), std::max(baseGreater.z, g.z));
		} else {
			baseLesser = glm::vec3(std::min(baseLesser.x, l.x), std::min(baseLesser.y, l.y), std::min(baseLesser.z, l.z));
			baseGreater = glm::vec3(std::max(baseGreater.x, g.x), std::max(baseGreater.y, g.y), std::max(baseGreater.z, g.z));
		}
	}

	if (subLesser.y <= baseLesser.y && baseGreater.y <= subGreater.y // base yz plane inside sub yz plane
		&& subLesser.z <= baseLesser.z && baseGreater.z <= subGreater.z) {
		baseLesser.x = std::min(baseLesser.x, subGreater.x); // shrink base in x dimension
		baseGreater.x = std::min(baseGreater.x, subLesser.x);
	}
	if (subLesser.x <= baseLesser.x && baseGreater.x <= subGreater.x // base xz plane inside sub xz plane
		&& subLesser.z <= baseLesser.z && baseGreater.z <= subGreater.z) {
		baseLesser.y = std::min(baseLesser.y, subGreater.y); // shrink base in y dimension
		baseGreater.y = std::min(baseGreater.y, subLesser.y);
	}
	if (subLesser.x <= baseLesser.x && baseGreater.x <= subGreater.x // base xy plane inside sub xy plane
		&& subLesser.y <= baseLesser.y && baseGreater.y <= subGreater.y) {
		baseLesser.z = std::min(baseLesser.z, subGreater.z); // shrink base in z dimension
		baseGreater.z = std::min(baseGreater.z, subLesser.z);
	}

	return {baseLesser, baseGreater};
}
