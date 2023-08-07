#include "ui/element.hpp"
#include "engine/setup.hpp"
#include "shader_structs.hpp"
#include "state.hpp"

#include <glm/gtx/rotate_vector.hpp>

Primrose::UIElement::UIElement(glm::vec2 pos, glm::vec2 scale, float angle) : pos(pos), scale(scale), angle(angle) {}
Primrose::UIElement::UIElement(glm::vec2 pos, float scale, float angle) : pos(pos), scale(glm::vec3(scale)), angle(angle) {}

Primrose::UIElement::~UIElement() {
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

void Primrose::UIElement::init(UI type) {
	uiType = type;

	updateBuffers();
	createDescriptorSet();
}

void Primrose::UIElement::updateBuffers() {
	// create vertex/index buffers
	std::vector<UIVertex> vertices = genVertices();
	std::vector<uint16_t> indices = genIndices();
	numIndices = (int)indices.size();

	for (auto& v : vertices) {
		// apply transformations
		v.pos *= scale;
        v.pos.y /= uniforms.screenHeight;
		float oldM = v.pos.length();
		v.pos = glm::rotate(v.pos, angle);
//		v.pos = glm::vec2(
//			v.pos.x*cos(angle) + v.pos.y*sin(angle),
//			v.pos.x*sin(angle) - v.pos.y*cos(angle));
		v.pos += pos;
	}

    vkQueueWaitIdle(graphicsQueue); // TODO make updating text more efficient
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	createBuffer(vertexBufferSize, vk::BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vk::MEMORY_PROPERTY_DEVICE_LOCAL_BIT | vk::MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&vertexBuffer, &vertexBufferMemory);
	writeToDevice(vertexBufferMemory, vertices.data(), vertexBufferSize);

	vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	createBuffer(indexBufferSize, vk::BUFFER_USAGE_INDEX_BUFFER_BIT,
		vk::MEMORY_PROPERTY_DEVICE_LOCAL_BIT | vk::MEMORY_PROPERTY_HOST_VISIBLE_BIT | vk::MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&indexBuffer, &indexBufferMemory);
	writeToDevice(indexBufferMemory, indices.data(), indexBufferSize);
}

std::vector<Primrose::UIVertex> Primrose::UIElement::genVertices() {
	return {
		{{0, 0}, {0, 0}},
		{{1, 0}, {1, 0}},
		{{0, 1}, {0, 1}},
		{{1, 1}, {1, 1}}
	};
}

std::vector<uint16_t> Primrose::UIElement::genIndices() {
	return {
		0, 1, 2,
		2, 1, 3
	};
}
