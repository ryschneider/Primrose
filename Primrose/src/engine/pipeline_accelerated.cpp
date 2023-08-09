#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "engine/pipeline_accelerated.hpp"
#include "engine/setup.hpp"
#include "state.hpp"
#include "embed/main_rgen_spv.h"
#include "embed/main_rint_spv.h"
#include "embed/main_rchit_spv.h"
#include "embed/main_rmiss_spv.h"

#include <vulkan/vulkan.hpp>
#include <stdexcept>
#include <iostream>

void Primrose::createAcceleratedDescriptorSetLayout() {
	// uniform binding
	vk::DescriptorSetLayoutBinding uniformBinding{};
	uniformBinding.binding = 0; // binding in shader
	uniformBinding.descriptorType = vk::DescriptorType::eUniformBuffer; // uniform buffer
	uniformBinding.descriptorCount = 1; // numbers of ubos
	uniformBinding.stageFlags = vk::ShaderStageFlagBits::eAnyHitKHR;

	// texture binding
	vk::DescriptorSetLayoutBinding textureBinding{};
	textureBinding.binding = 1; // binding in shader
	textureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler; // uniform buffer
	textureBinding.descriptorCount = 1; // numbers of textures
	textureBinding.pImmutableSamplers = nullptr;
	textureBinding.stageFlags = vk::ShaderStageFlagBits::eAnyHitKHR; // shader stage

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

void Primrose::createAcceleratedPipeline() {
	// pipeline layout
	std::vector<vk::PushConstantRange> pushRanges = {
		vk::PushConstantRange(vk::ShaderStageFlagBits::eAll, 0, 128)}; // 128 bytes is min supported push size
	// TODO change eALl to specific stage ^
	std::vector<vk::DescriptorSetLayout> setLayouts = {mainDescriptorSetLayout};
	vk::PipelineLayoutCreateInfo layoutInfo({}, setLayouts, pushRanges);
	mainPipelineLayout = device.createPipelineLayout(layoutInfo);

	// shader stages info
	vk::PipelineShaderStageCreateInfo rgenInfo({}, vk::ShaderStageFlagBits::eRaygenKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRgenSpvData), mainRgenSpvSize), "main");
	vk::PipelineShaderStageCreateInfo rintInfo({}, vk::ShaderStageFlagBits::eIntersectionKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRintSpvData), mainRintSpvSize), "main");
	vk::PipelineShaderStageCreateInfo rchitInfo({}, vk::ShaderStageFlagBits::eClosestHitKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRchitSpvData), mainRchitSpvSize), "main");
	vk::PipelineShaderStageCreateInfo rmissInfo({}, vk::ShaderStageFlagBits::eMissKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRmissSpvData), mainRmissSpvSize), "main");
	std::vector<vk::PipelineShaderStageCreateInfo> stages = {rgenInfo, rintInfo, rchitInfo, rmissInfo};

	// shader groups info
	vk::RayTracingShaderGroupCreateInfoKHR genGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // gen: stages[0]
	vk::RayTracingShaderGroupCreateInfoKHR hitGroup(vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
		VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, 1); // chit: stages[2], int: stages[1]
	vk::RayTracingShaderGroupCreateInfoKHR missGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // miss: stages[3]
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups = {genGroup, hitGroup, missGroup};

	// get rt pipeline properties
	auto rtProperties = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

	// pipeline create info
	vk::RayTracingPipelineCreateInfoKHR pipelineInfo(
		vk::PipelineCreateFlagBits::eRayTracingNoNullIntersectionShadersKHR // TODO maybe add skip triangles flag?
		| vk::PipelineCreateFlagBits::eRayTracingNoNullClosestHitShadersKHR
		| vk::PipelineCreateFlagBits::eRayTracingNoNullMissShadersKHR,
		stages, groups,
		rtProperties.maxRayRecursionDepth, // use device max recursion for max ray recursions
		nullptr, nullptr, nullptr, mainPipelineLayout);

	auto res = device.createRayTracingPipelineKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, pipelineInfo);
	if (res.result != vk::Result::eSuccess) throw std::runtime_error("failed to create accelerated pipeline");
	mainPipeline = res.value;

	device.destroyShaderModule(rgenInfo.module);
	device.destroyShaderModule(rintInfo.module);
	device.destroyShaderModule(rchitInfo.module);
	device.destroyShaderModule(rmissInfo.module);
}
