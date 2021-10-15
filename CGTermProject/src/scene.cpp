#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "scene.h"
#include "gfx/texture.h"
#include "model/quad.h"
#include "ui/button.h"
#include "util/sound.h"
#include "linedebugger.h"

using namespace glm;
using namespace commoncg;


const float MAX_CUE_POWER = 1.0f;
const float CUE_POWER_MODIFIER = 12.0f;

const float PRJ_FOV = DEGTORAD(45.0f);
const float PRJ_ASPECT = (float)SCREEN_WIDTH / SCREEN_HEIGHT;
const float PRJ_NEAR = 0.1f;
const float PRJ_FAR = 100.0f;

const model::Material BALL_MATERIAL =
{
	vec4(1.0f, 1.0f, 0.0f, 1.0f),  // albedo
	0.1f,           // metallic
	0.1f,           // roughness
	DEFAULT_AO,     // ao
	0, -1, -1, -1   // texIndex
};


struct StaticWall
{
	glm::vec2 position;
	glm::vec2 normal;
	float size;
};

const float SQ3 = 1.73205080756f;
const float INV_SQ2 = 0.70710678118f;
const float CORNER_REDUCE = TABLE_HOLE_SIZE * INV_SQ2 + TABLE_WALL_THICKNESS;
const float SIDE_CORNER_REDUCE = CORNER_REDUCE - TABLE_HOLE_SIZE * 0.5f;

// line walls.
const StaticWall WALLS[] =
{
	{{0, TABLE_HEIGHT / 2 - TABLE_WALL_THICKNESS}, {0, -1}, TABLE_WIDTH - 2 * CORNER_REDUCE},
	{{0, -TABLE_HEIGHT / 2 + TABLE_WALL_THICKNESS}, {0, 1}, TABLE_WIDTH - 2 * CORNER_REDUCE},
	{{-TABLE_WIDTH / 2 + TABLE_WALL_THICKNESS, TABLE_HEIGHT / 4 - SIDE_CORNER_REDUCE / 2}, {1, 0}, TABLE_HEIGHT / 2 - TABLE_HOLE_SIZE - SIDE_CORNER_REDUCE},
	{{-TABLE_WIDTH / 2 + TABLE_WALL_THICKNESS, -TABLE_HEIGHT / 4 + SIDE_CORNER_REDUCE / 2}, {1, 0}, TABLE_HEIGHT / 2 - TABLE_HOLE_SIZE - SIDE_CORNER_REDUCE},
	{{TABLE_WIDTH / 2 - TABLE_WALL_THICKNESS, TABLE_HEIGHT / 4 - SIDE_CORNER_REDUCE / 2}, {-1, 0}, TABLE_HEIGHT / 2 - TABLE_HOLE_SIZE - SIDE_CORNER_REDUCE},
	{{TABLE_WIDTH / 2 - TABLE_WALL_THICKNESS, -TABLE_HEIGHT / 4 + SIDE_CORNER_REDUCE / 2}, {-1, 0}, TABLE_HEIGHT / 2 - TABLE_HOLE_SIZE - SIDE_CORNER_REDUCE}
};

LineDebugger line;
void Scene::init()
{
	line.init(6);
	for (int i = 0; i < 6; i++)
	{
		vec3 pos = vec3(WALLS[i].position.x, 0.13f, WALLS[i].position.y);
		vec3 right = normalize(cross(vec3(WALLS[i].normal.x, 0, WALLS[i].normal.y), vec3(0, 1, 0)));
		line.points[i * 2] = pos + right * WALLS[i].size * 0.5f;
		line.points[i * 2 + 1] = pos - right * WALLS[i].size * 0.5f;
	}
	line.update();

	// initalize graphic variables
	brdfLUT.loadImage("res/texture/brdf.png");
	uboLight.create();
	uboView.create();

	// prepare ball textures
	for (int i = 0; i < BALL_TEXTURE_COUNT; i++)
		ballTextures[i] = Texture::cacheImage(("res/texture/ball/ball_" + std::to_string(i) + ".png").c_str());

	// model
	modelPoolTable.loadModel("res/model/pooltable.gltf");
	modelCue.loadModel("res/model/cue.gltf");
	modelBall.init(BALL_RADIUS);

	// shader
	shader.addShader("res/shader/pbr.vert", GL_VERTEX_SHADER);
	shader.addShader("res/shader/pbr.frag", GL_FRAGMENT_SHADER);
	shader.load();
	shader.use();
	shader.setUniform("texture_albedo", PBR_TEXTURE_INDEX_ALBEDO);
	shader.setUniform("texture_metallic", PBR_TEXTURE_INDEX_METALLIC);
	shader.setUniform("texture_roughness", PBR_TEXTURE_INDEX_ROUGHNESS);
	shader.setUniform("texture_normal", PBR_TEXTURE_INDEX_NORMAL);
	shader.setUniform("irradianceMap", PBR_TEXTURE_INDEX_IRRADIANCEMAP);
	shader.setUniform("specularMap", PBR_TEXTURE_INDEX_SPECULARMAP);
	shader.setUniform("brdfMap", PBR_TEXTURE_INDEX_BRDFMAP);

	// gui screen
	ui.init();

	// events setup
	ui.setScreenChangedEvent(this);
	table.setBallEvent(this);

	// skybox
	skybox.beginLoad();
	skybox.loadHDRSkybox("res/texture/skybox/skybox.hdr");
	skybox.loadDDSIrradianceMap("res/texture/skybox/irr.dds");
	skybox.loadDDSSpecularMap("res/texture/skybox/env.dds");
	skybox.endLoad();

	// lights
	LightData light;
	light.position = vec4(0.0f, 3.0f, 5.0f, 1.0f);
	light.color = vec4(50);
	lights[0] = light;

	light.position = vec4(0.0f, 3.0f, 0.0f, 1.0f);
	light.color = vec4(50);
	lights[1] = light;

	light.position = vec4(0.0f, 3.0f, -5.0f, 1.0f);
	light.color = vec4(50);
	lights[2] = light;
	updateLight();

	// camera (view)
	cam.getViewMatrix(&view.view);
	view.projection = perspective(PRJ_FOV, PRJ_ASPECT, PRJ_NEAR, PRJ_FAR);
	updateView();
}

