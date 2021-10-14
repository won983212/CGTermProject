#include <iostream>
#include <math.h>
#include "gfx/window.h"
#include "scene.h"

using namespace commoncg;

Scene scene;
glm::vec2 lastMouse(0, 0);
Window wnd;

void init()
{
    scene.init();
	glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    scene.render();
}

void reshape(int w, int h)
{
    glutReshapeWindow(SCREEN_WIDTH, SCREEN_HEIGHT);
}

void drag(int x, int y)
{
    glm::vec2 mouse = glm::vec2(x, y);
    glm::vec2 delta = mouse - lastMouse;
    scene.mouseDrag(x, y, delta.x, delta.y);
    lastMouse.x = x;
    lastMouse.y = y;
}

void mouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        lastMouse.x = x;
        lastMouse.y = y;
    }
    scene.mouse(button, state, x, y);
}

void mousemove(int x, int y)
{
    scene.mouseMove(x, y);
}

void wheel(int button, int state, int x, int y)
{
    scene.mouseWheel(button, state, x, y);
}

void update(float partialTime)
{
    scene.update(partialTime, wnd.getFPS());
}

int main(int argc, char* argv[])
{
    wnd.init("Pocket ball (CG Term Project)", &argc, argv);
	wnd.create(init, render, true);
    wnd.setMouseDragFunc(drag);
    wnd.setMouseFunc(mouse);
    wnd.setMouseMotionFunc(mousemove);
    wnd.setMouseWheelFunc(wheel);
    wnd.setReshapeFunc(reshape);
    wnd.setIdleFunc(update);
	wnd.loop();
	return 0;
}