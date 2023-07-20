#include "scene.hpp"
#include "state.hpp"
#include "shader_structs.hpp"
#include "embed/scene_schema_json.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <fmt/format.h>

std::unique_ptr<rapidjson::SchemaDocument> Primrose::Scene::schema(nullptr);
void Primrose::Scene::loadSchema() {
	rapidjson::Document sd;
	if (sd.Parse(sceneSchemaJsonData, sceneSchemaJsonSize).HasParseError()) {
		throw std::runtime_error("failed to parse scene json schema");
	}

	schema = std::make_unique<rapidjson::SchemaDocument>(sd);
}

rapidjson::Document Primrose::Scene::loadDoc(std::filesystem::path filePath) {
	if (schema.get() == nullptr) loadSchema();

	std::cout << "Loading scene from " << filePath.make_preferred() << std::endl;

	FILE* file = fopen(filePath.string().c_str(), "rb");

	char buffer[65536];
	rapidjson::FileReadStream stream(file, buffer, sizeof(buffer));

	rapidjson::SchemaValidatingReader<rapidjson::kParseDefaultFlags, rapidjson::FileReadStream, rapidjson::UTF8<>>
		reader(stream, *schema);

	rapidjson::Document doc;
	doc.Populate(reader);

	if (!reader.GetParseResult()) {
		if (!reader.IsValid()) {
			rapidjson::StringBuffer sb;
			reader.GetInvalidSchemaPointer().StringifyUriFragment(sb);
			std::cout << "invalid schema: " << sb.GetString() << std::endl;
			std::cout << "invalid keyword: " << reader.GetInvalidSchemaKeyword() << std::endl;
			sb.Clear();
			reader.GetInvalidDocumentPointer().StringifyUriFragment(sb);
			std::cout << "invalid document: " << sb.GetString() << std::endl;
			throw std::runtime_error("failed to validate json scene");
		}
	}

	fclose(file);

	return doc;
}

Primrose::Scene::Scene(std::filesystem::path sceneFile) {
	rapidjson::Document doc = loadDoc(sceneFile.string().c_str());

	if (doc.IsObject()) {
		root.emplace_back(processNode(doc, sceneFile.parent_path()));
	} else {
		for (const auto& v : doc.GetArray()) {
			root.emplace_back(processNode(v, sceneFile.parent_path()));
		}
	}
}

Primrose::SceneNode* Primrose::Scene::processNode(const rapidjson::Value& v, std::filesystem::path dir) {
	const char* type = v["type"].GetString();
	SceneNode* node = nullptr;

	if (strcmp(type, "sphere") == 0) {
		node = new SphereNode(v["radius"].GetFloat());
	} else if (strcmp(type, "box") == 0) {
		glm::vec3 size;
		if (v["size"].IsArray()) {
			auto a = v["size"].GetArray();
			size = glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
		} else {
			size = glm::vec3(v["size"].GetFloat());
		}
		node = new BoxNode(size);
	} else if (strcmp(type, "torus") == 0) {
		node = new TorusNode(v["ringRadius"].GetFloat(), v["majorRadius"].GetFloat());
	} else if (strcmp(type, "line") == 0) {
		node = new LineNode(v["height"].GetFloat(), v["radius"].GetFloat());
	} else if (strcmp(type, "cylinder") == 0) {
		node = new CylinderNode(v["radius"].GetFloat());
	} else if (strcmp(type, "union") == 0 || strcmp(type, "intersection") == 0) {
		if (strcmp(type, "union") == 0) {
			node = new UnionNode();
		} else {
			node = new IntersectionNode();
		}

		for (const auto& sub : v["objects"].GetArray()) {
			((UnionNode*)node)->objects.emplace_back(processNode(sub, dir));
		}
	} else if (strcmp(type, "difference") == 0) {
		node = new DifferenceNode(processNode(v["base"], dir), processNode(v["subtract"], dir));
	} else if (strcmp(type, "ref") == 0) {
		std::filesystem::path ref = v["ref"].GetString();
		rapidjson::Document refDoc = loadDoc((dir / ref).string().c_str());

		if (refDoc.IsObject()) {
			node = processNode(refDoc, (dir / ref).parent_path().string().c_str());
		} else {
			node = new UnionNode();
			for (const auto& sub : refDoc.GetArray()) {
				((UnionNode*)node)->objects.emplace_back(processNode(sub, (dir / ref).parent_path()));
			}
		}
	} else {
		throw std::runtime_error("failed to identify scene node");
	}

	if (v.HasMember("transform")) {
		const auto& t = v["transform"];
		if (t.HasMember("position")) {
			node->translate.x += t["position"][0].GetFloat();
			node->translate.y += t["position"][1].GetFloat();
			node->translate.z += t["position"][2].GetFloat();
		}
		if (t.HasMember("scale")) {
			if (t["scale"].IsArray()) {
				node->scale.x *= t["scale"][0].GetFloat();
				node->scale.y *= t["scale"][1].GetFloat();
				node->scale.z *= t["scale"][2].GetFloat();
			} else {
				node->scale *= glm::vec3(t["scale"].GetFloat());
			}
		}
		if (t.HasMember("rotation")) {
			auto rotVal = t["rotation"].GetObject();
			glm::quat orig = glm::angleAxis(glm::radians(node->angle), node->axis);
			glm::quat rot = glm::angleAxis(glm::radians(rotVal["angle"].GetFloat()),
				glm::vec3(rotVal["axis"][0].GetFloat(), rotVal["axis"][1].GetFloat(), rotVal["axis"][2].GetFloat()));

			glm::quat result = orig * rot;
			node->angle = glm::degrees(glm::angle(result));
			node->axis = glm::axis(result);
		}
	}

	return node;
}

