#include "ui_text.hpp"
#include "engine.hpp"

#include <stdexcept>
#include <map>
#include <iostream>

Primrose::UIText::~UIText() {
	vkDestroyImage(device, texture, nullptr);
	vkFreeMemory(device, textureMemory, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroySampler(device, sampler, nullptr);
}

void Primrose::UIText::init(const char* fontPath, int fntSize) {
	std::cout << "Creating UIText from path: " << fontPath << std::endl;

	FT_Library ft;
	if (FT_Init_FreeType(&ft)) {
		throw std::runtime_error("could not initialise freetype library");
	}

	if (FT_New_Face(ft, fontPath, 0, &fontFace)) {
		throw std::runtime_error(std::string("could not load font: ").append(fontPath));
	}

	fontSize = fntSize;

	UIElement::init(UI_TEXT);
}

void Primrose::UIText::createDescriptorSet() {
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

std::map<char, CharTexture> Primrose::UIText::createSampler(int* getWidth, int* getHeight) {
	FT_Set_Pixel_Sizes(fontFace, 0, fontSize);

	std::map<char, CharTexture> characters;
	uint32_t totalWidth = 0;
	uint32_t maxHeight = 0;
	for (const char& c : text) {
		if (characters.find(c) != characters.end()) continue;
		CharTexture charTexture;

		if (FT_Load_Char(fontFace, c, FT_LOAD_RENDER)) {
			throw std::runtime_error(std::string("failed to load character: ") + c);
		}
		const auto& bitmap = fontFace->glyph->bitmap;

		// texture coordinates
		charTexture.texX = totalWidth;
		charTexture.width = bitmap.width;
		charTexture.height = bitmap.rows;

		totalWidth += charTexture.width;
		if (charTexture.height > maxHeight) maxHeight = charTexture.height;

		// uv coordinates
		charTexture.bearingX = fontFace->glyph->bitmap_left;
		charTexture.bearingY = fontFace->glyph->bitmap_top;
		charTexture.advance = fontFace->glyph->advance.x;

		// create staging buffer
		VkDeviceSize dataSize = bitmap.width * bitmap.rows;
		createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&charTexture.buffer, &charTexture.bufferMemory);
		writeToDevice(charTexture.bufferMemory, bitmap.buffer, dataSize);

		characters[c] = charTexture;
	}

	// create image
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = VK_FORMAT_R8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.extent = { totalWidth, maxHeight, 1 };

	if (vkCreateImage(device, &imageInfo, nullptr, &texture) != VK_SUCCESS) {
		throw std::runtime_error("failed to create character bitmap image");
	}

	// create & bind image memory
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, texture, &memReqs);
	createDeviceMemory(memReqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureMemory);
	vkBindImageMemory(device, texture, textureMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(texture, VK_FORMAT_R8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy data to image
	VkCommandBuffer cmdBuffer = startSingleTimeCommandBuffer();

	for (const auto& pair : characters) {
		const auto& tex = pair.second;

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // 0 to use region.imageExtent
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {tex.texX, 0, 0 };
		region.imageExtent = {tex.width, tex.height, 1 };

		vkCmdCopyBufferToImage(cmdBuffer, pair.second.buffer,
			texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}

	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(texture, VK_FORMAT_R8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// cleanup character texture buffers
	for (const auto& pair : characters) {
		vkDestroyBuffer(device, pair.second.buffer, nullptr);
		vkFreeMemory(device, pair.second.bufferMemory, nullptr);
	}

	// create image view
	imageView = createImageView(texture, VK_FORMAT_R8_SRGB);

	// create sampler
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

	*getWidth = totalWidth;
	*getHeight = maxHeight;
	return characters;
}

std::vector<UIVertex> Primrose::UIText::genVertices() {
	int width, height;
	auto characters = createSampler(&width, &height);

	std::vector<UIVertex> vertices;
	float originX = 0;
	for (const char& c : text) {
		CharTexture tex = characters[c];

		float uvLeft = originX;
		float uvTop = 0;
		float uvW = (float)tex.width / (float)width;
		float uvH = (float)tex.height / (float)height;

		float posLeft = originX + tex.bearingX / (float)width;
		float posTop = 0 - tex.bearingY / (float)width;
		float posW = (float)tex.width / (float)width;
		float posH = (float)tex.height / (float)width;

		vertices.push_back({{posLeft, posTop}, {uvLeft, uvTop}});
		vertices.push_back({{posLeft, posTop + posH}, {uvLeft, uvTop + uvH}});
		vertices.push_back({{posLeft + posW, posTop}, {uvLeft + uvW, uvTop}});
		vertices.push_back({{posLeft + posW, posTop + posH}, {uvLeft + uvW, uvTop + uvH}});

		originX += tex.advance * 64 / (float)width;
	}

	return UIElement::genVertices();
}

std::vector<uint16_t> Primrose::UIText::genIndices() {
	std::vector<uint16_t> indices;
	int offset = 0;
	for (const char& c : text) {
		indices.insert(indices.end(), { (uint16_t)offset, (uint16_t)(offset+1), (uint16_t)(offset+2) });
		indices.insert(indices.end(), { (uint16_t)(offset+2), (uint16_t)(offset+1), (uint16_t)(offset+3) });
		offset += 4;
	}

	return indices;
}
