#pragma once
#include "ui/uiscreen.h"

class ScreenMenu : public UIScreen
{
public:
	virtual void init();
	virtual void onButtonClick(int id);
};