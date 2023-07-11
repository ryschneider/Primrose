#include "engine.hpp"
#include "state.hpp"
#include "engine_runtime.hpp"
#include "embed/flat_vert_spv.h"
#include "embed/march_frag_spv.h"
#include "embed/ui_vert_spv.h"
#include "embed/ui_frag_spv.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <fstream>
#include <cstring>

namespace Primrose {
	VkInstance instance; // used as global vulkan state
	VkPhysicalDevice physicalDevice; // graphics device
	VkDevice device; // logical connection to the physical device

	VkRenderPass renderPass; // render pass with commands used to render a frame
	VkDescriptorSetLayout descriptorSetLayout; // layout for shader uniforms
	VkPipelineLayout pipelineLayout; // graphics pipeline layout

	VkPipeline marchPipeline; // graphics pipeline
	VkPipeline uiPipeline; // graphics pipeline

	GLFWwindow* window;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain; // chain of images queued to be presented to display
	std::vector<SwapchainFrame> swapchainFrames;
	VkFormat swapchainImageFormat; // pixel format used in swapchain
	VkExtent2D swapchainExtent; // resolution of swapchain

	VkDescriptorPool descriptorPool;

	std::vector<FrameInFlight> framesInFlight;

	VkCommandPool commandPool;
	VkQueue graphicsQueue; // queue for graphics commands sent to gpu
	VkQueue presentQueue; // queue for present commands sent to gpu

	VkDebugUtilsMessengerEXT debugMessenger; // handles logging validation details

	void(*mouseMovementCallback)(float xpos, float ypos, bool refocused) = nullptr;

	// glfw callbacks
	namespace {
		void windowResizedCallback(GLFWwindow*, int, int) {
			windowResized = true;
		}
		void windowFocusCallback(GLFWwindow*, int focused) {
			if (focused != GL_TRUE) {
				windowFocused = false;
			}
		}
		void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
			bool refocused = !windowFocused && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GL_TRUE;
			if (refocused) windowFocused = true;
			if (mouseMovementCallback != nullptr) mouseMovementCallback(xpos, ypos, refocused);
		}
		void keyCallback(GLFWwindow*, int key, int, int, int) {
			if (key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, GL_TRUE);
			}
		}
		void mouseButtonCallback(GLFWwindow*, int button, int, int) {
			if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
				setZoom(1.f);
			}
		}
		void scrollCallback(GLFWwindow*, double, double scroll) {
			float co = 1.05;
			setZoom(zoom * pow(co, scroll));
		}
	}

	// vulkan callbacks
	namespace {
		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {

			if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT // if warning or worse
				|| type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { // or non-optimal use of vulkan

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

		int getPhysicalDeviceVramMb(VkPhysicalDevice phyDevice) {
			VkPhysicalDeviceMemoryProperties memoryProperties;
			vkGetPhysicalDeviceMemoryProperties(phyDevice, &memoryProperties);

			int vram = 0;
			for (unsigned int i = 0; i < memoryProperties.memoryHeapCount; ++i) {
				const auto& heap = memoryProperties.memoryHeaps + i;

				if (heap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) { // if memory is local to gpu
					vram += (int)(heap->size / (1024 * 1024));
				}
			}
			return vram;
		}

		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
		};
		QueueFamilyIndices getQueueFamilies(VkPhysicalDevice phyDevice) {
			uint32_t queueCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueCount);
			vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueCount, queueFamilies.data());

			QueueFamilyIndices indices;

			int i = 0;
			for (const auto& family : queueFamilies) {
				bool graphicsSupport = family.queueFlags & VK_QUEUE_GRAPHICS_BIT; // family supports graphics queue

				VkBool32 presentSupport = false; // family supports present queue
				vkGetPhysicalDeviceSurfaceSupportKHR(phyDevice, i, surface, &presentSupport);

				if (graphicsSupport && presentSupport) { // will overwrite existing indices if both found in one family
					indices.graphicsFamily = i;
					indices.presentFamily = i;
					break;
				}
				else if (graphicsSupport && (!indices.graphicsFamily.has_value())) {
					indices.graphicsFamily = i;
				}
				else if (presentSupport && (!indices.presentFamily.has_value())) {
					indices.presentFamily = i;
				}

				++i;
			}

			return indices;
		}

		struct SwapchainDetails {
			VkSurfaceCapabilitiesKHR capabilities{};
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};
		SwapchainDetails getSwapchainDetails(VkPhysicalDevice phyDevice) {
			SwapchainDetails details;

			// capabilities
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phyDevice, surface, &details.capabilities);

			// formats
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(phyDevice, surface, &formatCount, nullptr);
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(phyDevice, surface, &formatCount, details.formats.data());

			// present modes
			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(phyDevice, surface, &presentModeCount, nullptr);
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(phyDevice, surface, &presentModeCount, details.presentModes.data());

			return details;
		}

		bool isPhysicalDeviceSuitable(VkPhysicalDevice phyDevice) {
			// check for correct queue families
			QueueFamilyIndices indices = getQueueFamilies(phyDevice);
			if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) return false;

			// check if extensions are supported
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, availableExtensions.data());

			bool extensionsSupported = true;
			for (const auto& required : REQUIRED_EXTENSIONS) {
				bool found = false;
				for (const auto& available : availableExtensions) {
					if (strcmp(required, available.extensionName) == 0) {
						found = true;
						break;
					}
				}

				if (!found) {
					extensionsSupported = false;
					break;
				}
			}
			if (!extensionsSupported) return false;

			// make sure swapchain is adequate
			SwapchainDetails swapchainDetails = getSwapchainDetails(phyDevice);
			if (swapchainDetails.formats.empty() || swapchainDetails.presentModes.empty()) return false;

			// make sure anisotrophy is supported
			// TODO support devices without anisotrophy by forcing disabled in settings
			VkPhysicalDeviceFeatures supportedFeatures;
			vkGetPhysicalDeviceFeatures(phyDevice, &supportedFeatures);
			if (supportedFeatures.samplerAnisotropy == VK_FALSE) return false;

			// device is suitable for app
			return true;
		}
	}
}

