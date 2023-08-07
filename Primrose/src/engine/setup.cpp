#include "engine/setup.hpp"
#include "state.hpp"
#include "engine/runtime.hpp"
#include "embed/flat_vert_spv.h"
#include "embed/march_frag_spv.h"
#include "embed/ui_vert_spv.h"
#include "embed/ui_frag_spv.h"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <cstring>

namespace Primrose {
	vk::Instance instance; // used as global vulkan state
	vk::PhysicalDevice physicalDevice; // graphics device
	vk::Device device; // logical connection to the physical device

	vk::RenderPass renderPass; // render pass with commands used to render a frame
	vk::DescriptorSetLayout descriptorSetLayout; // layout for shader uniforms
	vk::PipelineLayout pipelineLayout; // graphics pipeline layout

	vk::Pipeline marchPipeline; // graphics pipeline
	vk::Pipeline uiPipeline; // graphics pipeline

	GLFWwindow* window;
	vk::SurfaceKHR surface;

	vk::SwapchainKHR swapchain; // chain of images queued to be presented to display
	std::vector<SwapchainFrame> swapchainFrames;
	vk::Format swapchainImageFormat; // pixel format used in swapchain
	vk::Extent2D swapchainExtent; // resolution of swapchain

	int windowWidth = 0;
	int windowHeight = 0;

	vk::DescriptorPool descriptorPool;

	vk::Image marchTexture;
	vk::DeviceMemory marchTextureMemory;
	vk::ImageView marchImageView;
	vk::Sampler marchSampler;

	std::vector<FrameInFlight> framesInFlight;

	vk::CommandPool commandPool;
	vk::Queue graphicsQueue; // queue for graphics commands sent to gpu
	vk::Queue presentQueue; // queue for present commands sent to gpu

	vk::DebugUtilsMessengerEXT debugMessenger; // handles logging validation details

	void(*mouseMovementCallback)(float xpos, float ypos, bool refocused) = nullptr;
	void(*mouseButtonCallback)(int button, int action) = nullptr;
	void(*keyCallback)(int key, int action, int mods) = nullptr;
	void(*endCallback)() = nullptr;
	void(*renderPassCallback)(vk::CommandBuffer& cmd) = nullptr;
	void(*scrollCallback)(float scroll) = nullptr;

	// glfw callbacks
	namespace {
		void windowResizedCallback(GLFWwindow*, int, int) {
			windowResized = true;
		}
		void windowFocusGlfwCallback(GLFWwindow*, int focused) {
			if (focused != GL_TRUE) {
				windowFocused = false;
			}
		}
		void cursorPosGlfwCallback(GLFWwindow*, double xpos, double ypos) {
			bool refocused = !windowFocused && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GL_TRUE;
			if (refocused) windowFocused = true;
			if (mouseMovementCallback != nullptr) mouseMovementCallback(xpos, ypos, refocused);
		}
		void keyGlfwCallback(GLFWwindow*, int key, int, int action, int mods) {
			if (key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, GL_TRUE);
			}
			if (keyCallback != nullptr && action != GLFW_REPEAT) {
				keyCallback(key, action, mods);
			}
		}
		void mouseButtonGlfwCallback(GLFWwindow*, int button, int action, int mods) {
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				setZoom(1.f);
			}

