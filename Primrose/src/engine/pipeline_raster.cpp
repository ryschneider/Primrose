#include "engine/pipeline_raster.hpp"
#include "engine/setup.hpp"
#include "state.hpp"
#include "embed/flat_vert_spv.h"
#include "embed/march_frag_spv.h"

#include <vector>

void Primrose::createRasterDescriptorSetLayout() {
	// uniform binding
	vk::DescriptorSetLayoutBinding uniformBinding{};
	uniformBinding.binding = 0; // binding in shader
	uniformBinding.descriptorType = vk::DescriptorType::eUniformBuffer; // uniform buffer
	uniformBinding.descriptorCount = 1; // numbers of ubos
	uniformBinding.stageFlags = vk::ShaderStageFlagBits::eFragment; // shader stage

	// texture binding
	vk::DescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 1; // binding in shader
	textureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler; // uniform buffer
	textureBinding.descriptorCount = 1; // numbers of textures
	textureBinding.pImmutableSamplers = nullptr;
	textureBinding.stageFlags = vk::ShaderStageFlagBits::eFragment; // shader stage

	vk::DescriptorSetLayoutBinding bindings[2] = {uniformBinding, textureBinding};

	// extra flags for descriptor set layout
	vk::DescriptorBindingFlags flags = vk::DescriptorBindingFlagBits::ePartiallyBound;
	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.pBindingFlags = &flags;

	// create descriptor set layout
	vk::DescriptorSetLayoutCreateInfo info{};
	info.bindingCount = 2;
	info.pBindings = bindings;
	info.pNext = &bindingFlags;

	mainDescriptorSetLayout = device.createDescriptorSetLayout(info);
}

void Primrose::createGraphicsPipeline(vk::ShaderModule vertModule, vk::ShaderModule fragModule,
	vk::PipelineVertexInputStateCreateInfo vertInputInfo, vk::PipelineInputAssemblyStateCreateInfo assemblyInfo,
	vk::PipelineLayout *pipelineLayout, vk::Pipeline *pipeline) {

	// pipeline layout
	vk::PushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = 128; // min required size
	pushConstant.stageFlags = vk::ShaderStageFlagBits::eFragment;

	vk::PipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &mainDescriptorSetLayout;
	layoutInfo.pPushConstantRanges = &pushConstant;
	layoutInfo.pushConstantRangeCount = 1;

	*pipelineLayout = device.createPipelineLayout(layoutInfo);

	// shaders
	vk::PipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertStageInfo.pName = "main"; // shader entrypoint
	vertStageInfo.module = vertModule;
	vertStageInfo.pSpecializationInfo = nullptr; // used to set constants for optimization

	vk::PipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragStageInfo.pName = "main";
	fragStageInfo.module = fragModule;
	fragStageInfo.pSpecializationInfo = nullptr;

	vk::PipelineShaderStageCreateInfo shaderStagesInfo[] = {vertStageInfo, fragStageInfo};

	// dynamic states
	// makes viewport size and cull area dynamic at render time, so we dont need to recreate pipeline with every resize
	std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// static viewport/scissor creation
	vk::Viewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(swapchainExtent.width);
	viewport.height = static_cast<float>(swapchainExtent.height);
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D(0, 0);
	scissor.extent = swapchainExtent;

	vk::PipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.depthClampEnable = VK_FALSE; // clamps pixels outside range instead of discarding
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // disables rasterization
	rasterizerInfo.polygonMode = vk::PolygonMode::eFill;
	rasterizerInfo.lineWidth = 1;
	rasterizerInfo.cullMode = vk::CullModeFlagBits::eNone; // dont cull backwards faces
	rasterizerInfo.depthBiasEnable = VK_FALSE; // whether to alter depth values

	// multisampling
	vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sampleShadingEnable = VK_FALSE; // no multisampling
	multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

	// colour blending
	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
										  | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

	vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	// pipeline
	vk::GraphicsPipelineCreateInfo pipelineInfo{};
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
	pipelineInfo.layout = *pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStagesInfo;
	pipelineInfo.subpass = 0;


	auto res = device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);
	if (res.result != vk::Result::eSuccess) throw std::runtime_error("failed to create graphics pipeline");
	*pipeline = res.value;
}

void Primrose::createRasterPipeline() {
	// shader modules
	vk::ShaderModule vertModule = createShaderModule(reinterpret_cast<uint32_t*>(flatVertSpvData), flatVertSpvSize);
	vk::ShaderModule fragModule = createShaderModule(reinterpret_cast<uint32_t*>(marchFragSpvData), marchFragSpvSize);

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vertInputInfo{};
	vertInputInfo.vertexBindingDescriptionCount = 0; // no inputs
	vertInputInfo.vertexAttributeDescriptionCount = 0;

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo{};
	assemblyInfo.topology = vk::PrimitiveTopology::eTriangleList; // one triangle every 3 vertices
	assemblyInfo.primitiveRestartEnable = VK_FALSE;

	// create pipeline
	createGraphicsPipeline(vertModule, fragModule, vertInputInfo, assemblyInfo, &mainPipelineLayout, &mainPipeline);

	// cleanup
	device.destroyShaderModule(vertModule);
	device.destroyShaderModule(fragModule);
}