void Primrose::allocateDescriptorSet(VkDescriptorSet* descSet) {
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	if (vkAllocateDescriptorSets(device, &allocInfo, descSet) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set :)");
	}
}

void Primrose::createDeviceMemory(VkMemoryRequirements memReqs,
	VkMemoryPropertyFlags properties, VkDeviceMemory* memory) {

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	uint32_t bestMemory;
	bool found = false;
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		// which of chosen [properties] are supported
		VkMemoryPropertyFlags supportedProperties = memProperties.memoryTypes[i].propertyFlags & properties;

		if (memReqs.memoryTypeBits & (1 << i) // check if memory is suitable according to memReqs
			&& supportedProperties == properties) { // and all properties are supported
			bestMemory = i;
			found = true;
			break;
		}
	}
	if (!found) {
		throw std::runtime_error("failed to find suitable memory type");
	}

	uint32_t heapIndex = memProperties.memoryTypes[bestMemory].heapIndex;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = bestMemory;
//	std::cout << "Creating " << memReqs.size << " byte buffer on heap " << heapIndex << " (";
//	std::cout << memProperties.memoryHeaps[heapIndex].size / (1024 * 1024) << " mb)" << std::endl;

	if (vkAllocateMemory(device, &allocInfo, nullptr, memory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate uniform buffer memory");
	}
}
void Primrose::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {

	// create buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer");
	}

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

	createDeviceMemory(memReqs, properties, bufferMemory);

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0); // bind memory to buffer
}

void Primrose::writeToDevice(VkDeviceMemory memory, void* data, size_t size) {
	void* dst;
	vkMapMemory(device, memory, 0, size, 0, &dst);
	memcpy(dst, data, size);
	vkUnmapMemory(device, memory);
}

