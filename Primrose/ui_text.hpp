#ifndef PRIMROSE_UI_TEXT_HPP
#define PRIMROSE_UI_TEXT_HPP

#include "ui_element.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>

struct CharTexture {
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	// coordinates in texture
	int32_t texX;
	uint32_t width;
	uint32_t height;

	// uv coordinates (in pixels)
	float bearingX;
	float bearingY;
	float advance;
};

namespace Primrose {
	class UIText: public UIElement {
	public:
		using UIElement::UIElement;
		~UIText();

		void init(const char* fontPath, int fontSize = 36);

		std::string text;
		int fontSize;

	private:
		std::map<char, CharTexture> createSampler(int* width = nullptr, int* height = nullptr);
		void createDescriptorSet();

		std::vector<UIVertex> genVertices();
		std::vector<uint16_t> genIndices();

		FT_Face fontFace;

		VkImage texture;
		VkDeviceMemory textureMemory;
		VkImageView imageView;
		VkSampler sampler;
	};
}

#endif
