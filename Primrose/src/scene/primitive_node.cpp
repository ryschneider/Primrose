#include <iostream>
#include <glm/gtx/string_cast.hpp>
#include "scene/primitive_node.hpp"
#include "scene/node_visitor.hpp"

using namespace Primrose;

std::vector<Primitive> PrimitiveNode::extractPrims() {
	std::vector<Primitive> prims = Node::extractPrims();

	Primitive p = toPrimitive();
	if (std::find(prims.begin(), prims.end(), p) == prims.end()) {
		prims.push_back(p);
	}

	return prims;
}

std::vector<Transformation> PrimitiveNode::extractTransforms() {
	std::vector<Transformation> transforms = Node::extractTransforms();

	if (shouldHide()) return transforms;

	glm::mat4 matrix = modelMatrix();
	if (std::find(transforms.begin(), transforms.end(), matrix) == transforms.end()) {
		transforms.push_back(Transformation(matrix));
	}

	return transforms;
}

bool PrimitiveNode::createOperations(const std::vector<Primitive>& prims,
	const std::vector<Transformation>& transforms, std::vector<Operation>& ops) {

	if (shouldHide()) return false;

	uint unionWith = -1;
	if (UnionNode::createOperations(prims, transforms, ops)) unionWith = ops.size() - 1;

	auto primIt = std::find(prims.begin(), prims.end(), toPrimitive());
	auto transformIt = std::find(transforms.begin(), transforms.end(), Transformation(modelMatrix()));

	if (primIt == prims.end() || transformIt == transforms.end()) {
		throw std::runtime_error("could not find prim/transform in supplied vectors");
	}

	// TODO :: combine OP_IDENTITY and OP_TRANSFORM
	ops.push_back(Operation::Transform(transformIt - transforms.begin()));
	ops.push_back(Operation::Identity(primIt - prims.begin(), static_cast<uint>(floor(scale.x)) % 3));
//	ops.push_back(Operation::Identity(primIt - prims.begin(), 0));

	if (unionWith != -1) {
		ops.push_back(Operation::Union(unionWith, ops.size() - 1));
	}

	return true;
}

namespace {
	static std::string glmToGlsl(glm::mat4 matrix) {
		return fmt::format("mat4({},{},{},{}, {},{},{},{}, {},{},{},{}, {},{},{},{})",
			matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
			matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
			matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
			matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]);
	}

	static glm::vec3 applyEach(float(*func)(float, float), glm::vec3 a, glm::vec3 b) {
		return glm::vec3(func(a.x, b.x), func(a.y, b.y), func(a.z, b.z));
	}

	template<typename... Args>
	static glm::vec3 applyEach(float(*func)(float, float), glm::vec3 a, glm::vec3 b, glm::vec3 c, Args... args) {
		return applyEach(func, applyEach(func, a, b), c, args...);
	}

	template<typename... Args>
	static glm::vec3 maxEach(glm::vec3 a, glm::vec3 b, Args... args) {
		return applyEach([](float x, float y) { return std::max(x, y); }, a, b, args...);
	}

	template<typename... Args>
	static glm::vec3 minEach(glm::vec3 a, glm::vec3 b, Args... args) {
		return applyEach([](float x, float y) { return std::min(x, y); }, a, b, args...);
	}

	static std::pair<glm::vec3, glm::vec3> extendAabb(glm::vec3 lesser, glm::vec3 greater, glm::mat4 matrix) {
		lesser = matrix * glm::vec4(lesser, 1);
		greater = matrix * glm::vec4(greater, 1);

		return { // return aabb containing all the transformed points
			minEach(lesser, greater),
			maxEach(lesser, greater)
		};
	}
}

SphereNode::SphereNode(Primrose::Node *parent, float radius) : PrimitiveNode(parent) {
	name = "Sphere";
	this->radius = radius;
}
void SphereNode::accept(NodeVisitor *visitor) { visitor->visit(this); }
std::string SphereNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere(" "radius" "={}" "){}", prefix, name, radius, Node::toString());
}
void SphereNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) {
	writer.String("type");
	writer.String("sphere");
	writer.String("radius");
	writer.Double(radius);
	Node::serialize(writer);
}
Primitive SphereNode::toPrimitive() {
	return Primitive::Sphere();
}
std::string SphereNode::generateIntersectionGlsl() {
	return fmt::format("sphereSDF({} * p, {})", glmToGlsl(glm::inverse(modelMatrix())), radius);
}
AABB SphereNode::generateAabb() {
	AABB aabb = AABB::fromPoints({-glm::vec3(radius), glm::vec3(radius)});
	aabb.applyTransform(modelMatrix());
	return aabb;
}