			if (mouseButtonCallback != nullptr) mouseButtonCallback(button, action);
		}
		void scrollGlfwCallback(GLFWwindow*, double, double scroll) {
			if (scrollCallback != nullptr) scrollCallback(scroll);
		}
	}

	// vulkan callbacks
	namespace {
		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
			const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {

			if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning // if warning or worse
				|| type == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) { // or non-optimal use of vulkan

				std::cerr << pCallbackData->pMessage << std::endl << std::endl;
			}

			return VK_FALSE; // whether to abort the vulkan call which triggered the callback
		}
	}

	// helper funcs
	namespace {
		std::vector<const char*> getRequiredExtensions() {
			uint32_t glfwCount = 0;
			const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);

			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);

			if (Settings::validationEnabled) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}

			return extensions;
		}

		int getPhysicalDeviceVramMb(vk::PhysicalDevice phyDevice) {
			vk::PhysicalDeviceMemoryProperties memoryProperties = phyDevice.getMemoryProperties();

			int vram = 0;
			for (unsigned int i = 0; i < memoryProperties.memoryHeapCount; ++i) {
				const auto& heap = memoryProperties.memoryHeaps + i;

				if (heap->flags & vk::MemoryHeapFlagBits::eDeviceLocal) { // if memory is local to gpu
					vram += static_cast<int>(heap->size / (1024 * 1024));
				}
			}
			return vram;
		}

		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
		};
		QueueFamilyIndices getQueueFamilies(vk::PhysicalDevice phyDevice) {
//			uint32_t queueCount = 0;
//			vk::GetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, nullptr);
//			std::vector<vk::QueueFamilyProperties> queueFamilies(queueCount);
//			vk::GetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, queueFamilies.data());

			auto queueFamilies = phyDevice.getQueueFamilyProperties();

			QueueFamilyIndices indices;

			int i = 0;
			for (const auto& family : queueFamilies) {
				bool graphicsSupport = static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
				bool presentSupport = phyDevice.getSurfaceSupportKHR(i, surface);

				if (graphicsSupport && presentSupport) { // will overwrite existing indices if both found in one family
					indices.graphicsFamily = i;
					indices.presentFamily = i;
					break;
				} else if (graphicsSupport && (!indices.graphicsFamily.has_value())) {
					indices.graphicsFamily = i;
				} else if (presentSupport && (!indices.presentFamily.has_value())) {
					indices.presentFamily = i;
				}

				++i;
			}

			return indices;
		}

		struct SwapchainDetails {
			vk::SurfaceCapabilitiesKHR capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR> presentModes;
		};
		SwapchainDetails getSwapchainDetails(vk::PhysicalDevice phyDevice) {
			SwapchainDetails details;

			details.capabilities = phyDevice.getSurfaceCapabilitiesKHR(surface);
			details.formats = phyDevice.getSurfaceFormatsKHR(surface);
			details.presentModes = phyDevice.getSurfacePresentModesKHR(surface);

			return details;
		}

		bool isPhysicalDeviceSuitable(vk::PhysicalDevice phyDevice) {
			// check for correct queue families
			QueueFamilyIndices indices = getQueueFamilies(phyDevice);
			if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) return false;

			// check if extensions are supported
			uint32_t extensionCount;
			vk::EnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, nullptr);
			std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
			vk::EnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, availableExtensions.data());

			for (const auto& required : REQUIRED_EXTENSIONS) {
				if (std::find_if(availableExtensions.begin(), availableExtensions.end(),
					[required](vk::ExtensionProperties& available) {
					return strcmp(required, available.extensionName) == 0;
				}) == availableExtensions.end()) {
					return false; // required extension not supported
				}
			}

			// make sure swapchain is adequate
			auto formats = phyDevice.getSurfaceFormatsKHR(surface);
			auto presentModes = phyDevice.getSurfacePresentModesKHR(surface);
			if (formats.empty() || presentModes.empty()) return false;

			// make sure anisotrophy is supported
			// TODO support devices without anisotrophy by forcing disabled in settings
			vk::PhysicalDeviceFeatures supportedFeatures;
			vk::GetPhysicalDeviceFeatures(phyDevice, &supportedFeatures);
			if (supportedFeatures.samplerAnisotropy == VK_FALSE) return false;

			// device is suitable for app
			return true;
		}
	}
}

void Primrose::allocateDescriptorSet(vk::DescriptorSet* descSet) {
	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = vk::_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	if (vk::AllocateDescriptorSets(device, &allocInfo, descSet) != vk::_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set :)");
	}
}

void Primrose::createDeviceMemory(vk::MemoryRequirements memReqs,
	vk::MemoryPropertyFlags properties, vk::DeviceMemory* memory) {

	vk::PhysicalDeviceMemoryProperties memProperties;
	vk::GetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	uint32_t bestMemory;
	bool found = false;
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		// which of chosen [properties] are supported
		vk::MemoryPropertyFlags supportedProperties = memProperties.memoryTypes[i].propertyFlags & properties;

		if (memReqs.memoryTypeBits & (1 << i) // check if memory is suitable according to memReqs
			&& supportedProperties == properties) { // and all properties are supported
			bestMemory = i;
			found = true;
			break;
		}
	}
	if (!found) {
		throw std::runtime_error("failed to find suitable memory nodeType");
	}

	uint32_t heapIndex = memProperties.memoryTypes[bestMemory].heapIndex;

	vk::MemoryAllocateInfo allocInfo{};
	allocInfo.sType = vk::_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = bestMemory;
//	std::cout << "Creating " << memReqs.size << " byte buffer on heap " << heapIndex << " (";
//	std::cout << memProperties.memoryHeaps[heapIndex].size / (1024 * 1024) << " mb)" << std::endl;

	if (vk::AllocateMemory(device, &allocInfo, nullptr, memory) != vk::_SUCCESS) {
		throw std::runtime_error("failed to allocate uniform buffer memory");
	}
}
void Primrose::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
	vk::MemoryPropertyFlags properties, vk::Buffer* buffer, vk::DeviceMemory* bufferMemory) {

	// create buffer
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.sType = vk::_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::_SHARING_MODE_EXCLUSIVE;

	if (vk::CreateBuffer(device, &bufferInfo, nullptr, buffer) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create buffer");
	}

	vk::MemoryRequirements memReqs;
	vk::GetBufferMemoryRequirements(device, *buffer, &memReqs);

	createDeviceMemory(memReqs, properties, bufferMemory);

	vk::BindBufferMemory(device, *buffer, *bufferMemory, 0); // bind memory to buffer
}

void Primrose::writeToDevice(vk::DeviceMemory memory, void* data, size_t size, size_t offset) {
	void* dst;
	vk::MapMemory(device, memory, offset, size, 0, &dst);
	memcpy(dst, data, size);
	vk::UnmapMemory(device, memory);
}

