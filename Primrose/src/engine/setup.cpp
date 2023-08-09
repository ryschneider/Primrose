#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "engine/setup.hpp"
#include "state.hpp"
#include "engine/runtime.hpp"
#include "engine/pipeline_accelerated.hpp"
#include "engine/pipeline_raster.hpp"
#include "embed/ui_vert_spv.h"
#include "embed/ui_frag_spv.h"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cmath>
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

	bool rayAcceleration; // set by pickPhysicalDevice
	vk::DescriptorSetLayout mainDescriptorLayout;
	vk::PipelineLayout mainPipelineLayout;
	vk::Pipeline mainPipeline;

	vk::DescriptorSetLayout uiDescriptorLayout;
	vk::PipelineLayout uiPipelineLayout;
	vk::Pipeline uiPipeline;

	GLFWwindow* window;
	vk::SurfaceKHR surface;

	vk::SwapchainKHR swapchain; // chain of images queued to be presented to display
	std::vector<SwapchainFrame> swapchainFrames;
	vk::Format swapchainImageFormat; // pixel format used in swapchain
	vk::Extent2D swapchainExtent; // resolution of swapchain

	int windowWidth = 0;
	int windowHeight = 0;

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
		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {

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
			std::vector<vk::QueueFamilyProperties> queueFamilies = phyDevice.getQueueFamilyProperties();

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

		bool isPhysicalDeviceSuitable(vk::PhysicalDevice phyDevice, bool* supportsRayAcc) {
			// check for correct queue families
			QueueFamilyIndices indices = getQueueFamilies(phyDevice);
			if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) return false;

			// check if extensions are supported
			auto availableExtensions = phyDevice.enumerateDeviceExtensionProperties();
			for (const auto& required : REQUIRED_EXTENSIONS) {
				if (std::find_if(availableExtensions.begin(), availableExtensions.end(),
					[required](vk::ExtensionProperties& available) {
					return strcmp(required, available.extensionName) == 0;
				}) == availableExtensions.end()) {

					return false; // required extension not supported
				}
			}

			*supportsRayAcc = true;
			for (const auto& rayExt : RAY_EXTENSIONS) {
				if (std::find_if(availableExtensions.begin(), availableExtensions.end(),
					[rayExt](VkExtensionProperties& available) {
						return strcmp(rayExt, available.extensionName) == 0;
					}) == availableExtensions.end()) {

					*supportsRayAcc = false; // doesn't support some ray extension
					break;
				}
			}

			// make sure swapchain is adequate
			auto formats = phyDevice.getSurfaceFormatsKHR(surface);
			auto presentModes = phyDevice.getSurfacePresentModesKHR(surface);
			if (formats.empty() || presentModes.empty()) return false;

			// make sure anisotrophy is supported
			// TODO support devices without anisotrophy by forcing disabled in settings
			vk::PhysicalDeviceFeatures supportedFeatures = phyDevice.getFeatures();
			if (supportedFeatures.samplerAnisotropy == VK_FALSE) return false;

			// device is suitable for app
			return true;
		}
	}
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE // storage for vulkan.hpp dynamic loader

void Primrose::createDeviceMemory(vk::MemoryRequirements memReqs,
	vk::MemoryPropertyFlags properties, vk::DeviceMemory* memory) {

	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

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

//	uint32_t heapIndex = memProperties.memoryTypes[bestMemory].heapIndex;

	vk::MemoryAllocateInfo allocInfo{};
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = bestMemory;
//	std::cout << "Creating " << memReqs.size << " byte buffer on heap " << heapIndex << " (";
//	std::cout << memProperties.memoryHeaps[heapIndex].size / (1024 * 1024) << " mb)" << std::endl;

	*memory = device.allocateMemory(allocInfo);
}
void Primrose::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
	vk::MemoryPropertyFlags properties, vk::Buffer* buffer, vk::DeviceMemory* bufferMemory) {

	// create buffer
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	*buffer = device.createBuffer(bufferInfo);

	vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(*buffer);

	createDeviceMemory(memReqs, properties, bufferMemory);

	device.bindBufferMemory(*buffer, *bufferMemory, 0); // bind memory to buffer
}

void Primrose::writeToDevice(vk::DeviceMemory memory, void* data, size_t size, size_t offset) {
	void* dst = device.mapMemory(memory, offset, size);
	memcpy(dst, data, size);
	device.unmapMemory(memory);
}

