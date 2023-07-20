#include "scene.hpp"
#include "embed/scene_schema_json.h"

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <iostream>

using namespace Primrose;

std::unique_ptr<rapidjson::SchemaDocument> Scene::schema(nullptr);
void Scene::loadSchema() {
	rapidjson::Document sd;
	if (sd.Parse(sceneSchemaJsonData, sceneSchemaJsonSize).HasParseError()) {
		throw std::runtime_error("failed to parse scene json schema");
	}

	schema = std::make_unique<rapidjson::SchemaDocument>(sd);
}

rapidjson::Document Scene::loadDoc(std::filesystem::path filePath) {
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

static void updateParents(SceneNode* node) {
	for (const auto& child : node->children) {
		child->parent = node;
		updateParents(child.get());
	}
}

std::map<std::string, NodeType> jsonNodeType = {
	{"sphere", NodeType::NODE_SPHERE},
	{"box", NodeType::NODE_BOX},
	{"torus", NodeType::NODE_TORUS},
	{"line", NodeType::NODE_LINE},
	{"cylinder", NodeType::NODE_CYLINDER},
	{"union", NodeType::NODE_UNION},
	{"intersection", NodeType::NODE_INTERSECTION},
	{"difference", NodeType::NODE_DIFFERENCE},
};
SceneNode* Scene::processNode(const rapidjson::Value& v, std::filesystem::path dir) {
	std::string type = v["type"].GetString();
	SceneNode* node = nullptr;

	if (type == "ref") {
		std::filesystem::path ref = v["ref"].GetString();
		rapidjson::Document refDoc = loadDoc((dir / ref).string().c_str());

		if (refDoc.IsObject()) {
			node = processNode(refDoc, (dir / ref).parent_path().string().c_str());
		} else {
			node = new UnionNode();
			for (const auto& sub : refDoc.GetArray()) {
				node->children.emplace_back(processNode(sub, (dir / ref).parent_path()));
			}
		}
	} else {
		switch (jsonNodeType[type]) {
			case NODE_SPHERE:
				node = new SphereNode(v["radius"].GetFloat());
				break;
			case NODE_BOX:
			{
				glm::vec3 size;
				if (v["size"].IsArray()) {
					auto a = v["size"].GetArray();
					size = glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
				} else {
					size = glm::vec3(v["size"].GetFloat());
				}
				node = new BoxNode(size);
			}
				break;
			case NODE_TORUS:
				node = new TorusNode(v["ringRadius"].GetFloat(), v["majorRadius"].GetFloat());
				break;
			case NODE_LINE:
				node = new LineNode(v["height"].GetFloat(), v["radius"].GetFloat());
				break;
			case NODE_CYLINDER:
				node = new CylinderNode(v["radius"].GetFloat());
				break;
			case NODE_UNION:
				node = new UnionNode();
				break;
			case NODE_INTERSECTION:
				node = new IntersectionNode();
				break;
			case NODE_DIFFERENCE:
				node = new DifferenceNode();
				break;
		}
	}

	if (v.HasMember("name")) {
		node->name = v["name"].GetString();
	}

	if (v.HasMember("children")) {
		for (const auto& child : v["children"].GetArray()) {
			node->children.emplace_back(processNode(child, dir));
		}
	}

	if (type == "difference" && v.HasMember("subtractIndices")) {
		for (const auto& sub : v["subtractIndices"].GetArray()) {
			int i = sub.GetInt();
			((DifferenceNode*)node)->subtractNodes.insert(node->children[i].get());
		}

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

Scene::Scene(std::filesystem::path sceneFile) {
	rapidjson::Document doc = loadDoc(sceneFile.string().c_str());

	if (doc.IsObject()) {
		root.emplace_back(processNode(doc, sceneFile.parent_path()));
	} else {
		for (const auto& v : doc.GetArray()) {
			root.emplace_back(processNode(v, sceneFile.parent_path()));
		}
	}

	for (const auto& node : root) {
		updateParents(node.get());
	}
}
