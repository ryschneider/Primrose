#include "scene.hpp"

using namespace Primrose;

std::string Primrose::Scene::toString() {
	std::string out = "";
	for (const auto& node : root) {
		out += node->toString("") + "\n";
	}
	return out;
}

//bool Primrose::operationsValid() {
//	for (int i = 0; i < uniforms.numOperations; ++i) {
//		Operation op = uniforms.operations[i];
//
//		if (op.type == OP::IDENTITY || op.type == OP::TRANSFORM) {
//			continue;
//		} else if (op.type == OP::UNION || op.type == OP::INTERSECTION || op.type == OP::DIFFERENCE) {
//			if (op.i >= i || op.j >= i) return false;
//			Operation op1 = uniforms.operations[op.i];
//			Operation op2 = uniforms.operations[op.j];
//			if (op1.type == OP::TRANSFORM || op2.type == OP::TRANSFORM) return false;
//		} else {
//			return false;
//		}
//	}
//
//	return true;
//}

std::string SceneNode::toString(std::string prefix) {
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

bool SceneNode::isPrim() {
	switch (type()) {
		case NODE_SPHERE:
		case NODE_BOX:
		case NODE_TORUS:
		case NODE_LINE:
		case NODE_CYLINDER:
			return true;
		case NODE_UNION:
		case NODE_INTERSECTION:
		case NODE_DIFFERENCE:
			return false;
	}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
}
#pragma GCC diagnostic pop

SphereNode::SphereNode(float radius) : radius(radius) { name = "Sphere"; }
NodeType SphereNode::type() { return NodeType::NODE_SPHERE; }
std::string SphereNode::toString(std::string prefix) {
	return fmt::format("{}sphere({}){}", prefix, radius, SceneNode::toString(""));
}

BoxNode::BoxNode(glm::vec3 size) : size(size) { name = "Box"; }
NodeType BoxNode::type() { return NodeType::NODE_BOX; }
std::string BoxNode::toString(std::string prefix) {
	return fmt::format("{}box(({}, {}, {})){}", prefix, size.x, size.y, size.z, SceneNode::toString(""));
}

TorusNode::TorusNode(float ringRadius, float majorRadius) : ringRadius(ringRadius), majorRadius(majorRadius) {
	name = "Torus";
}
NodeType TorusNode::type() { return NodeType::NODE_TORUS; }
std::string TorusNode::toString(std::string prefix) {
	return fmt::format("{}torus({}, {}){}", prefix, ringRadius, majorRadius, SceneNode::toString(""));
}

LineNode::LineNode(float height, float radius) : height(height), radius(radius) { name = "Line"; }
NodeType LineNode::type() { return NodeType::NODE_LINE; }
std::string LineNode::toString(std::string prefix) {
	return fmt::format("{}line({}, {}){}", prefix, height, radius, SceneNode::toString(""));
}

CylinderNode::CylinderNode(float radius) : radius(radius) { name = "Cylinder"; }
NodeType CylinderNode::type() { return NodeType::NODE_CYLINDER; }
std::string CylinderNode::toString(std::string prefix) {
	return fmt::format("{}cylinder({}){}", prefix, radius, SceneNode::toString(""));
}

UnionNode::UnionNode() { name = "Union"; }
NodeType UnionNode::type() { return NodeType::NODE_UNION; }
std::string UnionNode::toString(std::string prefix) {
	std::string childrenStr = "";
	for (const auto& node : children) {
		childrenStr += fmt::format("\n{}", node->toString(prefix + "  | "));
	}

	return fmt::format("{}union{}", prefix, childrenStr);
}

IntersectionNode::IntersectionNode() { name = "Intersection"; }
NodeType IntersectionNode::type() { return NodeType::NODE_INTERSECTION; }
std::string IntersectionNode::toString(std::string prefix) {
	std::string childrenStr = "";
	for (const auto& node : children) {
		childrenStr += fmt::format("\n{}", node->toString(prefix + "  & "));
	}

	return fmt::format("{}intersection{}", prefix, childrenStr);
}

DifferenceNode::DifferenceNode()  {	name = "Difference"; }
NodeType DifferenceNode::type() { return NodeType::NODE_DIFFERENCE; }
std::string DifferenceNode::toString(std::string prefix) {
	std::string out = fmt::format("{}difference", prefix);
	for (const auto& node : children) {
		if (subtractNodes.contains(node.get())) {
			out += fmt::format("\n{}", node->toString(prefix + "  - "));
		} else {
			out += fmt::format("\n{}", node->toString(prefix + "  + "));
		}
	}
	return out;
}
