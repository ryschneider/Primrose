#ifndef PRIMROSE_UI_IMAGE_HPP
#define PRIMROSE_UI_IMAGE_HPP

#include "ui_element.hpp"

namespace Primrose {
	class UIImage: public UIElement {
	public:
		using UIElement::UIElement;
		~UIImage();

		void init(const char* imagePath);

	private:
		void createDescriptorSet();
		void createSampler();

		std::vector<UIVertex> genVertices();

		float aspect; // width / height

		VkImage texture;
		VkDeviceMemory textureMemory;
		VkImageView imageView;
		VkSampler sampler;
	};
}

#endif
