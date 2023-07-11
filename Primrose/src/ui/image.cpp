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

	imageView = createImageView(texture, VK_FORMAT_R8G8B8A8_SRGB);

	createSampler();

	UIElement::init(UI_IMAGE);

	std::cout << std::endl; // newline
}

void Primrose::UIImage::createDescriptorSet() {
	allocateDescriptorSet(&descriptorSet);

	// write to descriptor set
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet; // which set to update
	descriptorWrite.dstArrayElement = 0; // not using an array, 0
	descriptorWrite.descriptorCount = 1;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = imageView;
	imageInfo.sampler = sampler;
	descriptorWrite.dstBinding = 1; // which binding index
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr); // apply write
}

void Primrose::UIImage::createSampler() {
	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR; // when magnified, ie one fragment corresponds to multiple image pixels
	samplerInfo.minFilter = VK_FILTER_LINEAR; // when minified, ie one fragment is between image pixels
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_TRUE; // TODO add to performance settings
	samplerInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // true to sample using width/height coords instead of 0-1 range
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler");
	}
}

std::vector<UIVertex> Primrose::UIImage::genVertices() {
	std::vector<UIVertex> verts = UIElement::genVertices();
	for (auto& v : verts) {
		v.pos.x *= aspect;
	}
	return verts;
}
