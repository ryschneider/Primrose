#ifndef PRIMROSE_PIPELINE_RASTER_HPP
#define PRIMROSE_PIPELINE_RASTER_HPP

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

namespace Primrose {
	void createGraphicsPipeline(vk::ShaderModule vertModule, vk::ShaderModule fragModule,
		vk::PipelineVertexInputStateCreateInfo vertInputInfo, vk::PipelineInputAssemblyStateCreateInfo assemblyInfo,
		vk::PipelineLayout *pipelineLayout, vk::Pipeline *pipeline);

	void createRasterDescriptorSetLayout();
	void createRasterPipeline();
}

#endif