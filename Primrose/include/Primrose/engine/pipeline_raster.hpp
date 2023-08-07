#ifndef PRIMROSE_PIPELINE_RASTER_HPP
#define PRIMROSE_PIPELINE_RASTER_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Primrose {
	void createGraphicsPipeline(VkShaderModule vertModule, VkShaderModule fragModule,
		VkPipelineVertexInputStateCreateInfo vertInputInfo, VkPipelineInputAssemblyStateCreateInfo assemblyInfo,
		VkPipelineLayout *pipelineLayout, VkPipeline *pipeline);
	void createRasterPipeline();
}

#endif
