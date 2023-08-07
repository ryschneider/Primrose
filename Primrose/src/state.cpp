#include "state.hpp"

namespace Primrose {
	// gameplay
	float fov = glm::radians(90.f); // changes during gameplay
	float zoom = 1.f;

	// engine logic
	float currentTime;
	float deltaTime;

	bool windowResized = false;
	bool windowMinimized = false;
	bool windowFocused = false;

	MarchUniforms uniforms = {};

	std::vector<std::unique_ptr<UIElement>> uiScene{};

	const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> REQUIRED_EXTENSIONS = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	const std::vector<const char*> RAY_EXTENSIONS = {
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
	};

	const VkSurfaceFormatKHR IDEAL_SURFACE_FORMAT = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	//const VkPresentModeKHR IDEAL_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
	const VkPresentModeKHR IDEAL_PRESENT_MODE = VK_PRESENT_MODE_IMMEDIATE_KHR;
	const std::map<int, const char*> PRESENT_MODE_STRINGS = {
		{ VK_PRESENT_MODE_IMMEDIATE_KHR, "IMMEDIATE" },
		{ VK_PRESENT_MODE_MAILBOX_KHR, "MAILBOX" },
		{ VK_PRESENT_MODE_FIFO_KHR, "FIFO" },
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR, "FIFO_RELAXED" },
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "SHARED_DEMAND_REFRESH" },
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "SHARED_CONTINUOUS_REFRESH" }
	};

	const bool DYNAMIC_VIEWPORT = true;

	const int MAX_FRAMES_IN_FLIGHT = 2;

	const char* ENGINE_NAME = "Primrose";
	const unsigned int ENGINE_VERSION = 001'000'000;

	// application details
	const char* appName;
	unsigned int appVersion;

	// engine settings
	namespace Settings {
		const char* windowTitle = "Primrose";
#ifdef _WIN32
		const int windowWidth = 1920;
		const int windowHeight = 1080;
#elif __APPLE__
		const int windowWidth = 600;
		const int windowHeight = 400;
#endif

#ifdef NDEBUG
		const bool validationEnabled = false;
#else
		const bool validationEnabled = true;
#endif

		const int MAX_NUM_PRIMITIVES = 100;
		const int MAX_NUM_OPERATIONS = 100;

		const float mouseSens = 0.003f;
		const float fov = glm::radians(90.f); // default fov (can change in runtime)
	}
}
