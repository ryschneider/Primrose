#include <iostream>
#include "scene.hpp"

using namespace Primrose;

static void writeVec3(rapidjson::Writer<rapidjson::OStreamWrapper>& writer, glm::vec3 vec) {
	writer.StartArray();
	writer.Double(vec[0]);
	writer.Double(vec[1]);
	writer.Double(vec[2]);
	writer.EndArray();
}

std::string Primrose::Scene::toString() {
	std::string out = "";
	for (const auto& node : root) {
		out += node->toString("") + "\n";
	}
	return out;
}

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
void SceneNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("name");
	writer.String(name.c_str());

	if (translate != glm::vec3(0) || scale != glm::vec3(1) || angle != 0) {
		writer.String("transform");
		writer.StartObject();
		if (translate != glm::vec3(0)) {
			writer.String("position");
			writeVec3(writer, translate);
		}
		if (scale != glm::vec3(1)) {
			writer.String("scale");
			writeVec3(writer, scale);
		}
		if (angle != 0) {
			writer.String("rotation");
			writer.StartObject();

			writer.String("angle");
			writer.Double(angle);
			writer.String("axis");
			writeVec3(writer, glm::normalize(axis));

			writer.EndObject();
		}
		writer.EndObject();
	}

	if (children.size() > 0) {
		writer.String("children");
		writer.StartArray();
		for (const auto& node : children) {
			writer.StartObject();
			node->serialize(writer);
			writer.EndObject();
		}
		writer.EndArray();
	}
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

static void copyBase(SceneNode* dst, SceneNode* src) {
	for (const auto& child : src->children) {
		dst->children.emplace_back(child->copy());
		dst->children.back()->parent = dst;
	}

	dst->name = src->name;
	dst->translate = src->translate;
	dst->scale = src->scale;
	dst->angle = src->angle;
	dst->axis = src->axis;
	dst->hide = src->hide;
}

SphereNode::SphereNode(float radius) : radius(radius) { name = "Sphere"; }
NodeType SphereNode::type() { return NodeType::NODE_SPHERE; }
SceneNode* SphereNode::copy() {
	SceneNode* node = new SphereNode(radius);
	copyBase(node, this);
	return node;
}
std::string SphereNode::toString(std::string prefix) {
	return fmt::format("{}sphere({}){}", prefix, radius, SceneNode::toString(""));
}
void SphereNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("sphere");
	writer.String("radius");
	writer.Double(radius);

	SceneNode::serialize(writer);
}

BoxNode::BoxNode(glm::vec3 size) : size(size) { name = "Box"; }
NodeType BoxNode::type() { return NodeType::NODE_BOX; }
SceneNode* BoxNode::copy() {
	SceneNode* node = new BoxNode(size);
	copyBase(node, this);
	return node;
}
std::string BoxNode::toString(std::string prefix) {
	return fmt::format("{}box(({}, {}, {})){}", prefix, size.x, size.y, size.z, SceneNode::toString(""));
}
void BoxNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("box");
	writer.String("size");
	writeVec3(writer, size);

	SceneNode::serialize(writer);
}

TorusNode::TorusNode(float ringRadius, float majorRadius) : ringRadius(ringRadius), majorRadius(majorRadius) {
	name = "Torus";
}
NodeType TorusNode::type() { return NodeType::NODE_TORUS; }
SceneNode* TorusNode::copy() {
	SceneNode* node = new TorusNode(ringRadius, majorRadius);
	copyBase(node, this);
	return node;
}
std::string TorusNode::toString(std::string prefix) {
	return fmt::format("{}torus({}, {}){}", prefix, ringRadius, majorRadius, SceneNode::toString(""));
}
void TorusNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("sphere");
	writer.String("ringRadius");
	writer.Double(ringRadius);
	writer.String("majorRadius");
	writer.Double(majorRadius);

	SceneNode::serialize(writer);
}

LineNode::LineNode(float height, float radius) : height(height), radius(radius) { name = "Line"; }
NodeType LineNode::type() { return NodeType::NODE_LINE; }
SceneNode* LineNode::copy() {
	SceneNode* node = new LineNode(height, radius);
	copyBase(node, this);
	return node;
}
std::string LineNode::toString(std::string prefix) {
	return fmt::format("{}line({}, {}){}", prefix, height, radius, SceneNode::toString(""));
}
void LineNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("sphere");
	writer.String("height");
	writer.Double(height);
	writer.String("radius");
	writer.Double(radius);

	SceneNode::serialize(writer);
}

CylinderNode::CylinderNode(float radius) : radius(radius) { name = "Cylinder"; }
NodeType CylinderNode::type() { return NodeType::NODE_CYLINDER; }
SceneNode* CylinderNode::copy() {
	SceneNode* node = new CylinderNode(radius);
	copyBase(node, this);
	return node;
}
std::string CylinderNode::toString(std::string prefix) {
	return fmt::format("{}cylinder({}){}", prefix, radius, SceneNode::toString(""));
}
void CylinderNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("cylinder");
	writer.String("radius");
	writer.Double(radius);

	SceneNode::serialize(writer);
}

UnionNode::UnionNode() { name = "Union"; }
NodeType UnionNode::type() { return NodeType::NODE_UNION; }
SceneNode* UnionNode::copy() {
	SceneNode* node = new UnionNode();
	copyBase(node, this);
	return node;
}
std::string UnionNode::toString(std::string prefix) {
	std::string childrenStr = "";
	for (const auto& node : children) {
		childrenStr += fmt::format("\n{}", node->toString(prefix + "  | "));
	}

	return fmt::format("{}union{}", prefix, childrenStr);
}
void UnionNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("union");

	SceneNode::serialize(writer);
}

IntersectionNode::IntersectionNode() { name = "Intersection"; }
NodeType IntersectionNode::type() { return NodeType::NODE_INTERSECTION; }
SceneNode* IntersectionNode::copy() {
	SceneNode* node = new IntersectionNode();
	copyBase(node, this);
	return node;
}
std::string IntersectionNode::toString(std::string prefix) {
	std::string childrenStr = "";
	for (const auto& node : children) {
		childrenStr += fmt::format("\n{}", node->toString(prefix + "  & "));
	}

	return fmt::format("{}intersection{}", prefix, childrenStr);
}
void IntersectionNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("intersection");

	SceneNode::serialize(writer);
}

DifferenceNode::DifferenceNode()  {	name = "Difference"; }
NodeType DifferenceNode::type() { return NodeType::NODE_DIFFERENCE; }
SceneNode* DifferenceNode::copy() {
	SceneNode* node = new DifferenceNode();
	copyBase(node, this);
	return node;
}
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
void DifferenceNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer) {
	writer.String("type");
	writer.String("difference");

	SceneNode::serialize(writer);

	if (subtractNodes.size() > 0) {
		writer.String("subtractIndices");
		writer.StartArray();
		for (const SceneNode* node : subtractNodes) {
			for (int i = 0; i < children.size(); ++i) {
				if (children[i].get() == node) {
					writer.Int(i);
					break;
				}
			}
		}
		writer.EndArray();
	}
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
