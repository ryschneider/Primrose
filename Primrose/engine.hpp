#ifndef PRIMROSE_ENGINE_HPP
#define PRIMROSE_ENGINE_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

namespace Primrose {
	extern VkInstance instance; // used as global vulkan state
	extern VkPhysicalDevice physicalDevice; // graphics device
	extern VkDevice device; // logical connection to the physical device

	extern VkRenderPass renderPass; // render pass with commands used to render a frame
	extern VkDescriptorSetLayout descriptorSetLayout; // layout for shader uniforms
	extern VkPipelineLayout pipelineLayout; // graphics pipeline layout

	extern VkPipeline marchPipeline; // graphics pipeline

	extern VkPipeline uiPipeline; // graphics pipeline
	extern VkBuffer uiVertexBuffer;
	extern VkDeviceMemory uiVertexBufferMemory;

	extern GLFWwindow* window;
	extern VkSurfaceKHR surface;

	extern VkSwapchainKHR swapchain; // chain of images queued to be presented to display
	struct SwapchainFrame {
		VkImage image; // gpu image handle
		VkImageView imageView; // interface for image
		VkFramebuffer framebuffer; // render pass target used to draw to image
	};
	extern std::vector<SwapchainFrame> swapchainFrames;
	extern VkFormat swapchainImageFormat; // pixel format used in swapchain
	extern VkExtent2D swapchainExtent; // resolution of swapchain

	extern VkDescriptorPool descriptorPool;

	struct FrameInFlight {
		VkCommandBuffer commandBuffer;

		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;
		VkFence inFlightFence;

		VkDescriptorSet descriptorSet; // descriptor set for uniforms
		VkBuffer uniformBuffer; // buffer for ubo
		VkDeviceMemory uniformBufferMemory; // memory for ubo

		//GlobalUniforms uniforms; // persistent uniform data
	};
	extern std::vector<FrameInFlight> framesInFlight;

	extern VkCommandPool commandPool;
	extern VkQueue graphicsQueue; // queue for graphics commands sent to gpu
	extern VkQueue presentQueue; // queue for present commands sent to gpu

	extern VkDebugUtilsMessengerEXT debugMessenger; // handles logging validation details

	// application functions
	extern void(*mouseMovementCallback)(float xpos, float ypos, bool refocused);

	// main functions
	void setup(const char* appName, unsigned int appVer);

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
	void createDescriptorSetLayout();

	void createPipelineLayout();
	VkPipeline createPipeline(VkShaderModule vertModule, VkShaderModule fragModule,
		VkPipelineVertexInputStateCreateInfo vertInputInfo,
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo);
	void createMarchPipeline();
	void createUIPipeline();
	void createUIVertexBuffer();
	void createPipelines();

	void createCommandPool();
	void createDescriptorPool();
	void createFramesInFlight();

	void cleanupSwapchain();
	void recreateSwapchain();

	void cleanup();

	// other vulkan
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
					  VkMemoryPropertyFlags properties, VkBuffer& buffer,
					  VkDeviceMemory& bufferMemory);
	void writeToDevice(VkDeviceMemory memory, void* data, size_t size);

	VkShaderModule createShaderModule(const uint32_t* code, size_t length);
}

#endif
