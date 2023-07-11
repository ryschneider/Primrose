#ifndef PRIMROSE_ELEMENT_HPP
#define PRIMROSE_ELEMENT_HPP

#include "../shader_structs.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <string>
#include <memory>

namespace Primrose {
	class UIElement {
	public:
		UIElement(glm::vec2 pos = glm::vec2(0), glm::vec2 scale = glm::vec2(1), float angle = 0);
		UIElement(glm::vec2 pos, float scale, float angle = 0);
		virtual ~UIElement();

		virtual void init(unsigned int uiType);

		bool hide = false;

		unsigned int uiType = UI_NULL;
		void* pPush = nullptr;
		uint32_t pushSize = 0;

		VkBuffer vertexBuffer = VK_NULL_HANDLE;
		VkBuffer indexBuffer = VK_NULL_HANDLE;
		int numIndices;

		VkDescriptorSet descriptorSet;

		glm::vec2 pos;
		glm::vec2 scale;
		float angle; // in radians

		void updateBuffers(); // call after changing transforms

	protected:
		virtual void createDescriptorSet() = 0;

		virtual std::vector<UIVertex> genVertices();
		virtual std::vector<uint16_t> genIndices();

		VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
		VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	};
}

#endif