vk::CommandBuffer Primrose::startSingleTimeCommandBuffer() {
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = vk::_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = vk::_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	vk::CommandBuffer cmdBuffer;
	vk::AllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.sType = vk::_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = vk::_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vk::BeginCommandBuffer(cmdBuffer, &beginInfo);

	return cmdBuffer;
}
void Primrose::endSingleTimeCommandBuffer(vk::CommandBuffer cmdBuffer) {
	if (vk::EndCommandBuffer(cmdBuffer) != vk::_SUCCESS) {
		throw std::runtime_error("failed to record copy command buffer");
	}

	vk::SubmitInfo submitInfo{};
	submitInfo.sType = vk::_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vk::QueueSubmit(graphicsQueue, 1, &submitInfo, vk::_NULL_HANDLE);
	vk::QueueWaitIdle(graphicsQueue);
	// note - if copying multiple buffers, could submit them all then wait for every fence to complete at the end

	vk::FreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
}

void Primrose::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	vk::ImageMemoryBarrier barrier{};
	barrier.sType = vk::_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = vk::_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = vk::_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = vk::_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (oldLayout == vk::_IMAGE_LAYOUT_UNDEFINED && newLayout == vk::_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = vk::_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = vk::_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = vk::_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == vk::_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == vk::_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = vk::_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = vk::_ACCESS_SHADER_READ_BIT;

		srcStage = vk::_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = vk::_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	vk::CmdPipelineBarrier(cmdBuffer,
		srcStage, dstStage, 0, 0, nullptr,
		0, nullptr, 1, &barrier);
	endSingleTimeCommandBuffer(cmdBuffer);
}
void Primrose::importTexture(const char* path, vk::Image* image, vk::DeviceMemory* imageMemory, float* aspect) {
	// generateUniforms image data
	int width, height, channels;
	std::cout << "Importing " << path << std::endl;
	stbi_uc* data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
	if (stbi_failure_reason()) {
		throw std::runtime_error(std::string("stbi_uc error: ").append(stbi_failure_reason()));
	}

	*aspect = static_cast<float>(width) / static_cast<float>(height);

	imageFromData(data, width, height, image, imageMemory);

	// free image data
	stbi_image_free(data);
}

