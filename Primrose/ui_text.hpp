#ifndef PRIMROSE_UI_TEXT_HPP
#define PRIMROSE_UI_TEXT_HPP

#include "ui_element.hpp"

namespace Primrose {
	class UIText: public UIElement {
	public:
		explicit UIText(std::string text);

	private:
		std::string text;
	};
}

#endif
