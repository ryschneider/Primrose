#include "engine/runtime.hpp"
#include "state.hpp"

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

	endCallback();

	cleanup();
}

void Primrose::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex, FrameInFlight& currentFlight) {
	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.sType = vk::STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != vk::SUCCESS) {
		throw std::runtime_error("failed to begin recording cmd buffer");
	}

	vk::RenderPassBeginInfo renderBeginInfo{};
	renderBeginInfo.sType = vk::STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginInfo.renderPass = renderPass;
	renderBeginInfo.framebuffer = swapchainFrames[imageIndex].framebuffer;

	renderBeginInfo.renderArea.offset = { 0, 0 };
	renderBeginInfo.renderArea.extent = swapchainExtent;

	vk::ClearValue clearColor = {{{1.f, 0.f, 1.f, 1.f}}}; // magenta, not intended to be shown
	renderBeginInfo.clearValueCount = 1;
	renderBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderBeginInfo, vk::SUBPASS_CONTENTS_INLINE); // cmd: begin render pass

//	vkCmdBindPipeline(commandBuffer, vk::PIPELINE_BIND_POINT_GRAPHICS, marchPipeline); // cmd: bind pipeline

	// set dynamic viewport and scissor
	if (DYNAMIC_VIEWPORT) {
		vk::Viewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(swapchainExtent.width);
		viewport.height = static_cast<float>(swapchainExtent.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport); // cmd: set viewport size

		vk::Rect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor); // cmd: set scissor
	}

	vkCmdBindDescriptorSets(commandBuffer, vk::PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
		0, 1, &currentFlight.descriptorSet, 0, nullptr); // cmd: bind descriptor sets

	// push constants
	PushConstants push{};
	push.time = glfwGetTime();
	vkCmdPushConstants(commandBuffer, pipelineLayout, vk::SHADER_STAGE_FRAGMENT_BIT, 0,
		sizeof(PushConstants), &push); // cmd: set push constants

	// draw 3d scene
	vkCmdBindPipeline(commandBuffer, vk::PIPELINE_BIND_POINT_GRAPHICS, marchPipeline); // cmd: bind pipeline

	vkCmdDraw(commandBuffer, 3, 1, 0, 0); // cmd: draw

	// draw ui
	vkCmdBindPipeline(commandBuffer, vk::PIPELINE_BIND_POINT_GRAPHICS, uiPipeline); // cmd: bind pipeline

	for (const auto& element : uiScene) {
		if (element->hide) continue;
		if (element->pPush != nullptr) {
			// push custom struct
			vkCmdPushConstants(commandBuffer, pipelineLayout, vk::SHADER_STAGE_FRAGMENT_BIT, 0,
				element->pushSize, element->pPush);
		} else {
			// push ui nodeType
			vkCmdPushConstants(commandBuffer, pipelineLayout, vk::SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(element->uiType), &element->uiType);
		}

		if (element->descriptorSet != vk::NULL_HANDLE) {
			vkCmdBindDescriptorSets(commandBuffer, vk::PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, 1, &element->descriptorSet, 0, nullptr);
		}

		vk::DeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &(element->vertexBuffer), &offset);
		vkCmdBindIndexBuffer(commandBuffer, element->indexBuffer, 0, vk::INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(commandBuffer, element->numIndices, 1, 0, 0, 0);
	}

	renderPassCallback(commandBuffer);

	vkCmdEndRenderPass(commandBuffer); // cmd: end render

	if (vkEndCommandBuffer(commandBuffer) != vk::SUCCESS) {
		throw std::runtime_error("failed to record command buffer");
	}
}

void Primrose::updateUniforms(FrameInFlight& frame) {
	writeToDevice(frame.uniformBufferMemory, &uniforms, sizeof(uniforms));
}

void Primrose::drawFrame() {
	if (windowMinimized) {
		// don't bother rendering if window minimized
		windowMinimized = isWindowMinimized();
		return;
	}

	static int flightIndex = 0;
	auto& currentFlight = framesInFlight[flightIndex];

	// don't clear the command buffer until the last frame is finished
	vkWaitForFences(device, 1, &currentFlight.inFlightFence, vk::TRUE, UINT64_MAX);

	uint32_t imageIndex;
	vk::Result result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, currentFlight.imageAvailableSemaphore,
		vk::NULL_HANDLE, &imageIndex); // signal imageAvailableSemaphore once the image is acquired

	if (result == vk::ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return;
	}
	else if (result != vk::SUCCESS && result != vk::SUBOPTIMAL_KHR) {
		// if swapchain is suboptimal it will be recreated later
		throw std::runtime_error("failed to acquire swap chain image");
	}

	updateUniforms(currentFlight);

	vkResetCommandBuffer(currentFlight.commandBuffer, 0);
	recordCommandBuffer(currentFlight.commandBuffer, imageIndex, currentFlight);

	vk::SubmitInfo submitInfo{};
	submitInfo.sType = vk::STRUCTURE_TYPE_SUBMIT_INFO;

	// once the gpu reaches the color attachment stage, wait until the image is actually available
	vk::PipelineStageFlags flag = vk::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.pWaitDstStageMask = &flag;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &currentFlight.imageAvailableSemaphore;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &currentFlight.commandBuffer;

	submitInfo.signalSemaphoreCount = 1; // signal the semaphore once the render is done
	submitInfo.pSignalSemaphores = &currentFlight.renderFinishedSemaphore;

	// submits command to the graphics queue
	vkResetFences(device, 1, &currentFlight.inFlightFence); // clear fence for use below
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFlight.inFlightFence) != vk::SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer");
	}

	vk::PresentInfoKHR presentInfo{};
	presentInfo.sType = vk::STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &currentFlight.renderFinishedSemaphore; // wait for the render to finish before presenting
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain; // swapchain we're presenting to
	presentInfo.pImageIndices = &imageIndex; // image we're presenting

	result = vkQueuePresentKHR(presentQueue, &presentInfo);
	if (result == vk::ERROR_OUT_OF_DATE_KHR || result == vk::SUBOPTIMAL_KHR || windowResized) {
		recreateSwapchain();
		windowResized = false;
	}
	else if (result != vk::SUCCESS) {
		throw std::runtime_error("failed to present swapchain image");
	}

	// go to next frame in flight
	flightIndex += 1;
	flightIndex %= MAX_FRAMES_IN_FLIGHT; // loop after end of indexing
}
