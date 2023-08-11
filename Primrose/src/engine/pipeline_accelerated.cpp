#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "engine/pipeline_accelerated.hpp"
#include "engine/setup.hpp"
#include "state.hpp"
#include "log.hpp"
#include "embed/main_rgen_spv.h"
#include "embed/main_rint_spv.h"
#include "embed/main_rchit_spv.h"
#include "embed/main_rmiss_spv.h"

#include <vulkan/vulkan.hpp>
#include <stdexcept>
#include <iostream>

const static int genGroupIndex = 0;
const static int hitGroupIndex = 1;
const static int missGroupIndex = 2;

void Primrose::createAcceleratedPipelineLayout() {
	log("Creating accelerated pipeline layout");

	std::vector<vk::PushConstantRange> pushRanges = {
		vk::PushConstantRange(vk::ShaderStageFlagBits::eRaygenKHR, 0, 128) // 128 bytes is min supported push size
	};

	std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eAccelerationStructureKHR, 1,
			vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1,
			vk::ShaderStageFlagBits::eRaygenKHR)
	};
	// TODO change eCompute to specific stage ^

	vk::DescriptorSetLayoutCreateInfo descLayoutInfo(
		vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR, bindings);
	mainDescriptorLayout = device.createDescriptorSetLayout(descLayoutInfo);

	vk::PipelineLayoutCreateInfo layoutInfo({}, mainDescriptorLayout, pushRanges);
	mainPipelineLayout = device.createPipelineLayout(layoutInfo);
}

void Primrose::createAcceleratedPipeline() {
	log("Creating accelerated pipeline");

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
	std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> groups;
	groups[genGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // gen: stages[0]
	groups[hitGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
		VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, 1); // chit: stages[2], int: stages[1]
	groups[missGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // miss: stages[3]

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

void Primrose::createShaderTable() {
	log("Creating shader table");

	auto rtProperties = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

	const int numGroups = 3;
	vk::DeviceSize progSize = rtProperties.shaderGroupBaseAlignment;
	vk::DeviceSize tableSize = progSize * numGroups;

	// create shader table buffer/memory
	Primrose::createBuffer(tableSize,
		vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible
		| vk::MemoryPropertyFlagBits::eHostCoherent,
		&rayShaderTable, &rayShaderTableMemory, true);

	// get handles to shader groups
	std::vector<uint8_t> handles = device.getRayTracingShaderGroupHandlesKHR<uint8_t>(mainPipeline,
		0, numGroups, tableSize);

	// copy handles to the shader table memory
	uint8_t* dst = reinterpret_cast<uint8_t*>(device.mapMemory(rayShaderTableMemory, 0, tableSize));
	for (int i = 0; i < numGroups; ++i) {
		uint8_t* src = handles.data() + (i * rtProperties.shaderGroupHandleSize);
		memcpy(dst, src, rtProperties.shaderGroupHandleSize);

		dst += progSize;
	}
	device.unmapMemory(rayShaderTableMemory);

	// get device addresses for shader groups
	vk::BufferDeviceAddressInfo tableBufferInfo(rayShaderTable);
	vk::DeviceAddress shaderTableAddress = device.getBufferAddress(tableBufferInfo);

	genGroupAddress = vk::StridedDeviceAddressRegionKHR(
		shaderTableAddress + genGroupIndex * progSize, progSize, progSize);
	hitGroupAddress = vk::StridedDeviceAddressRegionKHR(
		shaderTableAddress + hitGroupIndex * progSize, progSize, progSize);
	missGroupAddress = vk::StridedDeviceAddressRegionKHR(
		shaderTableAddress + missGroupIndex * progSize, progSize, progSize);
	callableGroupAddress = vk::StridedDeviceAddressRegionKHR();
}
