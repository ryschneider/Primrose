#include <stdexcept>
#include "ui_panel.hpp"
#include "engine.hpp"

void Primrose::UIPanel::init(glm::vec3 col) { init(glm::vec4(col, 1)); }

void Primrose::UIPanel::init(glm::vec4 col) { init(col, col); }

void Primrose::UIPanel::init(glm::vec4 col, glm::vec4 borderCol, float stroke) {
	push.color = col;
	push.borderColor = borderCol;
	push.borderStroke = stroke;

	pPush = &push;
	pushSize = sizeof(push);

	UIElement::init(UI_NULL);
}

void Primrose::UIPanel::createDescriptorSet() {}
