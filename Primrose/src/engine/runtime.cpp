#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "engine/runtime.hpp"
#include "state.hpp"
#include "log.hpp"

#include <GLFW/glfw3.h>

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
	log("Starting main loop");

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
	commandBuffer.begin(beginInfo);

	vk::RenderPassBeginInfo renderBeginInfo{};
	renderBeginInfo.renderPass = renderPass;
	renderBeginInfo.framebuffer = swapchainFrames[imageIndex].framebuffer;

	renderBeginInfo.renderArea.offset = vk::Offset2D(0, 0);
	renderBeginInfo.renderArea.extent = swapchainExtent;

//	vk::ClearValue clearColor = vk::ClearValue(vk::ClearColorValue(1.f, 0.f, 1.f, 1.f)); // magenta, should not be shown
//	renderBeginInfo.clearValueCount = 1;
//	renderBeginInfo.pClearValues = &clearColor;

	if (rayAcceleration) {
		// descriptor sets
		std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {
			vk::WriteDescriptorSet(VK_NULL_HANDLE, 0, 0, 1, vk::DescriptorType::eAccelerationStructureKHR),
			vk::WriteDescriptorSet(VK_NULL_HANDLE, 1, 0, 1, vk::DescriptorType::eStorageImage)
		};
		vk::WriteDescriptorSetAccelerationStructureKHR accWrite(1, &topStructure);
		descriptorWrites[0].pNext = &accWrite;

		vk::DescriptorImageInfo imgInfo = vk::DescriptorImageInfo(VK_NULL_HANDLE,
			traceImageView, vk::ImageLayout::eGeneral);
		descriptorWrites[1].pImageInfo = &imgInfo;

		commandBuffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR,
			mainPipelineLayout, 0, descriptorWrites);

		// push constants
		PushConstants push{};
		push.time = glfwGetTime();
		commandBuffer.pushConstants(mainPipelineLayout, vk::ShaderStageFlagBits::eRaygenKHR, 0,
			sizeof(PushConstants), &push); // cmd: set push constants

		// bind pipeline
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, mainPipeline);

		// draw
		commandBuffer.traceRaysKHR(genGroupAddress, missGroupAddress, hitGroupAddress, callableGroupAddress,
			swapchainExtent.width, swapchainExtent.height, 1);

		// transition images to prepare for copy
		transitionImageLayout(swapchainFrames[imageIndex].image, commandBuffer, // swap image: undefined -> transfer dst
			vk::ImageLayout::eUndefined, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe,
			vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer);
		transitionImageLayout(traceImage, commandBuffer, // traceImage: general -> transfer src
			vk::ImageLayout::eGeneral, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer);

		// copy image from traceImage to swap image
		vk::ImageCopy imgCopy(
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0), // src format
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0), // dst format
			vk::Extent3D(swapchainExtent, 1)); // image size
		commandBuffer.copyImage(traceImage, vk::ImageLayout::eTransferSrcOptimal,
			swapchainFrames[imageIndex].image, vk::ImageLayout::eTransferDstOptimal, imgCopy);

		// transition images back to normal
		transitionImageLayout(swapchainFrames[imageIndex].image, commandBuffer, // swap image: transfer dst -> present src
			vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
			vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eBottomOfPipe);
		transitionImageLayout(traceImage, commandBuffer, // traceImage: transfer src -> general
			vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer,
			vk::ImageLayout::eGeneral, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eBottomOfPipe);

		// begin render pass
		commandBuffer.beginRenderPass(renderBeginInfo, vk::SubpassContents::eInline); // cmd: begin render pass
	} else {
		// begin render pass
		commandBuffer.beginRenderPass(renderBeginInfo, vk::SubpassContents::eInline); // cmd: begin render pass

		// set dynamic viewport and scissor
		vk::Viewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(swapchainExtent.width);
		viewport.height = static_cast<float>(swapchainExtent.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		commandBuffer.setViewport(0, 1, &viewport); // cmd: set viewport size

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D(0, 0);
		scissor.extent = swapchainExtent;
		commandBuffer.setScissor(0, 1, &scissor);

		// uniforms
		std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {
			vk::WriteDescriptorSet(VK_NULL_HANDLE, 0, 0, 1, vk::DescriptorType::eUniformBuffer),
			vk::WriteDescriptorSet(VK_NULL_HANDLE, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler)
		};
		vk::DescriptorBufferInfo ubInfo = vk::DescriptorBufferInfo(currentFlight.uniformBuffer, 0, sizeof(MarchUniforms));
		descriptorWrites[0].pBufferInfo = &ubInfo;
		vk::DescriptorImageInfo imgInfo = vk::DescriptorImageInfo(marchSampler, marchImageView,
			vk::ImageLayout::eShaderReadOnlyOptimal);
		descriptorWrites[1].pImageInfo = &imgInfo;

		commandBuffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, mainPipelineLayout, 0, descriptorWrites);

		// push constants
		PushConstants push{};
		push.time = glfwGetTime();
		commandBuffer.pushConstants(mainPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
			sizeof(PushConstants), &push); // cmd: set push constants

		// draw 3d scene
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline); // cmd: bind pipeline
		commandBuffer.draw(3, 1, 0, 0); // cmd: draw
	}

