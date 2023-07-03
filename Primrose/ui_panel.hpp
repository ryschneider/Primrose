#ifndef PRIMROSE_UI_PANEL_HPP
#define PRIMROSE_UI_PANEL_HPP

#include "ui_element.hpp"

namespace Primrose {
	class UIPanel: public UIElement {
	public:
		using UIElement::UIElement;
		void init(glm::vec3 color);
		void init(glm::vec4 color = glm::vec4(1));
		void init(glm::vec4 color, glm::vec4 borderColor, float borderWidth = 0.1);

	private:
		void createDescriptorSet();

		struct {
			alignas(4) unsigned int type = UI_PANEL;

			alignas(16) glm::vec4 color;
			alignas(16) glm::vec4 borderColor;
			alignas(4) float borderStroke;
		} push;
	};
}

#endif
