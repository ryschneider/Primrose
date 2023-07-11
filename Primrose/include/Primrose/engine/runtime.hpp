#ifndef PRIMROSE_RUNTIME_HPP
#define PRIMROSE_RUNTIME_HPP

#include "setup.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Primrose {
	void setFov(float fov);
	void setZoom(float zoom);

	void run(void(*callback)(float));

	void updateUniforms(FrameInFlight& frame);
	void drawFrame();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, FrameInFlight& currentFlight);
}

#endif
