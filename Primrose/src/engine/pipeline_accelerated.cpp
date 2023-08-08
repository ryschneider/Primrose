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

void Primrose::createAcceleratedPipeline() {
	// pipeline layout
	std::vector<vk::PushConstantRange> pushRanges = {
		vk::PushConstantRange(vk::ShaderStageFlagBits::eAll, 0, 128)}; // 128 bytes is min supported push size
	// TODO change eALl to specific stage ^
	std::vector<vk::DescriptorSetLayout> setLayouts = {descriptorSetLayout};
	vk::PipelineLayoutCreateInfo layoutInfo({}, setLayouts, pushRanges);
	acceleratedPipelineLayout = static_cast<vk::Device>(device).createPipelineLayout(layoutInfo);

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
	auto rtProperties = static_cast<vk::PhysicalDevice>(physicalDevice).getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

	// pipeline create info
	vk::RayTracingPipelineCreateInfoKHR pipelineInfo(
		vk::PipelineCreateFlagBits::eRayTracingNoNullIntersectionShadersKHR // TODO maybe add skip triangles flag?
		| vk::PipelineCreateFlagBits::eRayTracingNoNullClosestHitShadersKHR
		| vk::PipelineCreateFlagBits::eRayTracingNoNullMissShadersKHR,
		stages, groups,
		rtProperties.maxRayRecursionDepth, // use device max recursion for max ray recursions
		nullptr, nullptr, nullptr, acceleratedPipelineLayout);

	vk::DispatchLoaderDynamic dld(instance, vkGetInstanceProcAddr); // TODO make default dispatcher
	auto res = static_cast<vk::Device>(device).createRayTracingPipelineKHR(
		VK_NULL_HANDLE, VK_NULL_HANDLE, pipelineInfo, nullptr, dld);

	if (res.result == vk::Result::eSuccess) {
		acceleratedPipeline = res.value;
	} else {
		std::cout << res.result << std::endl;
		throw std::runtime_error("failed to create accelerated pipeline");
	}

	device.destroyShaderModule(rgenInfo.module);
	device.destroyShaderModule(rintInfo.module);
	device.destroyShaderModule(rchitInfo.module);
	device.destroyShaderModule(rmissInfo.module);
}