void Primrose::imageFromData(void* data, uint32_t width, uint32_t height, vk::Image* image, vk::DeviceMemory* imageMemory) {

	vk::DeviceSize dataSize = width * height * 4;

	// create and write to staging buffer
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(dataSize, vk::_BUFFER_USAGE_TRANSFER_SRC_BIT,
		vk::_MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);
	writeToDevice(stagingBufferMemory, data, dataSize);

	// create image handler
	vk::ImageCreateInfo imageInfo{};
	imageInfo.sType = vk::_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = vk::_IMAGE_TYPE_2D;
	imageInfo.extent = {width, height, 1}; // depth=1 for 2d image
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = vk::_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = vk::_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = vk::_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = vk::_IMAGE_USAGE_TRANSFER_DST_BIT | vk::_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = vk::_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = vk::_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vk::CreateImage(device, &imageInfo, nullptr, image) != vk::_SUCCESS) {
		throw std::runtime_error(std::string("failed to create image"));
	}

	// create & bind image memory
	vk::MemoryRequirements memReqs;
	vk::GetImageMemoryRequirements(device, *image, &memReqs);
	createDeviceMemory(memReqs, vk::_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageMemory);
	vk::BindImageMemory(device, *image, *imageMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(*image, vk::_FORMAT_R8G8B8A8_SRGB,
		vk::_IMAGE_LAYOUT_UNDEFINED, vk::_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy data to image
	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	vk::BufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = vk::_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};
	vk::CmdCopyBufferToImage(cmdBuffer,
		stagingBuffer, *image, vk::_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(*image, vk::_FORMAT_R8G8B8A8_SRGB,
		vk::_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// cleanup staging buffer
	vk::DestroyBuffer(device, stagingBuffer, nullptr);
	vk::FreeMemory(device, stagingBufferMemory, nullptr);
}

vk::ImageView Primrose::createImageView(vk::Image image, vk::Format format) {
	vk::ImageViewCreateInfo viewInfo{};
	viewInfo.sType = vk::_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = vk::_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = vk::_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vk::ImageView imageView;
	if (vk::CreateImageView(device, &viewInfo, nullptr, &imageView) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create ui texture image view");
	}
	return imageView;
}

vk::ShaderModule Primrose::createShaderModule(const uint32_t* code, size_t length) {
	vk::ShaderModuleCreateInfo info{};
	info.sType = vk::_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = length;
	info.pCode = code;

	vk::ShaderModule shader;
	if (vk::CreateShaderModule(device, &info, nullptr, &shader) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create shader module");
	}

	return shader;
}



void Primrose::setup(const char* applicationName, unsigned int applicationVersion) {
	Primrose::appName = applicationName;
	Primrose::appVersion = applicationVersion;

	initWindow();
	initVulkan();

	// TODO put screenHeight definition in swapchainExtent setter
	setFov(Settings::fov); // initial set fov
	setZoom(1.f); // initial set zoom
	uniforms.screenHeight = (float)swapchainExtent.height / (float)swapchainExtent.width;
	//Runtime::uniforms.primitives = Scene::primitives.data();
}

void Primrose::initWindow() {
	std::cout << "Initialising glfw" << std::endl << std::endl;

	if (glfwInit() != GLFW_TRUE) {
		throw std::runtime_error("failed to initialise glfw");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(Settings::windowWidth, Settings::windowHeight, Settings::windowTitle, nullptr, nullptr);
	if (window == nullptr) {
		throw std::runtime_error("failed to create window");
	}

	{
		int count;
		GLFWmonitor* monitor = glfwGetMonitors(&count)[0];
		const GLFWvidmode* mSize = glfwGetVideoMode(monitor);
		int mx, my;
		glfwGetMonitorPos(monitor, &mx, &my);

		glfwSetWindowPos(window,
			mx + (mSize->width - Settings::windowWidth)/2,
			my + (mSize->height - Settings::windowHeight)/2);
	}

	glfwSetFramebufferSizeCallback(window, windowResizedCallback);
	glfwSetWindowFocusCallback(window, windowFocusGlfwCallback);
	glfwSetKeyCallback(window, keyGlfwCallback);
	glfwSetCursorPosCallback(window, cursorPosGlfwCallback);
	glfwSetMouseButtonCallback(window, mouseButtonGlfwCallback);
	glfwSetScrollCallback(window, scrollGlfwCallback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

bool Primrose::isWindowMinimized() {
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) {
		return true;
	}
	else {
		return false;
	}
}



void Primrose::initVulkan() {
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();

	createSwapchain();
	createRenderPass();
	createSwapchainFrames();
	createDescriptorSetLayout();
	createPipelineLayout();

	createMarchPipeline();
	createUIPipeline();

	createCommandPool();
	createDescriptorPool();

	createMarchTexture();

	createFramesInFlight();
}



void Primrose::setupDebugMessenger() {
	if (!Settings::validationEnabled) return;

	vk::DebugUtilsMessengerCreateInfoEXT info{};
	info.sType = vk::_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity = vk::_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = vk::_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = debugCallback;

	auto vk::CreateDebugUtilsMessengerEXT = (PFN_vk::CreateDebugUtilsMessengerEXT)vk::GetInstanceProcAddr(instance, "vk::CreateDebugUtilsMessengerEXT");
	if (vk::CreateDebugUtilsMessengerEXT == nullptr) {
		throw std::runtime_error("could not find vk::CreateDebugUtilsMessengerEXT");
	}

	if (vk::CreateDebugUtilsMessengerEXT(instance, &info, nullptr, &debugMessenger) != vk::_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger");
	}
}

void Primrose::createInstance() {
	if (Settings::validationEnabled) {
		// check for validation layers
		uint32_t layerCount = 0;
		vk::EnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<vk::LayerProperties> availableLayers(layerCount);
		vk::EnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		bool allSupported = true;
		for (const char* reqLayer : VALIDATION_LAYERS) {
			bool found = false;

			for (const auto& avLayer : availableLayers) {
				if (strcmp(reqLayer, avLayer.layerName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				allSupported = false;
				break;
			}
		}

		if (!allSupported) {
			throw std::runtime_error("validation layers not available");
		}
	}

	// info in case hardware vendors have optimizations for specific applications/engines
	vk::ApplicationInfo appInfo{};
	appInfo.sType = vk::_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = appVersion;
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.engineVersion = ENGINE_VERSION;
	appInfo.apiVersion = vk::_API_VERSION_1_2;

	vk::InstanceCreateInfo createInfo{};
	createInfo.sType = vk::_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// vk extensions required for glfw
	std::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	// enable validation layers in instance
	vk::DebugUtilsMessengerCreateInfoEXT debugInfo{};
	if (Settings::validationEnabled) {
		createInfo.enabledLayerCount = (uint32_t)VALIDATION_LAYERS.size();
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

		// enables debugger during instance creation and destruction
		debugInfo.sType = vk::_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugInfo.messageSeverity = vk::_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugInfo.messageType = vk::_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | vk::_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = &debugInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	// create instance
	if (vk::CreateInstance(&createInfo, nullptr, &instance) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create instance");
	}
}

void Primrose::createSurface() {
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
}



void Primrose::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vk::EnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("no GPU with vulkan support");
	}

	std::vector<vk::PhysicalDevice> devices(deviceCount);
	vk::EnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	int bestScore = -1;
	vk::PhysicalDevice bestDevice = vk::_NULL_HANDLE;
	for (const auto& phyDevice : devices) {

		if (!isPhysicalDeviceSuitable(phyDevice)) {
			continue;
		}

		// score graphics devices based on nodeType and specs
		int score = 0;

		// priority for dedicated graphics cards - arbitrary large number for priority
		vk::PhysicalDeviceProperties properties;
		vk::GetPhysicalDeviceProperties(phyDevice, &properties);
		if (properties.deviceType == vk::_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1 << 30; // 2^30
		}
		else if (properties.deviceType == vk::_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
			score += 1 << 29; // 2^29
		}

		// score multiple graphics cards of same nodeType by vram
		score += getPhysicalDeviceVramMb(phyDevice);

		if (score > bestScore) {
			bestScore = score;
			bestDevice = phyDevice;
		}
	}

	if (bestDevice == vk::_NULL_HANDLE) {
		throw std::runtime_error("no suitable graphics device could be found");
	}

	physicalDevice = bestDevice;
	std::cout << "Choosing graphics device with " << getPhysicalDeviceVramMb(physicalDevice) << " mb VRAM" << std::endl;

	std::cout << std::endl; // newline to seperate prints
}

void Primrose::createLogicalDevice() {
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	std::vector<vk::DeviceQueueCreateInfo> queueInfos;
	std::set<uint32_t> uniqueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float priority = 1;
	for (uint32_t family : uniqueFamilies) {
		vk::DeviceQueueCreateInfo info{};
		info.sType = vk::_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = family;
		info.queueCount = 1;
		info.pQueuePriorities = &priority;
		queueInfos.push_back(info);
	}

	vk::PhysicalDeviceFeatures features{};
	features.samplerAnisotropy = vk::_TRUE;

	vk::DeviceCreateInfo createInfo{};
	createInfo.sType = vk::_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()); // create queues
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_EXTENSIONS.size()); // enable extensions
	createInfo.ppEnabledExtensionNames = REQUIRED_EXTENSIONS.data();
	createInfo.pEnabledFeatures = &features;

	if (vk::CreateDevice(physicalDevice, &createInfo, nullptr, &device) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create logical device");
	}

	vk::GetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vk::GetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void Primrose::createSwapchain() {
	SwapchainDetails swapDetails = getSwapchainDetails(physicalDevice);
	auto swapCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	auto swapCormats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto swapCresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	// choose surface format
	bool ideal = false;
	vk::SurfaceFormatKHR surfaceFormat = swapDetails.formats[0]; // default if ideal not found
	for (const auto& format : swapDetails.formats) {
		if (format.format == IDEAL_SURFACE_FORMAT.format && format.colorSpace == IDEAL_SURFACE_FORMAT.colorSpace) {
			surfaceFormat = format;
			ideal = true;
			break;
		}
	}
	std::cout << "Swapchain format: " << surfaceFormat.format << ", " << surfaceFormat.colorSpace;
	std::cout << (ideal ? " (ideal)" : " (not ideal)") << std::endl;
	swapchainImageFormat = surfaceFormat.format; // save format to global var

	// choose presentation mode
	ideal = false;
	vk::PresentModeKHR presentMode = vk::_PRESENT_MODE_FIFO_KHR;
	for (const auto& availableMode : swapDetails.presentModes) {
		if (availableMode == IDEAL_PRESENT_MODE) {
			presentMode = availableMode;
			ideal = true;
			break;
		}
	}
	std::cout << "Swapchain present mode: " << PRESENT_MODE_STRINGS.find(presentMode)->second << (ideal ? " (ideal)" : " (not ideal)") << std::endl;

	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	// choose swap extent (resolution of images)
	if (swapDetails.capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
		// if currentExtent.width is max value, use the size of the window as extent

		// keep within min and max image extent allowed by swapchain
		swapchainExtent.width = std::clamp((uint32_t)windowWidth,
			swapDetails.capabilities.minImageExtent.width, swapDetails.capabilities.maxImageExtent.width);
		swapchainExtent.height = std::clamp((uint32_t)windowHeight,
			swapDetails.capabilities.minImageExtent.height, swapDetails.capabilities.maxImageExtent.height);
	} else {
		swapchainExtent = swapDetails.capabilities.currentExtent; // use extent given
	}
	std::cout << "Swapchain extent: (" << swapchainExtent.width << ", " << swapchainExtent.height << ")" << std::endl;

	// number of images held by swapchain
	uint32_t imageCount = swapDetails.capabilities.minImageCount + 1; // use 1 more than minimum
	if (imageCount > swapDetails.capabilities.maxImageCount // up to maximum
		&& swapDetails.capabilities.maxImageCount > 0) { // 0 means unlimited maximum
		imageCount = swapDetails.capabilities.maxImageCount;
	}

	// create info
	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.sType = vk::_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount; // only the min number images, could be more
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapchainExtent;
	createInfo.imageArrayLayers = 1; // layers per image
	createInfo.imageUsage = vk::_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // what we're using the images for, ie rendering to directly
	createInfo.presentMode = presentMode;

	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);
	uint32_t familyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily == indices.presentFamily) {
		// exclusive, ie each image is only owned by one queue family
		createInfo.imageSharingMode = vk::_SHARING_MODE_EXCLUSIVE;
	}
	else {
		// concurrent, ie images can be used between multiple queue families
		// TODO - make more efficient by explicitely transferring images between families when needed
		createInfo.imageSharingMode = vk::_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = familyIndices;
	}

	createInfo.preTransform = swapDetails.capabilities.currentTransform; // no special transformations
	createInfo.compositeAlpha = vk::_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // alpha bit is ignored by compositor
	createInfo.clipped = vk::_TRUE; // cull pixels that are covered by other windows

	createInfo.oldSwapchain = vk::_NULL_HANDLE; // handle for last swapchain

	if (vk::CreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create swapchain");
	}

	// print num of images (device could have chosen different number than imageCount)
	vk::GetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	std::cout << "Swapchain images: " << imageCount;
	std::cout << " (" << swapDetails.capabilities.minImageCount << " to ";
	std::cout << swapDetails.capabilities.maxImageCount << ")" << std::endl;

	std::cout << std::endl; // newline to seperate prints
}

void Primrose::createRenderPass() {
	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainImageFormat;
	colorAttachment.samples = vk::_SAMPLE_COUNT_1_BIT; // multisampling count

	colorAttachment.loadOp = vk::_ATTACHMENT_LOAD_OP_CLEAR; // what to do with the framebuffer data before rendering
	colorAttachment.storeOp = vk::_ATTACHMENT_STORE_OP_STORE; // what to do with the framebuffer data after rendering

	colorAttachment.stencilLoadOp = vk::_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = vk::_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = vk::_IMAGE_LAYOUT_UNDEFINED; // dont care what layout it has before rendering
	colorAttachment.finalLayout = vk::_IMAGE_LAYOUT_PRESENT_SRC_KHR; // want the image to be presentable after rendering

	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::_PIPELINE_BIND_POINT_GRAPHICS; // graphics subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; // the outputs of the frag shader map to the buffers in this array
	// eg layout(location = 0) refers to pColorAttachments[0]

	vk::SubpassDependency dependency{};
	dependency.srcSubpass = vk::_SUBPASS_EXTERNAL; // refers to first implicit subpass
	dependency.srcStageMask = vk::_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // all the commands in this subpass must reach the color attachment stage before the destination subpass can begin
	dependency.srcAccessMask = 0;

	dependency.dstSubpass = 0; // refers to our subpass
	dependency.dstStageMask = vk::_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // commands in our subpass cannot enter this stage until all the commands from the external subpass have reached this stage
	dependency.dstAccessMask = vk::_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // specifically don't allow writing the colors

	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = vk::_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vk::CreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create render pass");
	}
}

void Primrose::createSwapchainFrames() {
	// fetch images from swapchain
	uint32_t imageCount;
	vk::GetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	std::vector<vk::Image> images(imageCount);
	vk::GetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

	// framebuffer settings
	vk::FramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = vk::_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.width = swapchainExtent.width;
	framebufferInfo.height = swapchainExtent.height;
	framebufferInfo.layers = 1;

	swapchainFrames.resize(imageCount);
	for (size_t i = 0; i < swapchainFrames.size(); ++i) {
		// image
		swapchainFrames[i].image = images[i];

		// image view
		swapchainFrames[i].imageView = createImageView(swapchainFrames[i].image, swapchainImageFormat);

		// framebuffer
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchainFrames[i].imageView;
		if (vk::CreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFrames[i].framebuffer) != vk::_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer");
		}
	}
}