void Scene::update(float partialTime, int fps)
{
	if (isBallView)
	{
		vec2 pos = table.getBalls()[0]->position;
		if (pos.x != cam.center.x || pos.y != cam.center.z)
		{
			cam.center = vec3(pos.x, BALL_RADIUS / 2, pos.y);
			cam.update();
		}
	}
	else if (cam.center != vec3(0.0f))
	{
		cam.center = vec3(0.0f);
		cam.update();
	}

	table.update(partialTime);
	ui.fpsLabel->setText(std::wstring(L"FPS: ") + std::to_wstring(fps));
}

void Scene::updateLight()
{
	uboLight.bindBufferRange(UNIFORM_BINDING_LIGHT, 0, sizeof(lights));
	uboLight.buffer(sizeof(lights), &lights);
	uboLight.unbind();
}

void Scene::updateView()
{
	uboView.bindBufferRange(UNIFORM_BINDING_VIEWMAT, 0, sizeof(view));
	uboView.buffer(sizeof(view), &view);
	uboView.unbind();
}

// TODO Back plane이 material적용 없이 rendering됨
void Scene::render()
{
	// update only when cam has changed.
	if (cam.getViewMatrix(&view.view))
		updateView();
	shader.setUniform("camPos", cam.getEyePosition());

	// bind env maps and brdf LUT (lookup texture).
	skybox.bindEnvironmentTextures();
	glActiveTexture(GL_TEXTURE0 + PBR_TEXTURE_INDEX_BRDFMAP);
	brdfLUT.bind();

	// balls
	model::bindMaterial(&BALL_MATERIAL);
	const std::vector<Ball*> balls = table.getBalls();
	for (unsigned int i = 0; i < balls.size(); i++)
	{
		if (!balls[i]->visible)
			continue;

		vec2 pos = balls[i]->position;
		mat4 model = mat4(1.0f);

		model = translate(model, vec3(pos.x, BALL_RADIUS, pos.y));
		model = model * toMat4(balls[i]->rotation);

		glActiveTexture(GL_TEXTURE0 + PBR_TEXTURE_INDEX_ALBEDO);
		ballTextures[i]->bind();
		shader.setUniform("model", model);
		modelBall.draw();
	}
	Texture::unbind();

	// render pool table
	mat4 model = mat4(1.0f);
	model = scale(model, vec3(5.0f));
	model = rotate(model, DEGTORAD(90.0f), vec3(1, 0, 0));
	shader.setUniform("model", model);
	modelPoolTable.draw();

	// render cue
	if (cueTransform.mode != CueMode::INVISIBLE)
	{
		vec2 whiteBallPos = table.getBalls()[0]->position;
		if (cueTransform.position != whiteBallPos)
		{
			cueTransform.position = whiteBallPos;
			cueTransform.update();
		}
		shader.setUniform("model", cueTransform.getModelMatrix());
		modelCue.draw();
	}

	// skybox
	skybox.render(view.view);

	// gui
	ui.render();

	line.draw();
}

