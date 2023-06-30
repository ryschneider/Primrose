#include "engine_runtime.hpp"
#include "state.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>

void Primrose::setFov(float newFov) {
	fov = newFov;
	uniforms.focalLength = 1.f / tanf(fov / 2.f);
}
void Primrose::setZoom(float newZoom) {
	zoom = newZoom;
	uniforms.invZoom = 1.f / zoom;
}

void Primrose::run(void(*callback)()) {
	float lastTime = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;

		glfwPollEvents();
		callback();
		bool drawn = drawFrame();

		// don't count as frame if not drawn
		if (drawn) updateFps(false);

		lastTime = currentTime;
	}

	vkDeviceWaitIdle(device);

	std::cout << std::endl;

	cleanup();
}

void Primrose::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, FrameInFlight& currentFlight) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording cmd buffer");
	}

	VkRenderPassBeginInfo renderBeginInfo{};
	renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginInfo.renderPass = renderPass;
	renderBeginInfo.framebuffer = swapchainFrames[imageIndex].framebuffer;

	renderBeginInfo.renderArea.offset = { 0, 0 };
	renderBeginInfo.renderArea.extent = swapchainExtent;

	VkClearValue clearColor = {{{1.f, 0.f, 1.f, 1.f}}}; // magenta, not intended to be shown
	renderBeginInfo.clearValueCount = 1;
	renderBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // cmd: begin render pass

//	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, marchPipeline); // cmd: bind pipeline

	// set dynamic viewport and scissor
	if (DYNAMIC_VIEWPORT) {
		VkViewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = (float)swapchainExtent.width;
		viewport.height = (float)swapchainExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport); // cmd: set viewport size

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor); // cmd: set scissor
	}

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
		0, 1, &currentFlight.descriptorSet, 0, nullptr); // cmd: bind descriptor sets

	// push constants
	PushConstants push{};
	push.time = glfwGetTime();
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
		sizeof(PushConstants), &push); // cmd: set push constants

	// draw 3d scene
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, marchPipeline); // cmd: bind pipeline

	vkCmdDraw(commandBuffer, 3, 1, 0, 0); // cmd: draw

	// draw ui
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline); // cmd: bind pipeline

	VkBuffer buffers[] = {uiVertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

	vkCmdDraw(commandBuffer, uiVertices.size(), 1, 0, 0); // cmd: draw

	vkCmdEndRenderPass(commandBuffer); // cmd: end render

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer");
	}
}

void Primrose::updateUniforms(FrameInFlight& frame) {
	//Runtime::uniforms.numPrimitives = Scene::primitives.size();
	//Primitive* p = Scene::primitives.data();

	// write data
	writeToDevice(frame.uniformBufferMemory, &uniforms, sizeof(uniforms));
}

double frameTime;
double fenceTime;
bool Primrose::drawFrame() { // returns whether the frame was drawn
	double frS = glfwGetTime();

	if (windowMinimized) {
		// don't bother rendering if window minimized
		windowMinimized = isWindowMinimized();
		return false;
	}

	static int flightIndex = 0;
	auto& currentFlight = framesInFlight[flightIndex];

	double feS = glfwGetTime(); // includes time waiting for fences and vkAcquireNextImageKHR

	// don't reset the command buffer until the last frame is finished
	vkWaitForFences(device, 1, &currentFlight.inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, currentFlight.imageAvailableSemaphore,
		VK_NULL_HANDLE, &imageIndex); // signal imageAvailableSemaphore once the image is acquired

	fenceTime = glfwGetTime() - feS; // vkAcquireNextImageKHR is blocked by present mode such as refresh rate fps limit

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		// if swapchain is suboptimal it will be recreated later
		throw std::runtime_error("failed to acquire swap chain image");
	}

	updateUniforms(currentFlight);

	vkResetCommandBuffer(currentFlight.commandBuffer, 0);
	recordCommandBuffer(currentFlight.commandBuffer, imageIndex, currentFlight);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// once the gpu reaches the color attachment stage, wait until the image is actually available
	VkPipelineStageFlags flag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.pWaitDstStageMask = &flag;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &currentFlight.imageAvailableSemaphore;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &currentFlight.commandBuffer;

	submitInfo.signalSemaphoreCount = 1; // signal the semaphore once the render is done
	submitInfo.pSignalSemaphores = &currentFlight.renderFinishedSemaphore;

	// submits command to the graphics queue
	vkResetFences(device, 1, &currentFlight.inFlightFence); // reset fence for use below
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFlight.inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &currentFlight.renderFinishedSemaphore; // wait for the render to finish before presenting
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain; // swapchain we're presenting to
	presentInfo.pImageIndices = &imageIndex; // image we're presenting

	result = vkQueuePresentKHR(presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowResized) {
		recreateSwapchain();
		windowResized = false;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swapchain image");
	}

	// go to next frame in flight
	flightIndex += 1;
	flightIndex %= MAX_FRAMES_IN_FLIGHT; // loop after end of indexing

	frameTime = glfwGetTime() - frS;

	return true; // frame was drawn successfully
}

void Primrose::updateFps(bool init) {
	static double shortTime = glfwGetTime();
	static int shortFrames = 0;
	static int shortFps = 0;
	static double mediumTime = glfwGetTime();
	static int mediumFrames = 0;
	static int mediumFps = 0;
	static double longTime = glfwGetTime();
	static int longFrames = 0;
	static int longFps = 0;

	static double frameTimeTotal = 0.f;
	static int frameTimeAvgUs = 0.f; // avg microseconds for frame
	static double fenceTimeTotal = 0.f;
	static int fenceTimeAvgUs = 0.f; // avg microseconds waiting for fences
	static int sampleFrames = 0;
	static int cpuUtil = 0.f; // 5-digit(divide by 100) percent cpu usage

	if (init) return;

	frameTimeTotal += frameTime;
	fenceTimeTotal += fenceTime;
	++sampleFrames;

	++shortFrames;
	++mediumFrames;
	++longFrames;
	double cTime = glfwGetTime();
	bool update = false;
	if (cTime - shortTime > 1) {
		shortFps = shortFrames / (int)(cTime - shortTime);
		shortTime = cTime;
		shortFrames = 0;
		update = true;

		frameTimeAvgUs = (frameTimeTotal / (double)sampleFrames) * 1'000'000;
		fenceTimeAvgUs = (fenceTimeTotal / (double)sampleFrames) * 1'000'000;
		cpuUtil = (1.0 - fenceTimeTotal / frameTimeTotal) * 10000;
		sampleFrames = 0;
		frameTimeTotal = 0;
		fenceTimeTotal = 0;
	}
	if (cTime - mediumTime > 5) {
		mediumFps = mediumFrames / (int)(cTime - mediumTime);
		mediumTime = cTime;
		mediumFrames = 0;
		update = true;
	}
	if (cTime - longTime > 15) {
		longFps = longFrames / (int)(cTime - longTime);
		longTime = cTime;
		longFrames = 0;
		update = true;
	}

	if (update) {
		std::cout << "FPS(15): " << longFps;
		std::cout << " | FPS(5): " << mediumFps;
		std::cout << " | FPS(1): " << shortFps;
		//std::cout << " | Fra(1): " << frameTimeAvgUs << " us";
		//std::cout << " | Prs(1): " << fenceTimeAvgUs << " us";
		std::cout << " | CPU(1): " << (float)cpuUtil / 100.f << "%    ";
		// CPU over 90% indicates cpu bottleneck

		std::cout << std::flush << "\r";
	}
}