vk::CommandBuffer Primrose::startSingleTimeCommandBuffer() {
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(allocInfo)[0];

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	cmdBuffer.begin(beginInfo);

	return cmdBuffer;
}
void Primrose::endSingleTimeCommandBuffer(vk::CommandBuffer cmdBuffer) {
	cmdBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	if (graphicsQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
		throw std::runtime_error("failed to submit single time cmd buffer");
	}
	graphicsQueue.waitIdle();
	// note - if copying multiple buffers, could submit them all then wait for every fence to complete at the end

	device.freeCommandBuffers(commandPool, cmdBuffer);
}

void Primrose::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	vk::ImageMemoryBarrier barrier{};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}

	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	cmdBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits::eByRegion,
		0, nullptr, 0, nullptr, 1, &barrier);
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
	createBuffer(dataSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		&stagingBuffer, &stagingBufferMemory);
	writeToDevice(stagingBufferMemory, data, dataSize);

	// create image handler
	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent = vk::Extent3D(width, height, 1); // depth=1 for 2d image
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = vk::Format::eR8G8B8A8Srgb;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.samples = vk::SampleCountFlagBits::e1;

	*image = device.createImage(imageInfo);

	// create & bind image memory
	vk::MemoryRequirements memReqs = device.getImageMemoryRequirements(*image);
	createDeviceMemory(memReqs, vk::MemoryPropertyFlagBits::eDeviceLocal, imageMemory);
	device.bindImageMemory(*image, *imageMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(*image, vk::Format::eR8G8B8A8Srgb,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

	// copy data to image
	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();
	vk::BufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = vk::Offset3D(0, 0, 0);
	region.imageExtent = vk::Extent3D(width, height, 1);
	cmdBuffer.copyBufferToImage(stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(*image, vk::Format::eR8G8B8A8Srgb,
		vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	// cleanup staging buffer
	device.destroyBuffer(stagingBuffer);
	device.freeMemory(stagingBufferMemory);
}

vk::ImageView Primrose::createImageView(vk::Image image, vk::Format format) {
	vk::ImageViewCreateInfo viewInfo{};
	viewInfo.image = image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	return device.createImageView(viewInfo);
}

vk::ShaderModule Primrose::createShaderModule(const uint32_t* code, size_t length) {
	vk::ShaderModuleCreateInfo info{};
	info.codeSize = length;
	info.pCode = code;

	return device.createShaderModule(info); // TODO just inline everywhere
}



void Primrose::setup(const char* applicationName, unsigned int applicationVersion) {
	Primrose::appName = applicationName;
	Primrose::appVersion = applicationVersion;

	initWindow();
	initVulkan();

	// TODO put screenHeight definition in swapchainExtent setter
	setFov(Settings::fov); // initial set fov
	setZoom(1.f); // initial set zoom
	uniforms.screenHeight = static_cast<float>(swapchainExtent.height) / static_cast<float>(swapchainExtent.width);
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
	// set up vulkan.hpp dynamic loader
	static vk::DynamicLoader dl; // static since it needs to last entire program
	VULKAN_HPP_DEFAULT_DISPATCHER.init(dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();

	createSwapchain();
	createRenderPass();
	createSwapchainFrames();

	// testing
	createAcceleratedPipelineLayout();
	createAcceleratedPipeline();
	device.destroyPipeline(mainPipeline);
	device.destroyPipelineLayout(mainPipelineLayout);
	device.destroyDescriptorSetLayout(mainDescriptorLayout);
	// testing

	if (rayAcceleration) {
		createAcceleratedPipelineLayout();
		createAcceleratedPipeline();
	} else {
		createRasterPipelineLayout();
		createRasterPipeline();
	}
	createUIPipeline();

	createCommandPool();

	createDitherTexture();

	createFramesInFlight();
}



void Primrose::setupDebugMessenger() {
	if (!Settings::validationEnabled) return;

	vk::DebugUtilsMessengerCreateInfoEXT info{};
	info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
		| vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
		| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
		| vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
		| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	info.pfnUserCallback = debugCallback;

	debugMessenger = instance.createDebugUtilsMessengerEXT(info);
}

void Primrose::createInstance() {
	if (Settings::validationEnabled) {
		// check for validation layers
		auto availableLayers = vk::enumerateInstanceLayerProperties();

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
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = appVersion;
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.engineVersion = ENGINE_VERSION;
	appInfo.apiVersion = VK_API_VERSION_1_3;

	vk::InstanceCreateInfo createInfo{};
	createInfo.pApplicationInfo = &appInfo;

	// vk extensions required for glfw
	std::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	// enable validation layers in instance
	vk::DebugUtilsMessengerCreateInfoEXT debugInfo{};
	if (Settings::validationEnabled) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

		// enables debugger during instance creation and destruction
		debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
							   | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
							   | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
							   | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
		debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
						   | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
						   | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
		debugInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = &debugInfo;
	} else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	// create instance
	instance = vk::createInstance(createInfo);

	// associate with dynamic loader
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
}

void Primrose::createSurface() {
	VkSurfaceKHR tempSurface; // since glfwCreateWindowSurface needs VkSurfaceKHR*
	if (glfwCreateWindowSurface(VkInstance(instance), window, nullptr, &tempSurface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
	surface = tempSurface;
}



void Primrose::pickPhysicalDevice() {
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
	if (devices.empty()) {
		throw std::runtime_error("no GPU with vulkan support");
	}

	int bestScore = -1;
	vk::PhysicalDevice bestDevice = VK_NULL_HANDLE;
	bool bestSupportsRay = false;
	for (const auto& phyDevice : devices) {
		bool supportsRay = false;
		if (!isPhysicalDeviceSuitable(phyDevice, &supportsRay)) {
			continue;
		}

		// score graphics devices based on nodeType and specs
		int score = 0;

		// priority for dedicated graphics cards - arbitrary large number for priority
		vk::PhysicalDeviceProperties properties = phyDevice.getProperties();

		if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 1 << 30; // 2^30
		} else if (properties.deviceType == vk::PhysicalDeviceType::eVirtualGpu) {
			score += 1 << 29; // 2^29
		}

		if (supportsRay) {
			score += 1 << 28; // discrete gpus with raytracing support higher priority
		}

		// score multiple graphics cards of same nodeType by vram
		score += getPhysicalDeviceVramMb(phyDevice);

		if (score > bestScore) {
			bestScore = score;
			bestDevice = phyDevice;
			bestSupportsRay = supportsRay;
		}
	}

	if (VkPhysicalDevice(bestDevice) == VK_NULL_HANDLE) {
		throw std::runtime_error("no suitable graphics device could be found");
	}

	physicalDevice = bestDevice;
	rayAcceleration = bestSupportsRay && false;
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
		info.queueFamilyIndex = family;
		info.queueCount = 1;
		info.pQueuePriorities = &priority;
		queueInfos.push_back(info);
	}

	vk::PhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;

	std::vector<const char*> extensions;
	extensions.insert(extensions.end(), REQUIRED_EXTENSIONS.begin(), REQUIRED_EXTENSIONS.end());
	if (rayAcceleration || true) extensions.insert(extensions.end(), RAY_EXTENSIONS.begin(), RAY_EXTENSIONS.end()); // TODO remove ||true

	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures(VK_TRUE);

	vk::DeviceCreateInfo createInfo{};
	if (rayAcceleration || true) createInfo.pNext = &rtFeatures;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()); // create queues
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size()); // enable extensions
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.pEnabledFeatures = &features;

	device = physicalDevice.createDevice(createInfo);

	// associate with dynamic loader
	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

	// get device queues
	graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
	presentQueue = device.getQueue(indices.presentFamily.value(), 0);
}

void Primrose::createSwapchain() {
	vk::SurfaceCapabilitiesKHR swapCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	std::vector<vk::SurfaceFormatKHR> swapFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	std::vector<vk::PresentModeKHR> swapPresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	// choose surface format
	bool ideal = false;
	vk::SurfaceFormatKHR surfaceFormat = swapFormats[0]; // default if ideal not found
	for (const auto format : swapFormats) {
		if (format == IDEAL_SURFACE_FORMAT) {
			surfaceFormat = format;
			ideal = true;
			break;
		}
	}
	std::cout << "Swapchain format: " << to_string(surfaceFormat.format) << ", " << to_string(surfaceFormat.colorSpace);
	std::cout << (ideal ? " (ideal)" : " (not ideal)") << std::endl;
	swapchainImageFormat = surfaceFormat.format; // save format to global var

	// choose presentation mode
	ideal = false;
	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
	for (const auto& availableMode : swapPresentModes) {
		if (availableMode == IDEAL_PRESENT_MODE) {
			presentMode = availableMode;
			ideal = true;
			break;
		}
	}
	std::cout << "Swapchain present mode: " << to_string(presentMode) << (ideal ? " (ideal)" : " (not ideal)") << std::endl;

	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	// choose swap extent (resolution of images)
	if (swapCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
		// if currentExtent.width is max value, use the size of the window as extent

		// keep within min and max image extent allowed by swapchain
		swapchainExtent.width = std::clamp(static_cast<uint32_t>(windowWidth),
			swapCapabilities.minImageExtent.width, swapCapabilities.maxImageExtent.width);
		swapchainExtent.height = std::clamp(static_cast<uint32_t>(windowHeight),
			swapCapabilities.minImageExtent.height, swapCapabilities.maxImageExtent.height);
	} else {
		swapchainExtent = swapCapabilities.currentExtent; // use extent given
	}
	std::cout << "Swapchain extent: (" << swapchainExtent.width << ", " << swapchainExtent.height << ")" << std::endl;

	// number of images held by swapchain
	uint32_t imageCount = swapCapabilities.minImageCount + 1; // use 1 more than minimum
	if (imageCount > swapCapabilities.maxImageCount // up to maximum
		&& swapCapabilities.maxImageCount > 0) { // 0 means unlimited maximum
		imageCount = swapCapabilities.maxImageCount;
	}

	// create info
	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount; // only the min number images, could be more
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapchainExtent;
	createInfo.imageArrayLayers = 1; // layers per image
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // what we're using the images for
	createInfo.presentMode = presentMode;

	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);
	uint32_t familyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily == indices.presentFamily) {
		// exclusive, ie each image is only owned by one queue family
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}
	else {
		// concurrent, ie images can be used between multiple queue families
		// TODO - make more efficient by explicitly transferring images between families when needed
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = familyIndices;
	}

	createInfo.preTransform = swapCapabilities.currentTransform; // no special transformations
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // alpha bit is ignored by compositor
	createInfo.clipped = VK_TRUE; // cull pixels that are covered by other windows

	createInfo.oldSwapchain = VK_NULL_HANDLE; // handle for last swapchain

	swapchain = device.createSwapchainKHR(createInfo);

	// print num of images (device could have chosen different number than imageCount)
	std::cout << "Swapchain images: " << device.getSwapchainImagesKHR(swapchain).size();
	std::cout << " (" << swapCapabilities.minImageCount << " to ";
	std::cout << swapCapabilities.maxImageCount << ")" << std::endl;

	std::cout << std::endl; // newline to seperate prints
}

