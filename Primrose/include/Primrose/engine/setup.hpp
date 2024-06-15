#ifndef PRIMROSE_SETUP_HPP
#define PRIMROSE_SETUP_HPP

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <vector>

namespace Primrose {
	extern vk::Instance instance; // used as global vulkan state
	extern vk::PhysicalDevice physicalDevice; // graphics device
	extern vk::Device device; // logical connection to the physical device

	extern vk::RenderPass renderPass; // render pass with commands used to render a frame

	extern bool rayAcceleration;

	extern vk::AccelerationStructureKHR topStructure;
	extern vk::Buffer topStructureBuffer;
	extern vk::DeviceMemory topStructureMemory;
	extern vk::AccelerationStructureKHR aabbStructure;
	extern vk::Buffer aabbStructureBuffer;
	extern vk::DeviceMemory aabbStructureMemory;

	extern vk::Buffer rayShaderTable;
	extern vk::DeviceMemory rayShaderTableMemory;
	extern vk::StridedDeviceAddressRegionKHR genGroupAddress;
	extern vk::StridedDeviceAddressRegionKHR hitGroupAddress;
	extern vk::StridedDeviceAddressRegionKHR missGroupAddress;
	extern vk::StridedDeviceAddressRegionKHR callableGroupAddress;

	extern vk::Image traceImage;
	extern vk::ImageView traceImageView;
	extern vk::DeviceMemory traceImageMemory;

	extern vk::DescriptorSetLayout mainDescriptorLayout;
	extern vk::PipelineLayout mainPipelineLayout;
	extern vk::Pipeline mainPipeline;

	extern vk::DescriptorSetLayout uiDescriptorLayout;
	extern vk::PipelineLayout uiPipelineLayout;
	extern vk::Pipeline uiPipeline;

	extern GLFWwindow* window;
	extern vk::SurfaceKHR surface;

	extern vk::SwapchainKHR swapchain; // chain of images queued to be presented to display
	struct SwapchainFrame {
		vk::Image image; // gpu image handle
		vk::ImageView imageView; // interface for image
		vk::Framebuffer framebuffer; // render pass target used to draw to image
	};
	extern std::vector<SwapchainFrame> swapchainFrames;
	extern vk::Format swapchainImageFormat; // pixel format used in swapchain
	extern vk::Extent2D swapchainExtent; // resolution of swapchain

	extern int windowWidth;
	extern int windowHeight;

//	extern vk::DescriptorPool descriptorPool;

	extern vk::Image marchTexture;
	extern vk::DeviceMemory marchTextureMemory;
	extern vk::ImageView marchImageView;
	extern vk::Sampler marchSampler;

	struct FrameInFlight {
		vk::CommandBuffer commandBuffer;

		vk::Semaphore imageAvailableSemaphore;
		vk::Semaphore renderFinishedSemaphore;
		vk::Fence inFlightFence;

//		vk::DescriptorSet descriptorSet; // descriptor set for uniforms
		vk::Buffer uniformBuffer; // buffer for ubo
		vk::DeviceMemory uniformBufferMemory; // memory for ubo

		//GlobalUniforms uniforms; // persistent uniform data
	};
	extern std::vector<FrameInFlight> framesInFlight;

	extern vk::CommandPool commandPool;
	extern vk::Queue graphicsQueue; // queue for graphics commands sent to gpu
	extern vk::Queue presentQueue; // queue for present commands sent to gpu

	extern vk::DebugUtilsMessengerEXT debugMessenger; // handles logging validation details

	// application functions
	extern void(*mouseMovementCallback)(float xpos, float ypos, bool refocused);
	extern void(*mouseButtonCallback)(int button, int action);
	extern void(*keyCallback)(int key, int action, int mods);
	extern void(*endCallback)();
	extern void(*renderPassCallback)(vk::CommandBuffer& cmd);
	extern void(*scrollCallback)(float scroll);

	// main functions
	void setup(const char* applicationName, unsigned int applicationVersion);

	// glfw setup
	void initWindow();

	// other glfw
	bool isWindowMinimized();

	// vulkan setup
	void initVulkan();

	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();

	void createSwapchain();
	void createRenderPass();
	void createSwapchainFrames();
	void createTraceImage();
//	void createDescriptorSetLayout();

	void createUIPipeline();

	void createDitherTexture();

	void createCommandPool();
//	void createDescriptorPool();
	void createFramesInFlight();

	void cleanup();

	void cleanupSwapchain();
	void recreateSwapchain();

	// other vulkan
//	void allocateDescriptorSet(vk::DescriptorSet* descSet);

	void createDeviceMemory(vk::MemoryRequirements memReqs, vk::MemoryPropertyFlags properties,
		vk::DeviceMemory* memory, bool deviceAddressFlag = false);
	void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::Buffer* buffer, vk::DeviceMemory* bufferMemory, bool deviceAddressFlag = false);
	void writeToDevice(vk::DeviceMemory memory, const void* data, size_t size, size_t offset = 0);

	void transitionImageLayout(vk::Image image, vk::CommandBuffer cmd,
	vk::ImageLayout oldLayout, vk::AccessFlags oldAccess, vk::PipelineStageFlags oldStage,
		vk::ImageLayout newLayout, vk::AccessFlags newAccess, vk::PipelineStageFlags newStage);
	void transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

	void importTexture(const char* path, vk::Image* image, vk::DeviceMemory* imageMemory, float* aspect = nullptr);

	void imageFromData(void* data, uint32_t width, uint32_t height, vk::Image* image, vk::DeviceMemory* imageMemory);

	vk::ImageView createImageView(vk::Image image, vk::Format format);

	vk::CommandBuffer startSingleTimeCommandBuffer();
	void endSingleTimeCommandBuffer(vk::CommandBuffer cmdBuffer);

	vk::ShaderModule createShaderModule(const uint32_t* code, size_t length);
	vk::ShaderModule compileShaderModule(const char* code);
}

#endif
