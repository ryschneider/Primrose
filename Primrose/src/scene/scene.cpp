#include "scene/scene.hpp"
#include "embed/scene_schema_json.h"
#include "scene/primitive_node.hpp"
#include "scene/construction_node.hpp"
#include "state.hpp"

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <iostream>
#include <fstream>

using namespace Primrose;

Scene::Scene(std::filesystem::path sceneFile) {
	importScene(sceneFile);
}

void Scene::importScene(std::filesystem::path sceneFile) {
	rapidjson::Document doc = loadDoc(sceneFile.string().c_str());

	if (doc.IsObject()) {
		processJson((Node*)&root, doc, sceneFile.parent_path());
	} else {
		for (const auto& v : doc.GetArray()) {
			processJson((Node*)&root, v, sceneFile.parent_path());
		}
	}
}

Node* Scene::processJson(Node* parent, const rapidjson::Value& v, std::filesystem::path dir) {
	std::string type = v["type"].GetString();

	Node* node;

	if (type == "ref") {
		std::filesystem::path ref = v["ref"].GetString();
		rapidjson::Document refDoc = loadDoc((dir / ref).string().c_str());

		if (refDoc.IsObject()) {
			node = processJson(parent, refDoc, (dir / ref).parent_path().string().c_str());
		} else {
			node = new UnionNode(parent);
			for (const auto& sub : refDoc.GetArray()) {
				processJson(node, sub, (dir / ref).parent_path());
			}
		}
	} else if (type == "sphere") {
		node = new SphereNode(parent, v["radius"].GetFloat());
	} else if (type == "box") {
		glm::vec3 size;
		if (v["size"].IsArray()) {
			auto a = v["size"].GetArray();
			size = glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
		} else {
			size = glm::vec3(v["size"].GetFloat());
		}
		node = new BoxNode(parent, size);
	} else if (type == "torus") {
		node = new TorusNode(parent, v["ringRadius"].GetFloat(), v["majorRadius"].GetFloat());
	} else if (type == "line") {
		node = new LineNode(parent, v["height"].GetFloat(), v["radius"].GetFloat());
	} else if (type == "cylinder") {
		node = new CylinderNode(parent, v["radius"].GetFloat());
	} else if (type == "union") {
		node = new UnionNode(parent);
	} else if (type == "intersection") {
		node = new IntersectionNode(parent);
	} else if (type == "difference") {
		node = new DifferenceNode(parent);
	}

	if (v.HasMember("name")) {
		node->name = v["name"].GetString();
	}

	if (v.HasMember("children")) {
		for (const auto& child : v["children"].GetArray()) {
			processJson(node, child, dir);
		}
	}

	if (type == "difference" && v.HasMember("subtractIndices")) {
		for (const auto& sub : v["subtractIndices"].GetArray()) {
			int i = sub.GetInt();
			((DifferenceNode*)node)->subtractNodes.insert(node->getChildren()[i].get());
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



void Scene::saveScene(std::filesystem::path savePath) {
	std::ofstream stream(savePath);
	rapidjson::OStreamWrapper osw(stream);
	rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);

	if (root.getChildren().size() == 1) {
		writer.StartObject();
		root.getChildren()[0]->serialize(writer);
		writer.EndObject();
	} else {
		writer.StartArray();
		for (const auto& node : root.getChildren()) {
			writer.StartObject();
			node->serialize(writer);
			writer.EndObject();
		}
		writer.EndArray();
	}
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

std::unique_ptr<rapidjson::SchemaDocument> Scene::schema(nullptr);
void Scene::loadSchema() {
	rapidjson::Document sd;
	if (sd.Parse(sceneSchemaJsonData, sceneSchemaJsonSize).HasParseError()) {
		throw std::runtime_error("failed to parse scene json schema");
	}

	schema = std::make_unique<rapidjson::SchemaDocument>(sd);
}



std::string Primrose::Scene::toString() {
	std::string out = "";
	for (const auto& node : root.getChildren()) {
		out += node->toString() + "\n";
	}
	return out;
}

void Primrose::Scene::generateUniforms() {
	std::vector<Primitive> prims = root.extractPrims();
	std::vector<Transformation> transforms = root.extractTransforms();
	std::vector<Operation> ops;
	root.createOperations(prims, transforms, ops);

	if (prims.size() > 100 || transforms.size() > 100 || ops.size() > 100) {
		throw std::runtime_error("vector exceeded buffer size");
	}

	uniforms.numOperations = ops.size();
	std::copy(ops.begin(), ops.end(), uniforms.operations);
	std::copy(prims.begin(), prims.end(), uniforms.primitives);
	std::copy(transforms.begin(), transforms.end(), uniforms.transformations);
}
