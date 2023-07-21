#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include <filesystem>
#include <rapidjson/schema.h>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <rapidjson/writer.h>
#include <rapidjson/ostreamwrapper.h>

#include "shader_structs.hpp"

namespace Primrose {
	enum NodeType {
		NODE_SPHERE = 1,
		NODE_BOX = 2,
		NODE_TORUS = 3,
		NODE_LINE = 4,
		NODE_CYLINDER = 5,
		NODE_UNION = 6,
		NODE_INTERSECTION = 7,
		NODE_DIFFERENCE = 8,
	};

	struct SceneNode {
		virtual NodeType type() = 0;
		virtual SceneNode* copy() = 0;

		bool isPrim();

		virtual std::string toString(std::string prefix);
		virtual void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		std::string name = "Node";
		SceneNode* parent = nullptr;
		bool hide = false;

		glm::vec3 translate = glm::vec3(0);
		glm::vec3 scale = glm::vec3(1);
		float angle = 0; // in degrees
		glm::vec3 axis = glm::vec3(0, 0, 1);

		std::vector<std::unique_ptr<SceneNode>> children;
	};

	class Scene {
	public:
		Scene() = default;
		Scene(std::filesystem::path sceneFile);

		void importScene(std::filesystem::path sceneFile);
		void saveScene(std::filesystem::path savePath);

		void generateUniforms();
		std::string toString();

		std::vector<std::unique_ptr<SceneNode>> root;
	private:
		static std::unique_ptr<rapidjson::SchemaDocument> schema;
		static void loadSchema();

		rapidjson::Document loadDoc(std::filesystem::path filePath);

		SceneNode* processNode(const rapidjson::Value& v, std::filesystem::path dir);
	};

	struct SphereNode : public SceneNode {
		SphereNode(float radius);

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		float radius;
	};
	struct BoxNode : public SceneNode {
		BoxNode(glm::vec3 size);

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		glm::vec3 size;
	};
	struct TorusNode : public SceneNode {
		TorusNode(float ringRadius, float majorRadius);

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		float ringRadius;
		float majorRadius;
	};
	struct LineNode : public SceneNode {
		LineNode(float height, float radius);

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		float height;
		float radius;
	};
	struct CylinderNode : public SceneNode {
		CylinderNode(float radius);

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		float radius;
	};

	struct UnionNode : public SceneNode {
		UnionNode();

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);
	};
	struct IntersectionNode : public SceneNode {
		IntersectionNode();

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);
	};
	struct DifferenceNode : public SceneNode {
		DifferenceNode();

		NodeType type();
		SceneNode* copy();
		std::string toString(std::string prefix);
		void serialize(rapidjson::Writer<rapidjson::OStreamWrapper>& writer);

		std::set<SceneNode*> subtractNodes;
	};

	bool operationsValid();
}

#endif
