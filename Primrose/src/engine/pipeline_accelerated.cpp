#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "engine/pipeline_accelerated.hpp"
#include "engine/setup.hpp"
#include "log.hpp"
#include "embed/main_rgen_spv.h"
#include "embed/main_rint_spv.h"
#include "embed/main_rahit_spv.h"
#include "embed/main_rchit_spv.h"
#include "embed/main_rmiss_spv.h"
#include "scene/scene.hpp"

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

	std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eAccelerationStructureKHR, 1,
			vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1,
			vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1,
			vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1,
			vk::ShaderStageFlagBits::eRaygenKHR),
	};
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
	vk::PipelineShaderStageCreateInfo rahitInfo({}, vk::ShaderStageFlagBits::eAnyHitKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRahitSpvData), mainRahitSpvSize), "main");
	vk::PipelineShaderStageCreateInfo rchitInfo({}, vk::ShaderStageFlagBits::eClosestHitKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRchitSpvData), mainRchitSpvSize), "main");
	vk::PipelineShaderStageCreateInfo rmissInfo({}, vk::ShaderStageFlagBits::eMissKHR,
		createShaderModule(reinterpret_cast<uint32_t*>(mainRmissSpvData), mainRmissSpvSize), "main");
	std::vector<vk::PipelineShaderStageCreateInfo> stages = {rgenInfo, rintInfo, rahitInfo, rchitInfo, rmissInfo};

	// shader groups info
	std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> groups;
	groups[genGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // gen: stages[0]
	groups[hitGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
		VK_SHADER_UNUSED_KHR, 3, 2, 1); // chit: stages[3], ahit: stages[2], int: stages[1]
	groups[missGroupIndex] = vk::RayTracingShaderGroupCreateInfoKHR(vk::RayTracingShaderGroupTypeKHR::eGeneral,
		4, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR); // miss: stages[4]

	// get rt pipeline properties
	auto rtProperties = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

	// pipeline create info
	vk::RayTracingPipelineCreateInfoKHR pipelineInfo(
		vk::PipelineCreateFlagBits::eRayTracingNoNullIntersectionShadersKHR
		| vk::PipelineCreateFlagBits::eRayTracingNoNullClosestHitShadersKHR
		| vk::PipelineCreateFlagBits::eRayTracingNoNullMissShadersKHR
		| vk::PipelineCreateFlagBits::eRayTracingNoNullAnyHitShadersKHR,
		stages, groups,
		rtProperties.maxRayRecursionDepth, // use device max recursion for max ray recursions
		nullptr, nullptr, nullptr, mainPipelineLayout);

	auto res = device.createRayTracingPipelineKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, pipelineInfo);
	if (res.result != vk::Result::eSuccess) throw std::runtime_error("failed to create accelerated pipeline");
	mainPipeline = res.value;

	device.destroyShaderModule(rgenInfo.module);
	device.destroyShaderModule(rintInfo.module);
	device.destroyShaderModule(rahitInfo.module);
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

namespace {
	using namespace Primrose;

	static void createBottomAccelerationStructure(std::vector<vk::AabbPositionsKHR> aabbData,
		vk::AccelerationStructureKHR* blas, vk::Buffer* blasBuffer, vk::DeviceMemory* blasMemory) {

		log("Creating bottom-level acceleration structure");

		// store aabb data in device buffer
		vk::Buffer aabbDataBuffer;
		vk::DeviceMemory aabbDataMemory;
		createBuffer(aabbData.size() * sizeof(vk::AabbPositionsKHR), vk::BufferUsageFlagBits::eShaderDeviceAddress
																	 | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&aabbDataBuffer, &aabbDataMemory, true);
		writeToDevice(aabbDataMemory, aabbData.data(), aabbData.size() * sizeof(vk::AabbPositionsKHR));

		// build/create info structures
		vk::AccelerationStructureGeometryKHR geom{};
		geom.geometryType = vk::GeometryTypeKHR::eAabbs;
		geom.geometry.aabbs = vk::AccelerationStructureGeometryAabbsDataKHR(
			vk::DeviceOrHostAddressConstKHR(device.getBufferAddress(vk::BufferDeviceAddressInfo(aabbDataBuffer))),
			sizeof(vk::AabbPositionsKHR));
		geom.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;

		vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geom;

		vk::AccelerationStructureBuildSizesInfoKHR buildSizes = device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, {static_cast<unsigned int>(aabbData.size())});

		createBuffer(buildSizes.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal, blasBuffer, blasMemory);

		vk::AccelerationStructureCreateInfoKHR asInfo{};
		asInfo.buffer = *blasBuffer;
		asInfo.offset = 0;
		asInfo.size = buildSizes.accelerationStructureSize;
		asInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

		*blas = device.createAccelerationStructureKHR(asInfo);

		vk::Buffer scratchBuffer;
		vk::DeviceMemory scratchMemory;
		createBuffer(buildSizes.buildScratchSize,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eDeviceLocal, &scratchBuffer, &scratchMemory, true);
		vk::DeviceAddress scratchAddress = device.getBufferAddress(vk::BufferDeviceAddressInfo(scratchBuffer));