void Primrose::createRenderPass() {
	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainImageFormat;
	colorAttachment.samples = vk::SampleCountFlagBits::e1; // multisampling count

	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear; // what to do with the framebuffer data before rendering
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore; // what to do with the framebuffer data after rendering

	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

	colorAttachment.initialLayout = vk::ImageLayout::eUndefined; // dont care what layout it has before rendering
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR; // want the image to be presentable after rendering

	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics; // graphics subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; // the outputs of the frag shader map to the buffers in this array
	// eg layout(location = 0) refers to pColorAttachments[0]

	vk::SubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to first implicit subpass
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	// ^ all the commands in this subpass must reach the color attachment stage before the destination subpass can begin
	dependency.srcAccessMask = vk::AccessFlagBits::eNone;

	dependency.dstSubpass = 0; // refers to our subpass
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	// ^ commands cannot enter this stage until all the commands from the external subpass have reached this stage
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite; // specifically don't allow writing the colors

	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	renderPass = device.createRenderPass(renderPassInfo);
}

void Primrose::createSwapchainFrames() {
	// fetch images from swapchain
	std::vector<vk::Image> images = device.getSwapchainImagesKHR(swapchain);
	swapchainFrames.resize(images.size());

	// framebuffer settings
	vk::FramebufferCreateInfo framebufferInfo{};
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.width = swapchainExtent.width;
	framebufferInfo.height = swapchainExtent.height;
	framebufferInfo.layers = 1;

	for (size_t i = 0; i < swapchainFrames.size(); ++i) {
		// image
		swapchainFrames[i].image = images[i];

		// image view
		swapchainFrames[i].imageView = createImageView(swapchainFrames[i].image, swapchainImageFormat);

		// framebuffer
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &swapchainFrames[i].imageView;

		swapchainFrames[i].framebuffer = device.createFramebuffer(framebufferInfo);
	}
}



