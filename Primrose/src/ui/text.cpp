#include "ui/text.hpp"
#include "engine/setup.hpp"
#include "state.hpp"

#include <stdexcept>
#include <map>
#include <iostream>

const std::string Primrose::UIText::NUMERIC = std::string("1234567890");
const std::string Primrose::UIText::UPPERCASE = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
const std::string Primrose::UIText::LOWERCASE = std::string("abcdefghijklmnopqrstuvwxyz");
const std::string Primrose::UIText::ALPHANUMERIC = NUMERIC + UPPERCASE + LOWERCASE;

Primrose::UIText::~UIText() {
	vkDestroyImage(device, texture, nullptr);
	vkFreeMemory(device, textureMemory, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroySampler(device, sampler, nullptr);
}

void Primrose::UIText::init(const char* fontPath, int fntSize) {
	std::cout << "Creating UIText from path: " << fontPath << std::endl;

	fontSize = fntSize;
	if (alphabet == "") alphabet = text;

	loadAlphabet(fontPath);

	UIElement::init(UI::TEXT);

	std::cout << std::endl; // newline
}

void Primrose::UIText::createDescriptorSet() {
	allocateDescriptorSet(&descriptorSet);

	// write to descriptor set
	vk::WriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = vk::STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet; // which set to update
	descriptorWrite.dstArrayElement = 0; // not using an array, 0
	descriptorWrite.descriptorCount = 1;

	vk::DescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = vk::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = imageView;
	imageInfo.sampler = sampler;
	descriptorWrite.dstBinding = 1; // which binding index
	descriptorWrite.descriptorType = vk::DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr); // apply write
}

