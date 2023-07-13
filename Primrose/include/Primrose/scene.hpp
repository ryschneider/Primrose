#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include <filesystem>
#include <rapidjson/schema.h>
#include <glm/gtx/quaternion.hpp>

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
	public:
		virtual NodeType type() = 0;
		virtual std::string toString(std::string prefix);

		glm::vec3 translate = glm::vec3(0);
		glm::vec3 scale = glm::vec3(1);

		float angle = 0; // in degrees
		glm::vec3 axis = glm::vec3(0, 0, 1);
	};

	class Scene {
	public:
		Scene(const char* sceneFile);

		void load();
		std::string toString();

	private:
		static std::unique_ptr<rapidjson::SchemaDocument> schema;

		static void loadSchema();
		SceneNode* processNode(const rapidjson::Value& v);

		std::vector<std::unique_ptr<SceneNode>> root;
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

	struct MultiNode : public SceneNode {
		std::string toString(std::string indent);
		std::vector<std::unique_ptr<SceneNode>> objects;
	};
	struct UnionNode : public MultiNode {
		std::string toString(std::string prefix);
		NodeType type();
	};
	struct IntersectionNode : public MultiNode {
		std::string toString(std::string prefix);
		NodeType type();
	};
	struct DifferenceNode : public SceneNode {
		DifferenceNode(SceneNode* base, SceneNode* subtract);
		NodeType type();
		std::string toString(std::string prefix);

		std::unique_ptr<SceneNode> base;
		std::unique_ptr<SceneNode> subtract;
	};

	bool operationsValid();
}

#endif
