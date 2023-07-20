#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include <filesystem>
#include <rapidjson/schema.h>
#include <glm/gtx/quaternion.hpp>
#include <set>

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
		virtual std::string toString(std::string prefix);
		bool isPrim();

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
		std::string toString(std::string prefix);

		float radius;
	};
	struct BoxNode : public SceneNode {
		BoxNode(glm::vec3 size);
		NodeType type();
		std::string toString(std::string prefix);

		glm::vec3 size;
	};
	struct TorusNode : public SceneNode {
		TorusNode(float ringRadius, float majorRadius);
		NodeType type();
		std::string toString(std::string prefix);

		float ringRadius;
		float majorRadius;
	};
	struct LineNode : public SceneNode {
		LineNode(float height, float radius);
		NodeType type();
		std::string toString(std::string prefix);

		float height;
		float radius;
	};
	struct CylinderNode : public SceneNode {
		CylinderNode(float radius);
		NodeType type();
		std::string toString(std::string prefix);

		float radius;
	};

	struct UnionNode : public SceneNode {
		UnionNode();

		std::string toString(std::string prefix);
		NodeType type();
	};
	struct IntersectionNode : public SceneNode {
		IntersectionNode();

		std::string toString(std::string prefix);
		NodeType type();
	};
	struct DifferenceNode : public SceneNode {
		DifferenceNode();
		NodeType type();
		std::string toString(std::string prefix);

		std::set<SceneNode*> subtractNodes;
	};

	bool operationsValid();
}

#endif
