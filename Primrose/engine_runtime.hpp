#ifndef PRIMROSE_ENGINE_RUNTIME_HPP
#define PRIMROSE_ENGINE_RUNTIME_HPP

#include "engine.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Primrose {
	void setFov(float fov);
	void setZoom(float zoom);

	void run(void(*callback)());

	void updateUniforms(FrameInFlight& frame);
	bool drawFrame();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, FrameInFlight& currentFlight);
	void updateFps(bool init);
}

#endif
