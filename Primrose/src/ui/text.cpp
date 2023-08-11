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
		charTexture.advance = static_cast<float>(face->glyph->advance.x) / 64.f;

		// skip empty characters like space
		if (bitmap.width == 0 || bitmap.rows == 0) {
			characters[c] = charTexture;
			continue;
		};

		// create staging buffer
		vk::DeviceSize dataSize = bitmap.width * bitmap.rows;
		createBuffer(dataSize, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&charTexture.buffer, &charTexture.bufferMemory);
		writeToDevice(charTexture.bufferMemory, bitmap.buffer, dataSize);

		characters[c] = charTexture;
	}

	// create image
	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.mipLevels = 1; // no mipmapping
	imageInfo.arrayLayers = 1; // not a layered image
	imageInfo.format = vk::Format::eR8Srgb;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.extent = vk::Extent3D(textureWidth, textureHeight, 1);

	texture = device.createImage(imageInfo);

	// create & bind image memory
	vk::MemoryRequirements memReqs = device.getImageMemoryRequirements(texture);
	createDeviceMemory(memReqs, vk::MemoryPropertyFlagBits::eDeviceLocal, &textureMemory);
	vkBindImageMemory(device, texture, textureMemory, 0);

	// transition image layout to optimal destination layout
	transitionImageLayout(texture, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

	// copy data to image
	vk::CommandBuffer cmdBuffer = startSingleTimeCommandBuffer();

	for (const auto& pair : characters) {
		const auto& tex = pair.second;
		if (tex.width == 0 || tex.height == 0) continue;

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // 0 to use region.imageExtent
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D(tex.texX, 0, 0);
		region.imageExtent = vk::Extent3D(tex.width, tex.height, 1);

		cmdBuffer.copyBufferToImage(pair.second.buffer, texture, vk::ImageLayout::eTransferDstOptimal, {region});
	}

	endSingleTimeCommandBuffer(cmdBuffer);

	// transition image layout to optimal shader reading layout
	transitionImageLayout(texture, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	// cleanup character texture buffers
	for (const auto& pair : characters) {
		if (pair.second.width == 0 || pair.second.height == 0) continue;
		vkDestroyBuffer(device, pair.second.buffer, nullptr);
		vkFreeMemory(device, pair.second.bufferMemory, nullptr);
	}

	// create image view
	imageView = createImageView(texture, vk::Format::eR8Srgb);

	// create sampler
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
		float uvLeft = static_cast<float>(tex.texX) / static_cast<float>(textureWidth);
		float uvRight = static_cast<float>(tex.texX + tex.width) / static_cast<float>(textureWidth);
		float uvBottom = static_cast<float>(tex.height) / static_cast<float>(textureHeight);

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
		indices.insert(indices.end(), {
			static_cast<uint16_t>(offset+0), static_cast<uint16_t>(offset+1), static_cast<uint16_t>(offset+2),
			static_cast<uint16_t>(offset+2), static_cast<uint16_t>(offset+1), static_cast<uint16_t>(offset+3)
		});
		offset += 4;
	}

	return indices;
}