std::string Primrose::Scene::toString() {
	std::string out = "";
	for (const auto& node : root) {
		out += node->toString("") + "\n";
	}
	return out;
}



namespace {
	using namespace Primrose;

	static Primitive toPrim(Primrose::SceneNode* node) {
		switch (node->type()) {
			case NODE_SPHERE:
				return Primitive::Sphere();
			case NODE_BOX:
				return Primitive::Box();
			case NODE_TORUS:
				return Primitive::Torus(((TorusNode*)node)->ringRadius / ((TorusNode*)node)->majorRadius);
			case NODE_LINE:
				return Primitive::Line(((LineNode*)node)->height / ((LineNode*)node)->radius);
			case NODE_CYLINDER:
				return Primitive::Cylinder();
			case NODE_UNION:
			case NODE_INTERSECTION:
			case NODE_DIFFERENCE:
				return Primitive();
		}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
	}
#pragma GCC diagnostic pop

	static glm::mat4 toMatrix(Primrose::SceneNode* node) {
		glm::vec3 scale = glm::vec3(1);
		switch (node->type()) {
			case NODE_SPHERE:
				scale = glm::vec3(((SphereNode*)node)->radius);
				break;
			case NODE_BOX:
				scale = ((BoxNode*)node)->size;
				break;
			case NODE_TORUS:
				scale = glm::vec3(((TorusNode*)node)->majorRadius);
				break;
			case NODE_LINE:
				scale = glm::vec3(((LineNode*)node)->radius);
				break;
			case NODE_CYLINDER:
				scale = glm::vec3(((CylinderNode*)node)->radius);
				break;
			case NODE_UNION:
			case NODE_INTERSECTION:
			case NODE_DIFFERENCE:
				break;
		}

		return transformMatrix(node->translate, node->scale * scale, node->angle * M_PI / 180.f, node->axis);
	}

	static void extractPrims(std::vector<Primitive>& prims, Primrose::SceneNode* node) {
		switch (node->type()) {
			case NODE_SPHERE:
			case NODE_BOX:
			case NODE_TORUS:
			case NODE_LINE:
			case NODE_CYLINDER: {
				Primitive p = toPrim(node);

				if (std::find(prims.begin(), prims.end(), p) == prims.end()) {
					prims.push_back(p);
				}

				return;
			}
			case NODE_UNION:
			case NODE_INTERSECTION:
				for (const auto &ptr: ((MultiNode*)node)->objects) {
					extractPrims(prims, ptr.get());
				}
				return;
			case NODE_DIFFERENCE:
				extractPrims(prims, ((DifferenceNode*)node)->base.get());
				extractPrims(prims, ((DifferenceNode*)node)->subtract.get());
				return;
		}
	}

