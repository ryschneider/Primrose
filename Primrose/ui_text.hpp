#ifndef PRIMROSE_UI_TEXT_HPP
#define PRIMROSE_UI_TEXT_HPP

#include "ui_element.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>

struct CharTexture {
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	// texture coordinates (in px)
	int32_t texX;
	uint32_t width;
	uint32_t height;

	// world coordinates (in px)
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

		std::string alphabet = "";
		const static std::string NUMERIC;
		const static std::string ALPHANUMERIC;
		const static std::string LOWERCASE;
		const static std::string UPPERCASE;

		enum TextOrigin {
			BottomLeft,
			Center,
			TopLeft
		};
		TextOrigin origin = BottomLeft;

		std::string text;
		int fontSize;

	private:
		void loadAlphabet(const char* fontPath);
		void createDescriptorSet();

		std::vector<UIVertex> genVertices();
		std::vector<uint16_t> genIndices();

		std::map<char, CharTexture> characters;
		int textureWidth;
		int textureHeight;

		VkImage texture;
		VkDeviceMemory textureMemory;
		VkImageView imageView;
		VkSampler sampler;
	};
}

#endif
