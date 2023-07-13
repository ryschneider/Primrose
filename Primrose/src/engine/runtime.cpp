#include "engine/runtime.hpp"
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

void Primrose::run(void(*callback)(float)) {
	float lastTime = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;

		glfwPollEvents();
		callback(deltaTime);
		drawFrame();

		lastTime = currentTime;
	}

	vkDeviceWaitIdle(device);

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

	for (const auto& element : uiScene) {
		if (element->hide) continue;
		if (element->pPush != nullptr) {
			// push custom struct
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				element->pushSize, element->pPush);
		} else {
			// push ui nodeType
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(element->uiType), &element->uiType);
		}

		if (element->descriptorSet != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, 1, &element->descriptorSet, 0, nullptr);
		}

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &(element->vertexBuffer), &offset);
		vkCmdBindIndexBuffer(commandBuffer, element->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(commandBuffer, element->numIndices, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer); // cmd: end render

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer");
	}
}

void Primrose::updateUniforms(FrameInFlight& frame) {
	writeToDevice(frame.uniformBufferMemory, &uniforms, sizeof(uniforms));
}

void Primrose::drawFrame() { // returns whether the frame was drawn
	if (windowMinimized) {
		// don't bother rendering if window minimized
		windowMinimized = isWindowMinimized();
		return;
	}

	static int flightIndex = 0;
	auto& currentFlight = framesInFlight[flightIndex];

	// don't reset the command buffer until the last frame is finished
	vkWaitForFences(device, 1, &currentFlight.inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, currentFlight.imageAvailableSemaphore,
		VK_NULL_HANDLE, &imageIndex); // signal imageAvailableSemaphore once the image is acquired

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return;
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
}