	static void extractTransforms(std::vector<Transformation> &transforms,
		SceneNode *node, glm::mat4 matrix = glm::mat4(1)) {

		glm::mat4 nodeMatrix = matrix * toMatrix(node);
		Transformation t = Transformation(nodeMatrix);

		if (std::find(transforms.begin(), transforms.end(), t) == transforms.end()) {
			transforms.push_back(t);
		}

		switch (node->type()) {
			case NODE_SPHERE:
			case NODE_BOX:
			case NODE_TORUS:
			case NODE_LINE:
			case NODE_CYLINDER:
				break;
			case NODE_UNION:
			case NODE_INTERSECTION:
				for (const auto& ptr : ((MultiNode*)node)->objects) {
					extractTransforms(transforms, ptr.get(), nodeMatrix);
				}
				break;
			case NODE_DIFFERENCE:
				extractTransforms(transforms, ((DifferenceNode *) node)->base.get(), nodeMatrix);
				extractTransforms(transforms, ((DifferenceNode *) node)->subtract.get(), nodeMatrix);
				break;
		}
	}

	static uint createOperations(std::vector<Primitive> &prims, std::vector<Transformation> &transforms,
		std::vector<Operation> &ops, SceneNode *node, glm::mat4 matrix = glm::mat4(1)) {

		glm::mat4 nodeMatrix = matrix * toMatrix(node);
		Transformation transform = Transformation(nodeMatrix);

		switch (node->type()) {
			case NODE_SPHERE:
			case NODE_BOX:
			case NODE_TORUS:
			case NODE_LINE:
			case NODE_CYLINDER: {
				uint primI = std::find(prims.begin(), prims.end(), toPrim(node)) - prims.begin();
				uint transformI = std::find(transforms.begin(), transforms.end(), transform) - transforms.begin();

				// TODO :: combine OP_IDENTITY and OP_TRANSFORM
				ops.push_back(Operation::Transform(transformI));
				ops.push_back(Operation::Identity(primI));
				return ops.size() - 1;
			}
			case NODE_UNION:
			case NODE_INTERSECTION: {
				MultiNode* multi = (MultiNode*)node;

				std::vector<uint> objectOps;
				for (const auto& o : multi->objects) {
					uint index = createOperations(prims, transforms, ops, o.get(), nodeMatrix);
					if (index != -1) objectOps.push_back(index);
				}

				if (objectOps.size() == 0) {
					return -1;
				} else if (objectOps.size() == 1) {
					return objectOps[0];
				}

				uint index = objectOps[0];
				for (int i = 1; i < objectOps.size(); ++i) {
					if (multi->type() == NODE_UNION) ops.push_back(Operation::Union(index, objectOps[i]));
					if (multi->type() == NODE_INTERSECTION) ops.push_back(Operation::Intersection(index, objectOps[i]));
					index = ops.size() - 1;
				}

				return index;
			}
			case NODE_DIFFERENCE: {
				DifferenceNode* dif = (DifferenceNode*)node;

				uint base = createOperations(prims, transforms, ops, dif->base.get(), nodeMatrix);
				uint subtract = createOperations(prims, transforms, ops, dif->subtract.get(), nodeMatrix);

				if (base == -1) return -1;
				if (subtract == -1) return base;

				ops.push_back(Operation::Difference(base, subtract));
				return ops.size() - 1;
			}
		}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
	}
#pragma GCC diagnostic pop
}

void Primrose::Scene::generateUniforms() {
	std::vector<Primitive> prims;
	std::vector<Transformation> transforms;
	std::vector<Operation> ops;
	for (const auto& node : root) {
		extractPrims(prims, node.get()); // extract all unique primitives
		extractTransforms(transforms, node.get()); // extract all unique transforms

		uint index = createOperations(prims, transforms, ops, node.get());
		ops.push_back(Operation::Render(index));
	}

	if (prims.size() > 100 || transforms.size() > 100 || ops.size() > 100) {
		throw std::runtime_error("vector exceeded buffer size");
	}

	uniforms.numOperations = ops.size();
	std::copy(ops.begin(), ops.end(), uniforms.operations);
	std::copy(prims.begin(), prims.end(), uniforms.primitives);
	std::copy(transforms.begin(), transforms.end(), uniforms.transformations);
}