MouseRay Scene::calculateMouseRay(int mouseX, int mouseY)
{
	MouseRay ray;

	float sx = mouseX * 2.0f / SCREEN_WIDTH - 1.0f; // to -1 ~ 1
	float sy = mouseY * 2.0f / SCREEN_HEIGHT - 1.0f; // to -1 ~ 1
	float halfHeight = tan(PRJ_FOV / 2) * PRJ_NEAR; // near rectangle height/2

	// fits to near rectangle
	sx *= PRJ_ASPECT * halfHeight;
	sy *= halfHeight;

	ray.position = vec3(cam.getEyePosition());
	ray.direction = -cam.getUp() * sy + cam.getRight() * sx;

	ray.position = ray.position + ray.direction;
	ray.direction += normalize(cam.getFront()) * PRJ_NEAR;
	ray.direction = normalize(ray.direction);

	return ray;
}

void Scene::keyboard(unsigned char key, int x, int y)
{
	if (key == ' ')
	{
		isBallView = !isBallView;
		ui.showMessage(isBallView ? L"공을 중심으로 보기" : L"테이블을 중심으로 보기");
	}
}

void Scene::mouse(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON && ui.getCurrentScreen() == 2)
	{
		if (cueTransform.mode == CueMode::ROTATION)
		{
			cueTransform.mode = CueMode::PUSHING;
		}
		else if (cueTransform.mode == CueMode::PUSHING)
		{
			hitWhiteBall();
			cueTransform.mode = CueMode::ROTATION; // TODO (Debug) 계속 칠수있다
			cueTransform.pushAmount = -BALL_RADIUS;
			cueTransform.update();
		}
	}

	ui.mouse(button, state, x, y);
}

void Scene::mouseDrag(int button, int x, int y, int dx, int dy)
{
	ui.mouseDrag(x, y);
	if (ui.getCurrentScreen() != 2)
		return;

	if (button == GLUT_RIGHT_BUTTON)
	{
		cam.yaw += dx / 8.0f;
		cam.pitch += dy / 8.0f;
		cam.update();
	}
}

void Scene::mouseWheel(int button, int state, int x, int y)
{
	ui.mouseWheel(button, state, x, y);
	if (ui.getCurrentScreen() != 2)
		return;
	
	if (state > 0)
		cam.zoom -= 0.5f;
	else
		cam.zoom += 0.5f;
	cam.update();
}

void Scene::mouseMove(int x, int y)
{
	ui.mouseMove(x, y);

	if (ui.getCurrentScreen() != 2)
		return;

	if (cueTransform.mode == CueMode::INVISIBLE)
		return;

	MouseRay ray = calculateMouseRay(x, y);

	// 바닥 면(y=yLevel=0.16f)과 intersect되는 점을 calculate
	// pos + dir * t = 0.16f(y) 인 t를 찾으면 된다.
	const float yLevel = 0.16f;
	float t = (yLevel - ray.position.y) / ray.direction.y;
	vec3 hit = ray.position + ray.direction * t;
	vec2 diff = cueTransform.position - vec2(hit.x, hit.z);

	if (cueTransform.mode == CueMode::ROTATION)
	{
		float angle = atan2(diff.y, diff.x);
		cueTransform.rotation = -angle + DEGTORAD(90.0f);
	}
	else if (cueTransform.mode == CueMode::PUSHING)
	{
		float power = dot(cueTransform.getCueDirection(), diff);
		cueTransform.pushAmount = -clamp(power, BALL_RADIUS, MAX_CUE_POWER + BALL_RADIUS);
	}
	cueTransform.update();
}

void Scene::onScreenChanged(int id)
{
	// onGameStart
	if (id == 2)
	{
		cueTransform.mode = CueMode::ROTATION;
		table.resetBallPosition();
	}
}

void Scene::onAllBallStopped()
{
	if (ui.getCurrentScreen() != 2)
		return;

	cueTransform.mode = CueMode::ROTATION;
	ui.showMessage(L"당신 차례입니다!");
}

void Scene::onBallHoleIn(int ballId)
{
	// TODO ball goal-in sound
	getSoundEngine()->play2D("res/sound/cue_1.wav");

	// foul. white ball is in hole.
	if (ballId == 0)
	{
		foul();
	}
}

void Scene::hitWhiteBall()
{
	Ball* ball = table.getBalls()[0];
	float power = -(cueTransform.pushAmount + BALL_RADIUS);
	int powerLevel = power > MAX_CUE_POWER * 0.5f;

	getSoundEngine()->play2D(("res/sound/cue_" + std::to_string(powerLevel) + ".wav").c_str());
	ball->velocity = power * CUE_POWER_MODIFIER * cueTransform.getCueDirection();
}

void Scene::foul()
{
	ui.showMessage(L"파울입니다. 흰색공을 배치하세요.");
	// TODO Place white ball.
	table.getBalls()[0]->position = vec2(0, -TABLE_HEIGHT / 3);
	table.getBalls()[0]->visible = true;
}