VkCommandBuffer Primrose::startSingleTimeCommandBuffer() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	return cmdBuffer;
}
void Primrose::endSingleTimeCommandBuffer(VkCommandBuffer cmdBuffer) {
	if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record copy command buffer");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);
	// note - if copying multiple buffers, could submit them all then wait for every fence to complete at the end

	vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
}

void Primrose::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	VkCommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	vkCmdPipelineBarrier(cmdBuffer,
		srcStage, dstStage, 0, 0, nullptr,
		0, nullptr, 1, &barrier);
	endSingleTimeCommandBuffer(cmdBuffer);
}
void Primrose::importTexture(const char* path, VkImage* image, VkDeviceMemory* imageMemory, float* aspect) {
	// load image data
	int width, height, channels;
	std::cout << "Importing " << path << std::endl;
	stbi_uc* data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
	if (stbi_failure_reason()) {
		throw std::runtime_error(std::string("stbi_uc error: ").append(stbi_failure_reason()));
	}
	VkDeviceSize dataSize = width * height * 4;

	*aspect = (float)width / (float)height;

	// create and write to staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);
	writeToDevice(stagingBufferMemory, data, dataSize);

	// free image data
	stbi_image_free(data);

	// create image handler
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = {(uint32_t)width, (uint32_t)height, 1}; // depth=1 for 2d image
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, image) != VK_SUCCESS) {
		throw std::runtime_error(std::string("failed to create image ").append(path));
	}

	// create & bind image memory
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, *image, &memReqs);
	createDeviceMemory(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageMemory);
	vkBindImageMemory(device, *image, *imageMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(*image, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy data to image
	VkCommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {(uint32_t)width, (uint32_t)height, 1};
	vkCmdCopyBufferToImage(cmdBuffer,
		stagingBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(*image, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// cleanup staging buffer
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

VkImageView Primrose::createImageView(VkImage image, VkFormat format) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create ui texture image view");
	}
	return imageView;
}

VkShaderModule Primrose::createShaderModule(const uint32_t* code, size_t length) {
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = length;
	info.pCode = code;

	VkShaderModule shader;
	if (vkCreateShaderModule(device, &info, nullptr, &shader) != VK_SUCCESS) {
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
	glfwSetWindowFocusCallback(window, windowFocusCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetScrollCallback(window, scrollCallback);

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
	createFramesInFlight();
}

void Primrose::setupDebugMessenger() {
	if (!Settings::validationEnabled) return;

	VkDebugUtilsMessengerCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = debugCallback;

	auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (vkCreateDebugUtilsMessengerEXT == nullptr) {
		throw std::runtime_error("could not find vkCreateDebugUtilsMessengerEXT");
	}

	if (vkCreateDebugUtilsMessengerEXT(instance, &info, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger");
	}
}

void Primrose::createInstance() {
	if (Settings::validationEnabled) {
		// check for validation layers
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = appVersion;
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.engineVersion = ENGINE_VERSION;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// vk extensions required for glfw
	std::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	// enable validation layers in instance
	VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
	if (Settings::validationEnabled) {
		createInfo.enabledLayerCount = (uint32_t)VALIDATION_LAYERS.size();
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

		// enables debugger during instance creation and destruction
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = &debugInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	// create instance
	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance");
	}
}

void Primrose::createSurface() {
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
}

void Primrose::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("no GPU with vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	int bestScore = -1;
	VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
	for (const auto& phyDevice : devices) {

		if (!isPhysicalDeviceSuitable(phyDevice)) {
			continue;
		}

		// score graphics devices based on type and specs
		int score = 0;

		// priority for dedicated graphics cards - arbitrary large number for priority
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(phyDevice, &properties);
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1 << 30; // 2^30
		}
		else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
			score += 1 << 29; // 2^29
		}

		// score multiple graphics cards of same type by vram
		score += getPhysicalDeviceVramMb(phyDevice);

		if (score > bestScore) {
			bestScore = score;
			bestDevice = phyDevice;
		}
	}

	if (bestDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("no suitable graphics device could be found");
	}

	physicalDevice = bestDevice;
	std::cout << "Choosing graphics device with " << getPhysicalDeviceVramMb(physicalDevice) << " mb VRAM" << std::endl;

	std::cout << std::endl; // newline to seperate prints
}

void Primrose::createLogicalDevice() {
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::set<uint32_t> uniqueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float priority = 1;
	for (uint32_t family : uniqueFamilies) {
		VkDeviceQueueCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = family;
		info.queueCount = 1;
		info.pQueuePriorities = &priority;
		queueInfos.push_back(info);
	}

	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = (uint32_t)queueInfos.size(); // create queues
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.enabledExtensionCount = (uint32_t)REQUIRED_EXTENSIONS.size(); // enable extensions
	createInfo.ppEnabledExtensionNames = REQUIRED_EXTENSIONS.data();
	createInfo.pEnabledFeatures = &features;

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void Primrose::createSwapchain() {
	SwapchainDetails swapDetails = getSwapchainDetails(physicalDevice);

	// choose surface format
	bool ideal = false;
	VkSurfaceFormatKHR surfaceFormat = swapDetails.formats[0]; // default if ideal not found
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
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& availableMode : swapDetails.presentModes) {
		if (availableMode == IDEAL_PRESENT_MODE) {
			presentMode = availableMode;
			ideal = true;
			break;
		}
	}
	std::cout << "Swapchain present mode: " << PRESENT_MODE_STRINGS.find(presentMode)->second << (ideal ? " (ideal)" : " (not ideal)") << std::endl;

	// choose swap extent (resolution of images)
	if (swapDetails.capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
		// if currentExtent.width is max value, use the size of the window as extent
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// keep within min and max image extent allowed by swapchain
		swapchainExtent.width = std::clamp((uint32_t)width,
			swapDetails.capabilities.minImageExtent.width, swapDetails.capabilities.maxImageExtent.width);
		swapchainExtent.height = std::clamp((uint32_t)height,
			swapDetails.capabilities.minImageExtent.height, swapDetails.capabilities.maxImageExtent.height);
	}
	else {
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
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount; // only the min number images, could be more
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapchainExtent;
	createInfo.imageArrayLayers = 1; // layers per image
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // what we're using the images for, ie rendering to directly
	createInfo.presentMode = presentMode;

	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);
	uint32_t familyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily == indices.presentFamily) {
		// exclusive, ie each image is only owned by one queue family
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	else {
		// concurrent, ie images can be used between multiple queue families
		// TODO - make more efficient by explicitely transferring images between families when needed
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = familyIndices;
	}

	createInfo.preTransform = swapDetails.capabilities.currentTransform; // no special transformations
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // alpha bit is ignored by compositor
	createInfo.clipped = VK_TRUE; // cull pixels that are covered by other windows

	createInfo.oldSwapchain = VK_NULL_HANDLE; // handle for last swapchain

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain");
	}

	// print num of images (device could have chosen different number than imageCount)
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	std::cout << "Swapchain images: " << imageCount;
	std::cout << " (" << swapDetails.capabilities.minImageCount << " to ";
	std::cout << swapDetails.capabilities.maxImageCount << ")" << std::endl;

	std::cout << std::endl; // newline to seperate prints
}

void Primrose::createRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // multisampling count

	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with the framebuffer data before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what to do with the framebuffer data after rendering

	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // dont care what layout it has before rendering
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // want the image to be presentable after rendering

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // graphics subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; // the outputs of the frag shader map to the buffers in this array
	// eg layout(location = 0) refers to pColorAttachments[0]

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to first implicit subpass
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // all the commands in this subpass must reach the color attachment stage before the destination subpass can begin
	dependency.srcAccessMask = 0;

	dependency.dstSubpass = 0; // refers to our subpass
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // commands in our subpass cannot enter this stage until all the commands from the external subpass have reached this stage
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // specifically don't allow writing the colors

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass");
	}
}

void Primrose::createSwapchainFrames() {
	// fetch images from swapchain
	uint32_t imageCount;
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	std::vector<VkImage> images(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

	// framebuffer settings
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
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
		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFrames[i].framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer");
		}
	}
}

