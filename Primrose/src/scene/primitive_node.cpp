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
}

SphereNode::SphereNode(Primrose::Node* parent, float radius) : PrimitiveNode(parent), radius(radius) {
	name = "Sphere";
}
std::string SphereNode::toString(std::string prefix) {
	return fmt::format("{}{}/sphere({}){}", prefix, name, radius, Node::toString());
}
glm::mat4 SphereNode::modelMatrix() {
	glm::mat4 matrix = getParent()->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale * radius);
	return matrix;
}
void SphereNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
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
std::pair<glm::vec3, glm::vec3> SphereNode::generateAabb() {
	std::vector<glm::vec3> verts = {
		{0, 0, 1}, {0, 0, -1},
		{0, 1, 0}, {0, -1, 0},
		{1, 0, 0}, {-1, 0, 0}
	};

	auto[lesser, greater] = UnionNode::generateAabb();
	glm::mat4 matrix = modelMatrix();
	matrix = transformMatrix(getTranslate(matrix), getScale(matrix), 0, glm::vec3(1)); // ignore rotation
	for (auto& v : verts) {
		v = matrix * glm::vec4(v, 1);
		lesser.x = std::min(lesser.x, v.x); greater.x = std::max(greater.x, v.x);
		lesser.y = std::min(lesser.y, v.y); greater.y = std::max(greater.y, v.y);
		lesser.z = std::min(lesser.z, v.z); greater.z = std::max(greater.z, v.z);
	}

	return {lesser, greater};
}


BoxNode::BoxNode(Node* parent, glm::vec3 size) : PrimitiveNode(parent), size(size) {
	name = "Box";
}
std::string BoxNode::toString(std::string prefix) {
	return fmt::format("{}{}/box(({}, {}, {})){}", prefix, name, size.x, size.y, size.z, Node::toString());
}
glm::mat4 BoxNode::modelMatrix() {
	glm::mat4 matrix = getParent()->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale * size);
	return matrix;
}
void BoxNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
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
std::pair<glm::vec3, glm::vec3> BoxNode::generateAabb() {
	std::vector<glm::vec3> verts = {
		{-1, -1, -1}, {-1, -1, +1},
		{-1, +1, -1}, {-1, +1, +1},
		{+1, -1, -1}, {+1, -1, +1},
		{+1, +1, -1}, {+1, +1, +1}
	};

	auto[lesser, greater] = UnionNode::generateAabb();
	glm::mat4 matrix = modelMatrix();
	for (auto& v : verts) {
		v = matrix * glm::vec4(v, 1);
		lesser.x = std::min(lesser.x, v.x); greater.x = std::max(greater.x, v.x);
		lesser.y = std::min(lesser.y, v.y); greater.y = std::max(greater.y, v.y);
		lesser.z = std::min(lesser.z, v.z); greater.z = std::max(greater.z, v.z);
	}

	return {lesser, greater};
}


TorusNode::TorusNode(Node* parent, float ringRadius, float majorRadius)
	: PrimitiveNode(parent), ringRadius(ringRadius), majorRadius(majorRadius) {
	name = "Torus";
}
std::string TorusNode::toString(std::string prefix) {
	return fmt::format("{}{}/torus({}, {}){}", prefix, name, ringRadius, majorRadius, Node::toString());
}
glm::mat4 TorusNode::modelMatrix() {
	glm::mat4 matrix = getParent()->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale * majorRadius);
	return matrix;
}
void TorusNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
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
std::pair<glm::vec3, glm::vec3> TorusNode::generateAabb() {
	float r = (ringRadius / majorRadius) + 1;
	std::vector<glm::vec3> verts = {
		{0, 0, r}, {0, 0, -r},
		{0, r, 0}, {0, -r, 0},
		{r, 0, 0}, {-r, 0, 0}
	};

	auto[lesser, greater] = UnionNode::generateAabb();
	glm::mat4 matrix = modelMatrix();
	matrix = transformMatrix(getTranslate(matrix), getScale(matrix), 0, glm::vec3(1)); // ignore rotation
	for (auto& v : verts) {
		v = matrix * glm::vec4(v, 1);
		lesser.x = std::min(lesser.x, v.x); greater.x = std::max(greater.x, v.x);
		lesser.y = std::min(lesser.y, v.y); greater.y = std::max(greater.y, v.y);
		lesser.z = std::min(lesser.z, v.z); greater.z = std::max(greater.z, v.z);
	}

	return {lesser, greater};
}


LineNode::LineNode(Node* parent, float height, float radius)
	: PrimitiveNode(parent), height(height), radius(radius) {
	name = "Line";
}
std::string LineNode::toString(std::string prefix) {
	return fmt::format("{}{}/line({}, {}){}", prefix, name, height, radius, Node::toString());
}
glm::mat4 LineNode::modelMatrix() {
	glm::mat4 matrix = getParent()->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale * radius);
	return matrix;
}
void LineNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
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
std::pair<glm::vec3, glm::vec3> LineNode::generateAabb() {
	float h = height*0.5 / radius;
	std::vector<glm::vec3> verts = {
		{-1, -h, -1}, {-1, -h, +1},
		{-1, +h, -1}, {-1, +h, +1},
		{+1, -h, -1}, {+1, -h, +1},
		{+1, +h, -1}, {+1, +h, +1}
	};

	auto[lesser, greater] = UnionNode::generateAabb();
	glm::mat4 matrix = modelMatrix();
	for (auto& v : verts) {
		v = matrix * glm::vec4(v, 1);
		lesser.x = std::min(lesser.x, v.x); greater.x = std::max(greater.x, v.x);
		lesser.y = std::min(lesser.y, v.y); greater.y = std::max(greater.y, v.y);
		lesser.z = std::min(lesser.z, v.z); greater.z = std::max(greater.z, v.z);
	}

	return {lesser, greater};
}


CylinderNode::CylinderNode(Node* parent, float radius) : PrimitiveNode(parent), radius(radius) {
	name = "Cylinder";
}
std::string CylinderNode::toString(std::string prefix) {
	return fmt::format("{}{}/cylinder({}){}", prefix, name, radius, Node::toString());
}
glm::mat4 CylinderNode::modelMatrix() {
	glm::mat4 matrix = getParent()->modelMatrix();
	matrix = glm::translate(matrix, translate);
	matrix = glm::rotate(matrix, glm::radians(angle), axis);
	matrix = glm::scale(matrix, scale * radius);
	return matrix;
}
void CylinderNode::accept(NodeVisitor* visitor) {
	visitor->visit(this);
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
std::pair<glm::vec3, glm::vec3> CylinderNode::generateAabb() {
	float b = 1000; // arbitrary large number
	std::vector<glm::vec3> verts = {
		{-1, -b, -1}, {-1, -b, +1},
		{-1, +b, -1}, {-1, +b, +1},
		{+1, -b, -1}, {+1, -b, +1},
		{+1, +b, -1}, {+1, +b, +1}
	};

	auto[lesser, greater] = UnionNode::generateAabb();
	glm::mat4 matrix = modelMatrix();
	for (auto& v : verts) {
		v = matrix * glm::vec4(v, 1);
		lesser.x = std::min(lesser.x, v.x); greater.x = std::max(greater.x, v.x);
		lesser.y = std::min(lesser.y, v.y); greater.y = std::max(greater.y, v.y);
		lesser.z = std::min(lesser.z, v.z); greater.z = std::max(greater.z, v.z);
	}

	return {lesser, greater};
}
