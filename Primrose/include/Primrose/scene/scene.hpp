#ifndef PRIMROSE_SCENE_H
#define PRIMROSE_SCENE_H

#include <filesystem>
#include <rapidjson/schema.h>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <rapidjson/writer.h>
#include <rapidjson/ostreamwrapper.h>

#include "../shader_structs.hpp"
#include "node.hpp"

namespace Primrose {
	class Scene {
	public:
		Scene() = default;
		Scene(std::filesystem::path sceneFile);

		void importScene(std::filesystem::path sceneFile);
		void saveScene(std::filesystem::path savePath);

		void generateUniforms();
		std::string toString();

		RootNode root;

	private:
		static std::unique_ptr<rapidjson::SchemaDocument> schema;
		static void loadSchema();

		rapidjson::Document loadDoc(std::filesystem::path filePath);

		Node* processJson(Node* parent, const rapidjson::Value& v, std::filesystem::path dir);
	};
}

#endif