//	// draw ui
//	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, uiPipeline); // cmd: bind pipeline
//
//	for (const auto& element : uiScene) {
//		if (element->hide) continue;
//		if (element->pPush != nullptr) {
//			// push custom struct
//			commandBuffer.pushConstants(uiPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
//				element->pushSize, element->pPush);
//		} else {
//			// push ui nodeType
//			commandBuffer.pushConstants(uiPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
//				sizeof(element->uiType), &element->uiType);
//		}
//
//		if (VkDescriptorSet(element->descriptorSet) != VK_NULL_HANDLE) {
//			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, uiPipelineLayout,
//				0, 1, &element->descriptorSet, 0, nullptr);
//		}
//
//		vk::DeviceSize offset = 0;
//		commandBuffer.bindVertexBuffers(0, 1, &(element->vertexBuffer), &offset);
//		commandBuffer.bindIndexBuffer(element->indexBuffer, 0, vk::IndexType::eUint16);
//
//		commandBuffer.drawIndexed(element->numIndices, 1, 0, 0, 0);
//	}

	renderPassCallback(commandBuffer);

	commandBuffer.endRenderPass(); // cmd: end render

	commandBuffer.end();
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
	if (device.waitForFences(1, &currentFlight.inFlightFence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("failed to wait for fences");
	}

	auto res = device.acquireNextImageKHR(swapchain, UINT64_MAX,
		currentFlight.imageAvailableSemaphore, VK_NULL_HANDLE);

	if (res.result == vk::Result::eErrorOutOfDateKHR) {
		recreateSwapchain();
		return;
	} else if (res.result != vk::Result::eSuccess && res.result != vk::Result::eSuboptimalKHR) {
		// if swapchain is suboptimal it will be recreated later
		error("Failed to acquire swap chain image");
	}
	uint32_t imageIndex = res.value;

	updateUniforms(currentFlight);

	vkResetCommandBuffer(currentFlight.commandBuffer, 0);
	recordCommandBuffer(currentFlight.commandBuffer, imageIndex, currentFlight);

	vk::SubmitInfo submitInfo{};

	// once the gpu reaches the color attachment stage, wait until the image is actually available
	vk::PipelineStageFlags flag = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submitInfo.pWaitDstStageMask = &flag;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &currentFlight.imageAvailableSemaphore;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &currentFlight.commandBuffer;

	submitInfo.signalSemaphoreCount = 1; // signal the semaphore once the render is done
	submitInfo.pSignalSemaphores = &currentFlight.renderFinishedSemaphore;

	// submits command to the graphics queue
	device.resetFences({currentFlight.inFlightFence}); // clear fence for use below
	graphicsQueue.submit({submitInfo}, currentFlight.inFlightFence);

	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &currentFlight.renderFinishedSemaphore; // wait for the render to finish before presenting
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain; // swapchain we're presenting to
	presentInfo.pImageIndices = &imageIndex; // image we're presenting


	vk::Result result = presentQueue.presentKHR(&presentInfo);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || windowResized) {
		recreateSwapchain();
		windowResized = false;
	} else if (result != vk::Result::eSuccess) {
		error("Failed to present swapchain image");
	}

	// go to next frame in flight
	flightIndex += 1;
	flightIndex %= MAX_FRAMES_IN_FLIGHT; // loop after end of indexing
}