void Primrose::createUIPipeline() {
	// shader modules
	vk::ShaderModule vertModule = createShaderModule(reinterpret_cast<uint32_t*>(uiVertSpvData), uiVertSpvSize);
	vk::ShaderModule fragModule = createShaderModule(reinterpret_cast<uint32_t*>(uiFragSpvData), uiFragSpvSize);

	// vertex input
	auto bindingDesc = UIVertex::getBindingDescription();
	auto attributeDescs = UIVertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.vertexBindingDescriptionCount = 1;
	vertInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertInputInfo.vertexAttributeDescriptionCount = attributeDescs.size();
	vertInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.topology = vk::PrimitiveTopology::eTriangleList; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	createGraphicsPipelineLayout(&uiPipelineLayout, &uiDescriptorLayout);
	createGraphicsPipeline(vertModule, fragModule, vertInputInfo, assemblyInfo, uiPipelineLayout, &uiPipeline);

	// cleanup
	device.destroyShaderModule(vertModule);
	device.destroyShaderModule(fragModule);
}

void Primrose::createDitherTexture() {
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

	marchImageView = createImageView(marchTexture, vk::Format::eR8G8B8A8Srgb);

	vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.magFilter = vk::Filter::eNearest; // when magnified, ie one fragment corresponds to multiple image pixels
	samplerInfo.minFilter = vk::Filter::eLinear; // when minified, ie one fragment is between image pixels
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // true to sample using width/height coords instead of 0-1 range
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
	samplerInfo.mipLodBias = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	marchSampler = device.createSampler(samplerInfo);
}