void Primrose::createDescriptorSetLayout() {
	// uniform binding
	vk::DescriptorSetLayoutBinding uniformBinding{};
	uniformBinding.binding = 0; // binding in shader
	uniformBinding.descriptorType = vk::_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // uniform buffer
	uniformBinding.descriptorCount = 1; // numbers of ubos
	uniformBinding.stageFlags = vk::_SHADER_STAGE_FRAGMENT_BIT; // shader stage

	// texture binding
	vk::DescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 1; // binding in shader
	textureBinding.descriptorType = vk::_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // uniform buffer
	textureBinding.descriptorCount = 1; // numbers of textures
	textureBinding.pImmutableSamplers = nullptr;
	textureBinding.stageFlags = vk::_SHADER_STAGE_FRAGMENT_BIT; // shader stage

	vk::DescriptorSetLayoutBinding bindings[2] = {uniformBinding, textureBinding};

	// extra flags for descriptor set layout
	vk::DescriptorBindingFlags flags = vk::_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.sType = vk::_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pBindingFlags = &flags;

	// create descriptor set layout
	vk::DescriptorSetLayoutCreateInfo info{};
	info.sType = vk::_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 2;
	info.pBindings = bindings;
	info.pNext = &bindingFlags;

	if (vk::CreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}
}

