#ifndef PRIMROSE_STATE_HPP
#define PRIMROSE_STATE_HPP

#include "shader_structs.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <map>

namespace Primrose {
	// gameplay
	extern float fov;
	extern float zoom;

	// engine logic
	extern float currentTime;
	extern float deltaTime;

	extern bool windowResized;
	extern bool windowMinimized;
	extern bool windowFocused;

	extern EngineUniform uniforms;

	extern std::vector<UIVertex> uiVertices;
	extern std::vector<uint16_t> uiIndices;
	extern VkImage uiTexture;
	extern VkDeviceMemory uiTextureMemory;
	extern VkImageView uiTextureImageView;
	extern VkSampler uiTextureSampler;

	// engine constants
	extern const std::vector<const char*> VALIDATION_LAYERS;
	extern const std::vector<const char*> REQUIRED_EXTENSIONS;

	extern const VkSurfaceFormatKHR IDEAL_SURFACE_FORMAT;
	extern const VkPresentModeKHR IDEAL_PRESENT_MODE;
	extern const std::map<int, const char*> PRESENT_MODE_STRINGS;

	extern const bool DYNAMIC_VIEWPORT;

	extern const int MAX_FRAMES_IN_FLIGHT;

	extern const char* ENGINE_NAME;
	extern const unsigned int ENGINE_VERSION;

	// application details
	extern const char* appName;
	extern unsigned int appVersion;

	// engine settings
	namespace Settings {
		extern const char* windowTitle;
		extern const int windowWidth;
		extern const int windowHeight;

		extern const bool validationEnabled;

		extern const int MAX_NUM_PRIMITIVES;
		extern const int MAX_NUM_OPERATIONS;

		extern const float mouseSens;
		extern const float fov;
	}
}

#endif
