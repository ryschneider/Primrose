#ifndef PRIMROSE_SETUP_HPP
#define PRIMROSE_SETUP_HPP

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

	extern int windowWidth;
	extern int windowHeight;

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
	extern void(*mouseButtonCallback)(int button, int action);
	extern void(*keyCallback)(int key, int mods, bool pressed);
	extern void(*endCallback)();
	extern void(*renderPassCallback)(VkCommandBuffer& cmd);
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
	void createDescriptorSetLayout();

	void createPipelineLayout();
	VkPipeline createPipeline(VkShaderModule vertModule, VkShaderModule fragModule,
		VkPipelineVertexInputStateCreateInfo vertInputInfo,
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo);

	void createMarchPipeline();
	void createUIPipeline();

	void createCommandPool();
	void createDescriptorPool();
	void createFramesInFlight();

	void cleanup();

	void cleanupSwapchain();
	void recreateSwapchain();

	// other vulkan
	void allocateDescriptorSet(VkDescriptorSet* descSet);

	void createDeviceMemory(VkMemoryRequirements memReqs, VkMemoryPropertyFlags properties, VkDeviceMemory* memory);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer* buffer, VkDeviceMemory* bufferMemory);
	void writeToDevice(VkDeviceMemory memory, void* data, size_t size, size_t offset = 0);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void importTexture(const char* path, VkImage* image, VkDeviceMemory* imageMemory, float* aspect = nullptr);

	VkImageView createImageView(VkImage image, VkFormat format);

	VkCommandBuffer startSingleTimeCommandBuffer();
	void endSingleTimeCommandBuffer(VkCommandBuffer cmdBuffer);

	VkShaderModule createShaderModule(const uint32_t* code, size_t length);
}

#endif