BoxNode::BoxNode(Primrose::Node *parent, glm::vec3 size) : PrimitiveNode(parent) {
	name = "Box";
	this->size = size;
}
void BoxNode::accept(NodeVisitor *visitor) { visitor->visit(this); }
std::string BoxNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere(" "size" "={}" "){}", prefix, name,
		fmt::format("({}, {}, {})", size.x, size.y, size.z), Node::toString());
}
void BoxNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) {
	writer.String("type");
	writer.String("box");
	writer.String("size");
	writer.StartArray();
	writer.Double(size[0]);
	writer.Double(size[1]);
	writer.Double(size[2]);
	writer.EndArray();
	Node::serialize(writer);
}
Primitive BoxNode::toPrimitive() {
	return Primitive::Box();
}
std::string BoxNode::generateIntersectionGlsl() {
	return fmt::format("boxSDF({} * p, vec3({}, {}, {}))", glmToGlsl(glm::inverse(modelMatrix())),
		size.x, size.y, size.z);
}
AABB BoxNode::generateAabb() {
	AABB aabb = AABB::fromPoints({-0.5f * size, 0.5f * size});
	aabb.applyTransform(modelMatrix());
	return aabb;
}


TorusNode::TorusNode(Primrose::Node *parent, float ringRadius, float majorRadius) : PrimitiveNode(parent) {
	name = "Torus";
	this->ringRadius = ringRadius;
	this->majorRadius = majorRadius;
}
void TorusNode::accept(NodeVisitor *visitor) { visitor->visit(this); }
std::string TorusNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere(" "ringRadius" "={}" "majorRadius" "={}" "){}", prefix, name, ringRadius,
		majorRadius, Node::toString());
}
void TorusNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) {
	writer.String("type");
	writer.String("torus");
	writer.String("ringRadius");
	writer.Double(ringRadius);
	writer.String("majorRadius");
	writer.Double(majorRadius);
	Node::serialize(writer);
}
Primitive TorusNode::toPrimitive() {
	return Primitive::Torus(ringRadius / majorRadius);
}
std::string TorusNode::generateIntersectionGlsl() {
	return fmt::format("torusSDF({} * p, {}, {})", glmToGlsl(glm::inverse(modelMatrix())), majorRadius, ringRadius);
}
AABB TorusNode::generateAabb() {
	AABB aabb = AABB::fromPoints({-glm::vec3(majorRadius + ringRadius), glm::vec3(majorRadius + ringRadius)});
	aabb.applyTransform(modelMatrix());
	return aabb;
}


LineNode::LineNode(Primrose::Node *parent, float height, float radius) : PrimitiveNode(parent) {
	name = "Line";
	this->height = height;
	this->radius = radius;
}
void LineNode::accept(NodeVisitor *visitor) { visitor->visit(this); }
std::string LineNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere(" "height" "={}" "radius" "={}" "){}", prefix, name, height, radius,
		Node::toString());
}
void LineNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) {
	writer.String("type");
	writer.String("line");
	writer.String("height");
	writer.Double(height);
	writer.String("radius");
	writer.Double(radius);
	Node::serialize(writer);
}
Primitive LineNode::toPrimitive() {
	return Primitive::Line(height*0.5 / radius);
}
std::string LineNode::generateIntersectionGlsl() {
	return fmt::format("lineSDF({} * p, {}, {})", glmToGlsl(glm::inverse(modelMatrix())), height, radius);
}
AABB LineNode::generateAabb() {
	AABB aabb = AABB::fromPoints({-glm::vec3(radius, height*0.5, radius), glm::vec3(radius, height*0.5, radius)});
	aabb.applyTransform(modelMatrix());
	return aabb;
}


CylinderNode::CylinderNode(Primrose::Node *parent, float radius) : PrimitiveNode(parent) {
	name = "Cylinder";
	this->radius = radius;
}
void CylinderNode::accept(NodeVisitor *visitor) { visitor->visit(this); }
std::string CylinderNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere(" "radius" "={}" "){}", prefix, name, radius, Node::toString());
}
void CylinderNode::serialize(rapidjson::Writer<rapidjson::OStreamWrapper> &writer) {
	writer.String("type");
	writer.String("cylinder");
	writer.String("radius");
	writer.Double(radius);
	Node::serialize(writer);
}
Primitive CylinderNode::toPrimitive() {
	return Primitive::Cylinder();
}
std::string CylinderNode::generateIntersectionGlsl() {
	return fmt::format("cylinderSDF({} * p, {})", glmToGlsl(glm::inverse(modelMatrix())), radius);
}
AABB CylinderNode::generateAabb() {
	AABB aabb = AABB::fromPoints({-glm::vec3(radius, 1000, radius), glm::vec3(radius, 1000, radius)});
	aabb.applyTransform(modelMatrix());
	return aabb;
}