		buildInfo.dstAccelerationStructure = *blas;
		buildInfo.scratchData.deviceAddress = scratchAddress;

		vk::AccelerationStructureBuildRangeInfoKHR buildRange(aabbData.size(), 0, 0, 0);

		// build acceleration structure
		vk::CommandBuffer cmd = startSingleTimeCommandBuffer();
		cmd.buildAccelerationStructuresKHR({buildInfo}, {&buildRange});
		endSingleTimeCommandBuffer(cmd);

		// cleanup
		device.freeMemory(scratchMemory);
		device.destroyBuffer(scratchBuffer);

		device.freeMemory(aabbDataMemory);
		device.destroyBuffer(aabbDataBuffer);
	};

	static void createTopAccelerationStructure(vk::AccelerationStructureKHR blas,
		vk::AccelerationStructureKHR* tlas, vk::Buffer* tlasBuffer, vk::DeviceMemory* tlasMemory) {

		log("Creating top-level acceleration structure");

		// instance data
		vk::DeviceAddress blStructureAddress = device.getAccelerationStructureAddressKHR(
			vk::AccelerationStructureDeviceAddressInfoKHR(blas));

		vk::TransformMatrixKHR identityMatrix{};
		identityMatrix.matrix[0] = vk::ArrayWrapper1D<float, 4>({1, 0, 0, 0});
		identityMatrix.matrix[1] = vk::ArrayWrapper1D<float, 4>({0, 1, 0, 0});
		identityMatrix.matrix[2] = vk::ArrayWrapper1D<float, 4>({0, 0, 1, 0});

		std::vector<vk::AccelerationStructureInstanceKHR> instanceData = {
			vk::AccelerationStructureInstanceKHR(identityMatrix, 0, 0xFF, 0, {}, blStructureAddress)
		};

		vk::Buffer instanceBuffer;
		vk::DeviceMemory instanceMemory;
		createBuffer(instanceData.size() * sizeof(vk::AccelerationStructureInstanceKHR),
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
			| vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&instanceBuffer, &instanceMemory, true);
		writeToDevice(instanceMemory,
			instanceData.data(), instanceData.size() * sizeof(vk::AccelerationStructureInstanceKHR));

		vk::AccelerationStructureGeometryKHR geom{};
		geom.geometryType = vk::GeometryTypeKHR::eInstances;
		geom.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR(VK_FALSE,
			vk::DeviceOrHostAddressConstKHR(device.getBufferAddress(vk::BufferDeviceAddressInfo(instanceBuffer))));
		geom.flags = vk::GeometryFlagBitsKHR::eOpaque;

		// fill build info struct enough to calculate structure size
		vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geom;

		// calculate structure build size
		vk::AccelerationStructureBuildSizesInfoKHR buildSizes = device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, {static_cast<unsigned int>(instanceData.size())});

		// create buffer for structure
		createBuffer(buildSizes.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal, tlasBuffer, tlasMemory);

		// create structure
		vk::AccelerationStructureCreateInfoKHR asInfo{};
		asInfo.buffer = *tlasBuffer;
		asInfo.offset = 0;
		asInfo.size = buildSizes.accelerationStructureSize;
		asInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		*tlas = device.createAccelerationStructureKHR(asInfo);

		// create scratch buffer
		vk::Buffer scratchBuffer;
		vk::DeviceMemory scratchMemory;
		createBuffer(buildSizes.buildScratchSize,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eDeviceLocal, &scratchBuffer, &scratchMemory, true);
		vk::DeviceAddress scratchAddress = device.getBufferAddress(vk::BufferDeviceAddressInfo(scratchBuffer));

		// finish build info structure
		buildInfo.dstAccelerationStructure = *tlas;
		buildInfo.scratchData.deviceAddress = scratchAddress;

		// build acceleration structure
		vk::AccelerationStructureBuildRangeInfoKHR buildRange(instanceData.size(), 0, 0, 0);

		vk::CommandBuffer cmd = startSingleTimeCommandBuffer();
		cmd.buildAccelerationStructuresKHR({buildInfo}, {&buildRange});
		endSingleTimeCommandBuffer(cmd);

		// cleanup
		device.freeMemory(scratchMemory);
		device.destroyBuffer(scratchBuffer);

		device.freeMemory(instanceMemory);
		device.destroyBuffer(instanceBuffer);
	};
}

void Primrose::createAccelerationStructure(Scene& scene) {
	log("Creating acceleration structure");
	std::vector<vk::AabbPositionsKHR> aabbData;
	for (const auto& node : scene.root.getChildren()) {
		auto[l, g] = node->generateAabb();
		aabbData.push_back(vk::AabbPositionsKHR(l.x, l.y, l.z, g.x, g.y, g.z));
	}

	createBottomAccelerationStructure(aabbData, &aabbStructure, &aabbStructureBuffer, &aabbStructureMemory);
	createTopAccelerationStructure(aabbStructure, &topStructure, &topStructureBuffer, &topStructureMemory);
}
