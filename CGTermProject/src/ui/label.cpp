#include "uiscreen.h"
#include "label.h"
#include "font.h"


bool Label::render(std::vector<UIVertex>& vertices)
{
	FontRenderer* fontRenderer = UIScreen::getFontRenderer();
	fontRenderer->renderText(text, x, y, color, fontPoint, centered);
	return false;
}

void Label::setText(std::wstring text)
{
	this->text = text;
}

void Label::setColor(int color)
{
	this->color = color;
}

void Label::setUseCentered(bool centered)
{
	this->centered = centered;
}

void Label::setTextPoint(float point)
{
	this->fontPoint = point;
}