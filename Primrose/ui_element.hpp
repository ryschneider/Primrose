#ifndef PRIMROSE_UI_ELEMENT_HPP
#define PRIMROSE_UI_ELEMENT_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace Primrose {
	class UIElement {
	public:
		UIElement();
		~UIElement();

		VkBuffer vertexBuffer{};
		VkBuffer indexBuffer{};

	protected:
		VkDeviceMemory vertexBufferMemory{};
		VkDeviceMemory indexBufferMemory{};

		VkImage texture{};
		VkDeviceMemory textureMemory{};
		VkImageView imageView{};
		VkSampler sampler{};
	};

	class UIText: public UIElement {
	public:
		UIText(std::string text);

		std::string text;
	};
}

#endif
