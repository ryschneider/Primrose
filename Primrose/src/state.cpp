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
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
	};
	const std::vector<const char*> RAY_EXTENSIONS = {
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
	};

	const vk::SurfaceFormatKHR IDEAL_SURFACE_FORMAT = vk::SurfaceFormatKHR(
		vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
	//const vk::PresentModeKHR IDEAL_PRESENT_MODE = vk::PresentModeKHR::eMailbox;
	const vk::PresentModeKHR IDEAL_PRESENT_MODE = vk::PresentModeKHR::eImmediate;

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
