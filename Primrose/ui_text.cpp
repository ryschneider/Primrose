#include "ui_text.hpp"

Primrose::UIText::UIText(std::string txt) : UIElement() {
	text = std::move(txt);
}
