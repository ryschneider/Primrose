#include "ui/image.hpp"

#include "engine/setup.hpp"
#include "shader_structs.hpp"

#include <iostream>

Primrose::UIImage::~UIImage() {
	vkDestroyImage(device, texture, nullptr);
	vkFreeMemory(device, textureMemory, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroySampler(device, sampler, nullptr);
}

void Primrose::UIImage::init(const char *imagePath) {
	std::cout << "Creating UIImage from path: " << imagePath << std::endl;

	importTexture(imagePath, &texture, &textureMemory, &aspect);

	imageView = createImageView(texture, vk::Format::eR8G8B8A8Srgb);

	createSampler();

	UIElement::init(UI::IMAGE);

	std::cout << std::endl; // newline
}

void Primrose::UIImage::createDescriptorSet() {
//	allocateDescriptorSet(&descriptorSet);
//
//	// write to descriptor set
//	vk::WriteDescriptorSet descriptorWrite{};
//	descriptorWrite.dstSet = descriptorSet; // which set to update
//	descriptorWrite.dstArrayElement = 0; // not using an array, 0
//	descriptorWrite.descriptorCount = 1;
//
//	vk::DescriptorImageInfo imageInfo{};
//	imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
//	imageInfo.imageView = imageView;
//	imageInfo.sampler = sampler;
//	descriptorWrite.dstBinding = 1; // which binding index
//	descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//	descriptorWrite.pImageInfo = &imageInfo;
//
//	device.updateDescriptorSets({descriptorWrite}, nullptr); // apply write
}

void Primrose::UIImage::createSampler() {
	vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.magFilter = vk::Filter::eLinear; // when magnified, ie one fragment corresponds to multiple image pixels
	samplerInfo.minFilter = vk::Filter::eLinear; // when minified, ie one fragment is between image pixels
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.anisotropyEnable = VK_TRUE; // TODO add to performance settings
	samplerInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // true to sample using width/height coords instead of 0-1 range
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = vk::CompareOp::eAlways;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.mipLodBias = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	sampler = device.createSampler(samplerInfo);
}

std::vector<Primrose::UIVertex> Primrose::UIImage::genVertices() {
	std::vector<UIVertex> verts = UIElement::genVertices();
	for (auto& v : verts) {
		v.pos.x *= aspect;
	}
	return verts;
}
