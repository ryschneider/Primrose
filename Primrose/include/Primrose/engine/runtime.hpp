#ifndef PRIMROSE_RUNTIME_HPP
#define PRIMROSE_RUNTIME_HPP

#include "setup.hpp"

namespace Primrose {
	void setFov(float fov);
	void setZoom(float zoom);

	void run(void(*callback)(float));

	void updateUniforms(FrameInFlight& frame);
	void drawFrame();

	void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex, FrameInFlight& currentFlight);
}

#endif