void Primrose::createPipelineLayout() {
	vk::PushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = 128; // min required size
	pushConstant.stageFlags = vk::_SHADER_STAGE_FRAGMENT_BIT;

	vk::PipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	layoutInfo.pPushConstantRanges = &pushConstant;
	layoutInfo.pushConstantRangeCount = 1;

	if (vk::CreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}
}

vk::Pipeline Primrose::createPipeline(
	vk::ShaderModule vertModule, vk::ShaderModule fragModule,
	vk::PipelineVertexInputStateCreateInfo vertInputInfo,
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo) {

	vk::PipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = vk::_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.pName = "main"; // shader entrypoint
	vertStageInfo.module = vertModule;
	vertStageInfo.pSpecializationInfo = nullptr; // used to set constants for optimization

	vk::PipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = vk::_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.pName = "main";
	fragStageInfo.module = fragModule;
	fragStageInfo.pSpecializationInfo = nullptr;

	vk::PipelineShaderStageCreateInfo shaderStagesInfo[] = {vertStageInfo, fragStageInfo};

	// dynamic states
	// makes viewport size and cull area dynamic at render time, so we dont need to recreate pipeline with every resize
	std::vector<vk::DynamicState> dynamicStates = { vk::_DYNAMIC_STATE_VIEWPORT, vk::_DYNAMIC_STATE_SCISSOR };
	vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// static viewport/scissor creation
	vk::Viewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	vk::Rect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	vk::PipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthClampEnable = VK_FALSE; // clamps pixels outside range instead of discarding
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // disables rasterization
	rasterizerInfo.polygonMode = vk::_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1;
	rasterizerInfo.cullMode = vk::_CULL_MODE_NONE; // dont cull backwards faces
	rasterizerInfo.depthBiasEnable = VK_FALSE; // whether to alter depth values

	// multisampling
	vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_FALSE; // no multisampling
	multisamplingInfo.rasterizationSamples = vk::_SAMPLE_COUNT_1_BIT;

	// colour blending
	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::_COLOR_COMPONENT_R_BIT | vk::_COLOR_COMPONENT_G_BIT
										  | vk::_COLOR_COMPONENT_B_BIT | vk::_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = vk::_TRUE;
	colorBlendAttachment.srcColorBlendFactor = vk::_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = vk::_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = vk::_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = vk::_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = vk::_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = vk::_BLEND_OP_ADD;

	vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	// pipeline
	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = vk::_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pVertexInputState = &vertInputInfo;
	pipelineInfo.pInputAssemblyState = &assemblyInfo;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlendInfo;
	if (DYNAMIC_VIEWPORT) {
		pipelineInfo.pDynamicState = &dynamicStateInfo;
	}
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStagesInfo;
	pipelineInfo.subpass = 0;

	vk::Pipeline pipeline;
	if (vk::CreateGraphicsPipelines(device, vk::_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	return pipeline;
}

void Primrose::createMarchPipeline() {
	// shader modules
	vk::ShaderModule vertModule = createShaderModule((uint32_t*)flatVertSpvData, flatVertSpvSize);
	vk::ShaderModule fragModule = createShaderModule((uint32_t*)marchFragSpvData, marchFragSpvSize);

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 0; // no inputs
	vertInputInfo.vertexAttributeDescriptionCount = 0;

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyInfo.topology = vk::_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	marchPipeline = createPipeline(vertModule, fragModule,
		vertInputInfo, assemblyInfo);

	// cleanup
	vk::DestroyShaderModule(device, vertModule, nullptr);
	vk::DestroyShaderModule(device, fragModule, nullptr);
}

void Primrose::createUIPipeline() {
	// shader modules
	vk::ShaderModule vertModule = createShaderModule((uint32_t*)uiVertSpvData, uiVertSpvSize);
	vk::ShaderModule fragModule = createShaderModule((uint32_t*)uiFragSpvData, uiFragSpvSize);

	// vertex input
	auto bindingDesc = UIVertex::getBindingDescription();
	auto attributeDescs = UIVertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 1;
	vertInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertInputInfo.vertexAttributeDescriptionCount = attributeDescs.size();
	vertInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.sType = vk::_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyInfo.topology = vk::_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	uiPipeline = createPipeline(vertModule, fragModule,
		vertInputInfo, assemblyInfo);

	// cleanup
	vk::DestroyShaderModule(device, vertModule, nullptr);
	vk::DestroyShaderModule(device, fragModule, nullptr);
}

void Primrose::createMarchTexture() {
	int texWidth = windowWidth*2;
	int texHeight = windowHeight*2;
	size_t dataSize = texWidth * texHeight * 4;

	srand(time(nullptr));

	void* randData = malloc(dataSize);
	for (int i = 0; i < dataSize; ++i) {
		*(static_cast<uint8_t*>(randData) + i) = rand() % 256;
	}

	imageFromData(randData, texWidth, texHeight, &marchTexture, &marchTextureMemory);

	free(randData);

	marchImageView = createImageView(marchTexture, vk::_FORMAT_R8G8B8A8_SRGB);

	vk::PhysicalDeviceProperties deviceProperties{};
	vk::GetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.sType = vk::_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = vk::_FILTER_NEAREST; // when magnified, ie one fragment corresponds to multiple image pixels
	samplerInfo.minFilter = vk::_FILTER_LINEAR; // when minified, ie one fragment is between image pixels
	samplerInfo.addressModeU = vk::_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = vk::_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = vk::_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // true to sample using width/height coords instead of 0-1 range
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.mipmapMode = vk::_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.mipLodBias = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	if (vk::CreateSampler(device, &samplerInfo, nullptr, &marchSampler) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create march texture sampler");
	}
}



void Primrose::createCommandPool() {
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	vk::CommandPoolCreateInfo info{};
	info.sType = vk::_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = vk::_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow rerecording commands
	info.queueFamilyIndex = indices.graphicsFamily.value();

	if (vk::CreateCommandPool(device, &info, nullptr, &commandPool) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create command pool");
	}
}

void Primrose::createDescriptorPool() {
	vk::DescriptorPoolSize poolSizes[2]{};
	// uniform
	poolSizes[0].type = vk::_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	// sampler
	poolSizes[1].type = vk::_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;

	vk::DescriptorPoolCreateInfo info{};
	info.sType = vk::_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = 2;
	info.pPoolSizes = poolSizes;
//	info.maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	info.maxSets = 100;

	if (vk::CreateDescriptorPool(device, &info, nullptr, &descriptorPool) != vk::_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}
}

void Primrose::createFramesInFlight() {
	// resize vector
	framesInFlight.resize(MAX_FRAMES_IN_FLIGHT);

	// create objects for each frame
	std::cout << "Creating frames in flight" << std::endl;
	for (auto& frame : framesInFlight) {
		// create command buffers
		vk::CommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = vk::_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = vk::_COMMAND_BUFFER_LEVEL_PRIMARY; // primary = can be submitted directly to gpu, secondary = can be called from primary
		commandBufferInfo.commandBufferCount = 1;

		if (vk::AllocateCommandBuffers(device, &commandBufferInfo, &frame.commandBuffer) != vk::_SUCCESS) {
			throw std::runtime_error("failed to create command buffer");
		}

		// create semaphores
		vk::SemaphoreCreateInfo semaInfo{};
		semaInfo.sType = vk::_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vk::CreateSemaphore(device, &semaInfo, nullptr, &frame.imageAvailableSemaphore) != vk::_SUCCESS
			|| vk::CreateSemaphore(device, &semaInfo, nullptr, &frame.renderFinishedSemaphore) != vk::_SUCCESS) {
			throw std::runtime_error("failed to create semaphores");
		}

		// create fence
		vk::FenceCreateInfo fenceInfo{};
		fenceInfo.sType = vk::_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = vk::_FENCE_CREATE_SIGNALED_BIT; // default signaled on creation

		if (vk::CreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != vk::_SUCCESS) {
			throw std::runtime_error("failed to create fence");
		}

		// create uniform buffers
		createBuffer(sizeof(MarchUniforms), vk::_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//			vk::_MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vk::_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | vk::_MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::_MEMORY_PROPERTY_HOST_COHERENT_BIT, // smart access memory (256 mb)
			&frame.uniformBuffer, &frame.uniformBufferMemory);

		allocateDescriptorSet(&frame.descriptorSet);

		vk::WriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = vk::_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = frame.descriptorSet; // which set to update
		descriptorWrite.dstArrayElement = 0; // not using an array, 0
		descriptorWrite.descriptorCount = 1;
		vk::WriteDescriptorSet writes[2] = {descriptorWrite, descriptorWrite};

		vk::DescriptorBufferInfo uniformBufferInfo{};
		uniformBufferInfo.buffer = frame.uniformBuffer;
		uniformBufferInfo.offset = 0;
		uniformBufferInfo.range = sizeof(MarchUniforms);
		writes[0].dstBinding = 0; // which binding index
		writes[0].descriptorType = vk::_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &uniformBufferInfo;

		vk::DescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = vk::_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = marchImageView;
		imageInfo.sampler = marchSampler;
		writes[1].dstBinding = 1; // which binding index
		writes[1].descriptorType = vk::_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &imageInfo;

		vk::UpdateDescriptorSets(device, 2, writes, 0, nullptr); // apply write
	}

	std::cout << std::endl; // newline
}



void Primrose::cleanup() {
	// vulkan destruction
	for (const auto& frame : framesInFlight) {
		vk::DestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
		vk::DestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
		vk::DestroyFence(device, frame.inFlightFence, nullptr);

		vk::DestroyBuffer(device, frame.uniformBuffer, nullptr);
		vk::FreeMemory(device, frame.uniformBufferMemory, nullptr);
	}

	uiScene.clear();

	vk::DestroyImage(device, marchTexture, nullptr);
	vk::FreeMemory(device, marchTextureMemory, nullptr);
	vk::DestroyImageView(device, marchImageView, nullptr);
	vk::DestroySampler(device, marchSampler, nullptr);

	vk::DestroyDescriptorPool(device, descriptorPool, nullptr);
	vk::DestroyCommandPool(device, commandPool, nullptr);

	vk::DestroyPipeline(device, uiPipeline, nullptr);
	vk::DestroyPipeline(device, marchPipeline, nullptr);
	vk::DestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vk::DestroyPipelineLayout(device, pipelineLayout, nullptr);

	cleanupSwapchain();
	vk::DestroyRenderPass(device, renderPass, nullptr);

	vk::DestroyDevice(device, nullptr);
	vk::DestroySurfaceKHR(instance, surface, nullptr);

	if (Settings::validationEnabled) {
		auto vk::DestroyDebugUtilsMessengerEXT = (PFN_vk::DestroyDebugUtilsMessengerEXT)vk::GetInstanceProcAddr(instance, "vk::DestroyDebugUtilsMessengerEXT");
		if (vk::DestroyDebugUtilsMessengerEXT == nullptr) {
			//throw std::runtime_error("could not find vk::DestroyDebugUtilsMessengerEXT");
			std::cerr << "could not find vk::DestroyDebugUtilsMessengerEXT" << std::endl;
		}
		else {
			vk::DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
	}

	vk::DestroyInstance(instance, nullptr);

	// glfw destruction
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Primrose::cleanupSwapchain() {
	for (const auto& frame : swapchainFrames) {
		vk::DestroyFramebuffer(device, frame.framebuffer, nullptr);
		vk::DestroyImageView(device, frame.imageView, nullptr);
	}

	vk::DestroySwapchainKHR(device, swapchain, nullptr);
}

void Primrose::recreateSwapchain() {
	if (isWindowMinimized()) {
		windowMinimized = true;
		return;
	}

	std::cout << "recreating swapchain might mess up screen idk" << std::endl;
	std::cout << "Recreating Swapchain                          " << std::endl;

	vk::DeviceWaitIdle(device);

	cleanupSwapchain();

	createSwapchain();
	createSwapchainFrames();

	//for (auto& frame : framesInFlight) {
	//	frame.uniforms.screenHeight = (float)swapchainExtent.height / (float)swapchainExtent.width;
	//}
}
