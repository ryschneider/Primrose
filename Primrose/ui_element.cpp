#include "ui_element.hpp"

#include <utility>
#include "engine.hpp"

Primrose::UIElement::UIElement() = default;
Primrose::UIElement::~UIElement() {
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	vkDestroyImage(device, texture, nullptr);
	vkFreeMemory(device, textureMemory, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroySampler(device, sampler, nullptr);
}

Primrose::UIText::UIText(std::string txt) : UIElement() {
	text = std::move(txt);
}