void Primrose::createDescriptorSetLayout() {
	// uniform binding
	VkDescriptorSetLayoutBinding uniformBinding{};
	uniformBinding.binding = 0; // binding in shader
	uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // uniform buffer
	uniformBinding.descriptorCount = 1; // numbers of ubos
	uniformBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // shader stage

	// texture binding
	VkDescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 1; // binding in shader
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // uniform buffer
	textureBinding.descriptorCount = 1; // numbers of textures
	textureBinding.pImmutableSamplers = nullptr;
	textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // shader stage

	VkDescriptorSetLayoutBinding bindings[2] = {uniformBinding, textureBinding};

	// extra flags for descriptor set layout
	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pBindingFlags = &flags;

	// create descriptor set layout
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 2;
	info.pBindings = bindings;
	info.pNext = &bindingFlags;

	if (vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}
}

void Primrose::createPipelineLayout() {
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = 128; // min required size
	pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	layoutInfo.pPushConstantRanges = &pushConstant;
	layoutInfo.pushConstantRangeCount = 1;

	if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}
}

VkPipeline Primrose::createPipeline(
	VkShaderModule vertModule, VkShaderModule fragModule,
	VkPipelineVertexInputStateCreateInfo vertInputInfo,
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo) {

	VkPipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.pName = "main"; // shader entrypoint
	vertStageInfo.module = vertModule;
	vertStageInfo.pSpecializationInfo = nullptr; // used to set constants for optimization

	VkPipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.pName = "main";
	fragStageInfo.module = fragModule;
	fragStageInfo.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo shaderStagesInfo[] = {vertStageInfo, fragStageInfo};

	// dynamic states
	// makes viewport size and cull area dynamic at render time, so we dont need to recreate pipeline with every resize
	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// static viewport/scissor creation
	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	// rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthClampEnable = VK_FALSE; // clamps pixels outside range instead of discarding
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // disables rasterization
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1;
	rasterizerInfo.cullMode = VK_CULL_MODE_NONE; // dont cull backwards faces
	rasterizerInfo.depthBiasEnable = VK_FALSE; // whether to alter depth values

	// multisampling
	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_FALSE; // no multisampling
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// colour blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
										  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	// pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	return pipeline;
}