std::string Primrose::SceneNode::toString(std::string prefix) {
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



Primrose::SphereNode::SphereNode(float radius) : radius(radius) {
	name = "Sphere";
}
Primrose::NodeType Primrose::SphereNode::type() { return NodeType::NODE_SPHERE; }
std::string Primrose::SphereNode::toString(std::string prefix) {
	return fmt::format("{}sphere({}){}", prefix, radius, SceneNode::toString(""));
}

Primrose::BoxNode::BoxNode(glm::vec3 size) : size(size) {
	name = "Box";
}
Primrose::NodeType Primrose::BoxNode::type() { return NodeType::NODE_BOX; }
std::string Primrose::BoxNode::toString(std::string prefix) {
	return fmt::format("{}box(({}, {}, {})){}", prefix, size.x, size.y, size.z, SceneNode::toString(""));
}

Primrose::TorusNode::TorusNode(float ringRadius, float majorRadius) : ringRadius(ringRadius), majorRadius(majorRadius) {
	name = "Torus";
}
Primrose::NodeType Primrose::TorusNode::type() { return NodeType::NODE_TORUS; }
std::string Primrose::TorusNode::toString(std::string prefix) {
	return fmt::format("{}torus({}, {}){}", prefix, ringRadius, majorRadius, SceneNode::toString(""));
}

Primrose::LineNode::LineNode(float height, float radius) : height(height), radius(radius) {
	name = "Line";
}
Primrose::NodeType Primrose::LineNode::type() { return NodeType::NODE_LINE; }
std::string Primrose::LineNode::toString(std::string prefix) {
	return fmt::format("{}line({}, {}){}", prefix, height, radius, SceneNode::toString(""));
}

Primrose::CylinderNode::CylinderNode(float radius) : radius(radius) {
	name = "Cylinder";
}
Primrose::NodeType Primrose::CylinderNode::type() { return NodeType::NODE_CYLINDER; }
std::string Primrose::CylinderNode::toString(std::string prefix) {
	return fmt::format("{}cylinder({}){}", prefix, radius, SceneNode::toString(""));
}

Primrose::MultiNode::MultiNode(std::vector<SceneNode*> nodes) {
	for (const auto& ptr : nodes) {
		objects.emplace_back(ptr);
	}
}
std::string Primrose::MultiNode::toString(std::string prefix) {
	std::string out = "";
	for (const auto& node : objects) {
		out += fmt::format("\n{}", node->toString(prefix));
	}
	return out;
}

Primrose::UnionNode::UnionNode(std::vector<SceneNode*> nodes) : MultiNode(nodes) {
	name = "Union";
}
Primrose::NodeType Primrose::UnionNode::type() { return NodeType::NODE_UNION; }
std::string Primrose::UnionNode::toString(std::string prefix) {
	return fmt::format("{}union{}", prefix, MultiNode::toString(prefix + "  | "));
}

Primrose::IntersectionNode::IntersectionNode(std::vector<SceneNode*> nodes) : MultiNode(nodes) {
	name = "Intersection";
}
Primrose::NodeType Primrose::IntersectionNode::type() { return NodeType::NODE_INTERSECTION; }
std::string Primrose::IntersectionNode::toString(std::string prefix) {
	return fmt::format("{}intersection{}", prefix, MultiNode::toString(prefix + "  & "));
}

Primrose::DifferenceNode::DifferenceNode(SceneNode* base, SceneNode* subtract) : base(base), subtract(subtract) {
	name = "Difference";
}
Primrose::NodeType Primrose::DifferenceNode::type() { return NodeType::NODE_DIFFERENCE; }
std::string Primrose::DifferenceNode::toString(std::string prefix) {
	return fmt::format("difference\n{}\n{}", base->toString(prefix + "  + "), subtract->toString(prefix + "  - "));
}


bool Primrose::operationsValid() {
	for (int i = 0; i < uniforms.numOperations; ++i) {
		Operation op = uniforms.operations[i];

		if (op.type == OP::IDENTITY || op.type == OP::TRANSFORM) {
			continue;
		} else if (op.type == OP::UNION || op.type == OP::INTERSECTION || op.type == OP::DIFFERENCE) {
			if (op.i >= i || op.j >= i) return false;
			Operation op1 = uniforms.operations[op.i];
			Operation op2 = uniforms.operations[op.j];
			if (op1.type == OP::TRANSFORM || op2.type == OP::TRANSFORM) return false;
		} else {
			return false;
		}
	}

	return true;
}
