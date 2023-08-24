#include "scene/node.hpp"

#include <glm/geometric.hpp>
#include <iostream>

using namespace Primrose;

Node::Node(Node* parent) : parent(parent) {
	parent->children.emplace_back(this);
}

Node::~Node() {
	if (parent == nullptr) return; // if parent has already been destroyed don't bother
//	std::cout << "deleting " << name << std::endl;

	auto it = locationInParent();
	it->release();
	parent->children.erase(it);
}

RootNode::RootNode() { name = "Root"; }

void RootNode::clear() {
	children.clear();
}



bool Node::isDescendantOf(Primrose::Node* ancestor) {
	return ancestor == parent || parent->isDescendantOf(ancestor);
}

bool RootNode::isDescendantOf(Primrose::Node* ancestor) {
	return false;
}

std::vector<std::unique_ptr<Node>>::iterator Node::locationInParent() {
	for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
		if (it->get() == this) {
			return it;
		}
	}
	throw std::runtime_error("could not find node in parent's children");
}

void Node::reparent(Node* newParent) {
	if (this == newParent) return;
	std::cout << "reparenting " << name << " under " << newParent->name << std::endl;
	if (newParent->isDescendantOf(this)) {
		newParent->reparent(parent);
	}

	auto it = locationInParent();

	it->release();
	parent->children.erase(it);

	newParent->children.emplace_back(this);
	parent = newParent;
}

void Node::swap(Primrose::Node* node) {
	throw std::runtime_error("not done yet !!!!");

	std::cout << "swap" << std::endl;

	if (this->parent == node) {
		node->swap(this); // only deal with case "node" is child of "this"
	} else if (node->parent == this) {
		auto it = node->locationInParent();
		it->release();
		children.erase(it);

//		newChildren.emplace_back(node)
//		node->parent = newChildren
	}

	// reparent children
	std::vector<Node*> oldChildren;
	std::vector<Node*> newChildren;
	for (const auto& child : children) { oldChildren.push_back(child.get()); }
	for (const auto& child : node->children) { newChildren.push_back(child.get()); }
	for (Node* child : oldChildren) {
		if (child != node) { child->reparent(node); }
//		else { child->reparent(this); }
	}
	for (Node* child : newChildren) { child->reparent(this); } // "this" cannot be child of "node"

	// swap in parents' children vectors
	locationInParent()->swap(*node->locationInParent());



	// change parent
	Node* oldParent = parent;
	Node* newParent = node->parent;

	parent = newParent;
	if (node->parent == this) {
		children.emplace_back(node);
	}
	node->parent = oldParent;

	std::cout << node->name << " has new parent " << node->parent->name << std::endl;
}

void Node::duplicate() {
	throw std::runtime_error("dont work yet");
}

Node* Node::getParent() {
	return parent;
}

const std::vector<std::unique_ptr<Node>>& Node::getChildren() {
	return children;
}

std::string Node::toString(std::string prefix) {
	std::string out = "[";
	if (translate != glm::vec3(0)) {
		out += fmt::format("pos=({},{},{}))", translate.x, translate.y, translate.z);
	}
	if (scale != glm::vec3(1)) {
		out += fmt::format("scale=({},{},{}))", scale.x, scale.y, scale.z);
	}
	if (angle != 0) {
		out += fmt::format("rot=({},({},{},{})))", angle, axis.x, axis.y, axis.z);
	}
	out += "]";
	if (out == "[]") out = "";
	return out;
}

glm::mat4 Node::modelMatrix() {
	glm::mat4 matrix = parent->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale);
	return matrix;
}

glm::mat4 RootNode::modelMatrix() {
	return glm::mat4(1);
}

std::string RootNode::generateIntersectionGlsl() {
	return "";
}

std::pair<glm::vec3, glm::vec3> RootNode::generateAabb() {
	return {glm::vec3(0), glm::vec3(0)};
}

bool Node::shouldHide() {
	return hide || glm::determinant(modelMatrix()) == 0;
}

void RootNode::accept(Primrose::NodeVisitor* visitor) {}



void Node::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("name");
	writer.String(name.c_str());

	if (translate != glm::vec3(0) || scale != glm::vec3(1) || angle != 0) {
		writer.String("transform");
		writer.StartObject();
		if (translate != glm::vec3(0)) {
			writer.String("position");
			writer.StartArray();
			writer.Double(translate[0]);
			writer.Double(translate[1]);
			writer.Double(translate[2]);
			writer.EndArray();
		}
		if (scale != glm::vec3(1)) {
			writer.String("scale");
			writer.StartArray();
			writer.Double(scale[0]);
			writer.Double(scale[1]);
			writer.Double(scale[2]);
			writer.EndArray();
		}
		if (angle != 0) {
			writer.String("rotation");
			writer.StartObject();

			writer.String("angle");
			writer.Double(angle);
			writer.String("axis");
			axis = glm::normalize(axis);
			writer.StartArray();
			writer.Double(axis[0]);
			writer.Double(axis[1]);
			writer.Double(axis[2]);
			writer.EndArray();

			writer.EndObject();
		}
		writer.EndObject();
	}

	if (getChildren().size() > 0) {
		writer.String("children");
		writer.StartArray();
		for (const auto& node : getChildren()) {
			writer.StartObject();
			node->serialize(writer);
			writer.EndObject();
		}
		writer.EndArray();
	}
}



std::vector<Primitive> Node::extractPrims() {
	std::vector<Primitive> prims;
	for (const auto& child : getChildren()) {
		for (const auto& p : child->extractPrims()) {
			if (std::find(prims.begin(), prims.end(), p) == prims.end()) {
				prims.push_back(p);
			}
		}
	}
	return prims;
}

std::vector<Transformation> Node::extractTransforms() {
	std::vector<Transformation> transforms;

	for (const auto& child : getChildren()) {
		for (const auto& t : child->extractTransforms()) {
			if (std::find(transforms.begin(), transforms.end(), t) == transforms.end()) {
				transforms.push_back(t);
			}
		}
	}

	return transforms;
}

bool RootNode::createOperations(const std::vector<Primitive>& prims,
	const std::vector<Transformation>& transforms, std::vector<Operation>& ops) {

	bool shouldRender = false;

	for (const auto& child : getChildren()) {
		if (child->createOperations(prims, transforms, ops)) {
			ops.push_back(Operation::Render(ops.size() - 1));
			shouldRender = true;
		}
	}

	return shouldRender;
}