void Primrose::createCommandPool() {
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	vk::CommandPoolCreateInfo info{};
	info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // allow rerecording commands
	info.queueFamilyIndex = indices.graphicsFamily.value();

	commandPool = device.createCommandPool(info);
}

void Primrose::createFramesInFlight() {
	// resize vector
	framesInFlight.resize(MAX_FRAMES_IN_FLIGHT);

	// create objects for each frame
	std::cout << "Creating frames in flight" << std::endl;
	for (auto& frame : framesInFlight) {
		// create command buffers
		vk::CommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = vk::CommandBufferLevel::ePrimary; // primary can be submitted directly to gpu
		commandBufferInfo.commandBufferCount = 1;

		frame.commandBuffer = device.allocateCommandBuffers(commandBufferInfo)[0];

		// create semaphores
		vk::SemaphoreCreateInfo semaInfo{};
		frame.imageAvailableSemaphore = device.createSemaphore(semaInfo);
		frame.renderFinishedSemaphore = device.createSemaphore(semaInfo);

		// create fence
		vk::FenceCreateInfo fenceInfo{};
		fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled; // default signaled on creation
		frame.inFlightFence = device.createFence(fenceInfo);

		// create uniform buffers
		createBuffer(sizeof(MarchUniforms), vk::BufferUsageFlagBits::eUniformBuffer,
//			vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
			vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible
			| vk::MemoryPropertyFlagBits::eHostCoherent, // smart access memory (256 mb)
			&frame.uniformBuffer, &frame.uniformBufferMemory);
	}

	std::cout << std::endl; // newline
}



void Primrose::cleanup() {
	// vulkan destruction
	for (const auto& frame : framesInFlight) {
		device.destroySemaphore(frame.imageAvailableSemaphore);
		device.destroySemaphore(frame.renderFinishedSemaphore);
		device.destroyFence(frame.inFlightFence);

		device.destroyBuffer(frame.uniformBuffer);
		device.freeMemory(frame.uniformBufferMemory);
	}

	uiScene.clear();

	device.destroyImage(marchTexture);
	device.freeMemory(marchTextureMemory);
	device.destroyImageView(marchImageView);
	device.destroySampler(marchSampler);

	device.destroyCommandPool(commandPool);

	device.destroyPipeline(uiPipeline);
	device.destroyPipelineLayout(uiPipelineLayout);
	device.destroyDescriptorSetLayout(uiDescriptorLayout);

	device.destroyPipeline(mainPipeline);
	device.destroyPipelineLayout(mainPipelineLayout);
	device.destroyDescriptorSetLayout(mainDescriptorLayout);

	cleanupSwapchain();
	device.destroyRenderPass(renderPass);

	device.destroy();
	instance.destroySurfaceKHR(surface);

	if (Settings::validationEnabled) {
		instance.destroyDebugUtilsMessengerEXT(debugMessenger);
	}

	instance.destroy();

	// glfw destruction
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Primrose::cleanupSwapchain() {
	for (const auto& frame : swapchainFrames) {
		device.destroyFramebuffer(frame.framebuffer);
		device.destroyImageView(frame.imageView);
	}

	device.destroySwapchainKHR(swapchain);
}

void Primrose::recreateSwapchain() {
	if (isWindowMinimized()) {
		windowMinimized = true;
		return;
	}

	std::cout << "recreating swapchain might mess up screen idk" << std::endl;
	std::cout << "Recreating Swapchain                          " << std::endl;

	device.waitIdle();

	cleanupSwapchain();

	createSwapchain();
	createSwapchainFrames();

	//for (auto& frame : framesInFlight) {
	//	frame.uniforms.screenHeight = (float)swapchainExtent.height / (float)swapchainExtent.width;
	//}
}