void Primrose::UIText::loadAlphabet(const char* fontPath) {
	FT_Library ft;
	if (FT_Init_FreeType(&ft)) {
		throw std::runtime_error("could not initialise freetype library");
	}

	FT_Face face;
	if (FT_New_Face(ft, fontPath, 0, &face)) {
		throw std::runtime_error(std::string("could not generateUniforms font: ").append(fontPath));
	}

	FT_Set_Pixel_Sizes(face, 0, fontSize);

	textureWidth = 0;
	textureHeight = 0;
	alphabet += ' '; // always generateUniforms space since it uses zero data in texture
	for (const char& c : alphabet) {
		if (characters.find(c) != characters.end()) continue; // skip already loaded chars
		CharTexture charTexture;

		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			throw std::runtime_error(std::string("failed to generateUniforms character: '") + c + std::string("'"));
		}
		const auto& bitmap = face->glyph->bitmap;

		// texture coordinates
		charTexture.texX = textureWidth;
		charTexture.width = bitmap.width;
		charTexture.height = bitmap.rows;

		textureWidth += charTexture.width;
		if (charTexture.height > textureHeight) textureHeight = charTexture.height;

		// uv coordinates
		charTexture.bearingX = face->glyph->bitmap_left;
		charTexture.bearingY = face->glyph->bitmap_top;
		charTexture.advance = (float)face->glyph->advance.x / 64.f;

		// skip empty characters like space
		if (bitmap.width == 0 || bitmap.rows == 0) {
			characters[c] = charTexture;
			continue;
		};

		// create staging buffer
		vk::DeviceSize dataSize = bitmap.width * bitmap.rows;
		createBuffer(dataSize, vk::BUFFER_USAGE_TRANSFER_SRC_BIT,
			vk::MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&charTexture.buffer, &charTexture.bufferMemory);
		writeToDevice(charTexture.bufferMemory, bitmap.buffer, dataSize);

		characters[c] = charTexture;
	}

	// create image
	vk::ImageCreateInfo imageInfo{};
	imageInfo.sType = vk::STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = vk::IMAGE_TYPE_2D;
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = vk::FORMAT_R8_SRGB;
	imageInfo.tiling = vk::IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = vk::IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = vk::IMAGE_USAGE_TRANSFER_DST_BIT | vk::IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = vk::SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = vk::SAMPLE_COUNT_1_BIT;
	imageInfo.extent = { (uint32_t)textureWidth, (uint32_t)textureHeight, 1 };

	if (vkCreateImage(device, &imageInfo, nullptr, &texture) != vk::SUCCESS) {
		throw std::runtime_error("failed to create character bitmap image");
	}

	// create & bind image memory
	vk::MemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, texture, &memReqs);
	createDeviceMemory(memReqs, vk::MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureMemory);
	vkBindImageMemory(device, texture, textureMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(texture, vk::FORMAT_R8_SRGB,
		vk::IMAGE_LAYOUT_UNDEFINED, vk::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy data to image
	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();

	for (const auto& pair : characters) {
		const auto& tex = pair.second;
		if (tex.width == 0 || tex.height == 0) continue;

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // 0 to use region.imageExtent
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = vk::IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {tex.texX, 0, 0 };
		region.imageExtent = { tex.width, tex.height, 1 };

		vkCmdCopyBufferToImage(cmdBuffer, pair.second.buffer,
			texture, vk::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}

	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(texture, vk::FORMAT_R8_SRGB,
		vk::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// cleanup character texture buffers
	for (const auto& pair : characters) {
		if (pair.second.width == 0 || pair.second.height == 0) continue;
		vkDestroyBuffer(device, pair.second.buffer, nullptr);
		vkFreeMemory(device, pair.second.bufferMemory, nullptr);
	}

	// create image view
	imageView = createImageView(texture, vk::FORMAT_R8_SRGB);

	// create sampler
	vk::PhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.sType = vk::STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = vk::FILTER_LINEAR; // when magnified, ie one fragment corresponds to multiple image pixels
	samplerInfo.minFilter = vk::FILTER_LINEAR; // when minified, ie one fragment is between image pixels
	samplerInfo.addressModeU = vk::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = vk::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = vk::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = vk::TRUE; // TODO add to performance settings
	samplerInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; // true to sample using width/height coords instead of 0-1 range
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = vk::COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = vk::SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != vk::SUCCESS) {
		throw std::runtime_error("failed to create texture sampler");
	}
}

std::vector<Primrose::UIVertex> Primrose::UIText::genVertices() {
	const float textScale = 1 / 400.f;

	std::vector<UIVertex> vertices;
	float originX = 0; // in px
	for (const char& c : text) {
		if (characters.find(c) == characters.end()) {
			throw std::runtime_error(std::string("character '") + c + std::string("' not loaded in alphabet"));
		}
		CharTexture tex = characters[c];

		// world coords (in px)
		float posLeft = originX + tex.bearingX;
		float posTop = 0 - tex.bearingY;
		float posRight = posLeft + tex.width;
		float posBottom = posTop + tex.height;

		// uv coords
		float uvLeft = (float)tex.texX / (float)textureWidth;
		float uvRight = (float)(tex.texX + tex.width) / (float)textureWidth;
		float uvBottom = (float)tex.height / (float)textureHeight;

		originX += tex.advance;
		if (tex.width == 0 || tex.height == 0) continue; // don't render empty characters like space

		vertices.push_back({{ posLeft, posTop }, { uvLeft, 0 }});
		vertices.push_back({{ posLeft, posBottom }, { uvLeft, uvBottom }});
		vertices.push_back({{ posRight, posTop }, { uvRight, 0 }});
		vertices.push_back({{ posRight, posBottom }, { uvRight, uvBottom }});
	}

	for (auto& v : vertices) {
		v.pos.x *= textScale;
		v.pos.y *= textScale * uniforms.screenHeight;
	}

	return vertices;
}

std::vector<uint16_t> Primrose::UIText::genIndices() {
	std::vector<uint16_t> indices;
	int offset = 0;
	for (const char& c : text) {
		indices.insert(indices.end(), { (uint16_t)(offset+0), (uint16_t)(offset+1), (uint16_t)(offset+2) });
		indices.insert(indices.end(), { (uint16_t)(offset+2), (uint16_t)(offset+1), (uint16_t)(offset+3) });
		offset += 4;
	}

	return indices;
}