void Primrose::createMarchPipeline() {
	// shader modules
	VkShaderModule vertModule = createShaderModule((uint32_t*)flatVertSpvData, flatVertSpvSize);
	VkShaderModule fragModule = createShaderModule((uint32_t*)marchFragSpvData, marchFragSpvSize);

	// vertex input
	VkPipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 0; // no inputs
	vertInputInfo.vertexAttributeDescriptionCount = 0;

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	marchPipeline = createPipeline(vertModule, fragModule,
		vertInputInfo, assemblyInfo);

	// cleanup
	vkDestroyShaderModule(device, vertModule, nullptr);
	vkDestroyShaderModule(device, fragModule, nullptr);
}

void Primrose::createUIPipeline() {
	// shader modules
	VkShaderModule vertModule = createShaderModule((uint32_t*)uiVertSpvData, uiVertSpvSize);
	VkShaderModule fragModule = createShaderModule((uint32_t*)uiFragSpvData, uiFragSpvSize);

	// vertex input
	auto bindingDesc = UIVertex::getBindingDescription();
	auto attributeDescs = UIVertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 1;
	vertInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertInputInfo.vertexAttributeDescriptionCount = attributeDescs.size();
	vertInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	uiPipeline = createPipeline(vertModule, fragModule,
		vertInputInfo, assemblyInfo);

	// cleanup
	vkDestroyShaderModule(device, vertModule, nullptr);
	vkDestroyShaderModule(device, fragModule, nullptr);
}

