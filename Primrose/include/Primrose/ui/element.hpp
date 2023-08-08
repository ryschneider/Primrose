#ifndef PRIMROSE_ELEMENT_HPP
#define PRIMROSE_ELEMENT_HPP

#include "../shader_structs.hpp"

#include <glm/vec2.hpp>
#include <string>
#include <memory>
#include <vulkan/vulkan.hpp>

namespace Primrose {
	class UIElement {
	public:
		UIElement(glm::vec2 pos = glm::vec2(0), glm::vec2 scale = glm::vec2(1), float angle = 0);
		UIElement(glm::vec2 pos, float scale, float angle = 0);
		virtual ~UIElement();

		void init(UI type);

		bool hide = false;

		UI uiType;
		void* pPush = nullptr;
		uint32_t pushSize = 0;

		vk::Buffer vertexBuffer = VK_NULL_HANDLE;
		vk::Buffer indexBuffer = VK_NULL_HANDLE;
		int numIndices;

		vk::DescriptorSet descriptorSet;

		glm::vec2 pos;
		glm::vec2 scale;
		float angle; // in radians

		void updateBuffers(); // call after changing transforms

	protected:
		virtual void createDescriptorSet() = 0;

		virtual std::vector<UIVertex> genVertices();
		virtual std::vector<uint16_t> genIndices();

		vk::DeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
		vk::DeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	};
}

#endif
