#ifndef PRIMROSE_IMAGE_HPP
#define PRIMROSE_IMAGE_HPP

#include "element.hpp"

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

		vk::Image texture;
		vk::DeviceMemory textureMemory;
		vk::ImageView imageView;
		vk::Sampler sampler;
	};
}

#endif