void Primrose::createCommandPool() {
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow rerecording commands
	info.queueFamilyIndex = indices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool");
	}
}

void Primrose::createDescriptorPool() {
	VkDescriptorPoolSize poolSizes[2]{};
	// uniform
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	// sampler
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = 2;
	info.pPoolSizes = poolSizes;
//	info.maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT;
	info.maxSets = 100;

	if (vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool) != VK_SUCCESS) {
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
		VkCommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary = can be submitted directly to gpu, secondary = can be called from primary
		commandBufferInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(device, &commandBufferInfo, &frame.commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command buffer");
		}

		// create semaphores
		VkSemaphoreCreateInfo semaInfo{};
		semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(device, &semaInfo, nullptr, &frame.imageAvailableSemaphore) != VK_SUCCESS
			|| vkCreateSemaphore(device, &semaInfo, nullptr, &frame.renderFinishedSemaphore) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores");
		}

		// create fence
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // default signaled on creation

		if (vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fence");
		}

		// create uniform buffers
		createBuffer(sizeof(MarchUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // smart access memory (256 mb)
			&frame.uniformBuffer, &frame.uniformBufferMemory);

		allocateDescriptorSet(&frame.descriptorSet);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = frame.descriptorSet; // which set to update
		descriptorWrite.dstArrayElement = 0; // not using an array, 0
		descriptorWrite.descriptorCount = 1;

		VkDescriptorBufferInfo uniformBufferInfo{};
		uniformBufferInfo.buffer = frame.uniformBuffer;
		uniformBufferInfo.offset = 0;
		uniformBufferInfo.range = sizeof(MarchUniforms);
		descriptorWrite.dstBinding = 0; // which binding index
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &uniformBufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr); // apply write
	}

	std::cout << std::endl; // newline
}



void Primrose::cleanup() {
	// vulkan destruction
	for (const auto& frame : framesInFlight) {
		vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
		vkDestroyFence(device, frame.inFlightFence, nullptr);

		vkDestroyBuffer(device, frame.uniformBuffer, nullptr);
		vkFreeMemory(device, frame.uniformBufferMemory, nullptr);
	}

	uiScene.clear();

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);

	vkDestroyPipeline(device, uiPipeline, nullptr);
	vkDestroyPipeline(device, marchPipeline, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	cleanupSwapchain();
	vkDestroyRenderPass(device, renderPass, nullptr);

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (Settings::validationEnabled) {
		auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT == nullptr) {
			//throw std::runtime_error("could not find vkDestroyDebugUtilsMessengerEXT");
			std::cerr << "could not find vkDestroyDebugUtilsMessengerEXT" << std::endl;
		}
		else {
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
	}

	vkDestroyInstance(instance, nullptr);

	// glfw destruction
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Primrose::cleanupSwapchain() {
	for (const auto& frame : swapchainFrames) {
		vkDestroyFramebuffer(device, frame.framebuffer, nullptr);
		vkDestroyImageView(device, frame.imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void Primrose::recreateSwapchain() {
	if (isWindowMinimized()) {
		windowMinimized = true;
		return;
	}

	std::cout << "recreating swapchain might mess up screen idk" << std::endl;
	std::cout << "Recreating Swapchain                          " << std::endl;

	vkDeviceWaitIdle(device);

	cleanupSwapchain();

	createSwapchain();
	createSwapchainFrames();

	//for (auto& frame : framesInFlight) {
	//	frame.uniforms.screenHeight = (float)swapchainExtent.height / (float)swapchainExtent.width;
	//}
}
