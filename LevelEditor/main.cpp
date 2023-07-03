#include <Primrose/core.hpp>
#include <Primrose/ui_image.hpp>
#include <iostream>
#include <memory>

#include "player_movement.hpp"
#include "Primrose/ui_panel.hpp"

const char* APP_NAME = "Level Editor";
const unsigned int APP_VERSION = 001'000'000;

using namespace Primrose;

typedef unsigned int uint;
float r = 80;
glm::vec3 planetPos(0, -r, 5);
void _planetScene() {
	uniforms.primitives[0] = Prim(PRIM_SPHERE);
	uniforms.primitives[4] = Prim(PRIM_4);

	addTranslate(planetPos.x, planetPos.y, planetPos.z);
	addScale(r);
	uint planet = addIdentity(0);

	addScale(1);
	uint rocks = addIdentity(4, false);

	addTranslate(planetPos.x, planetPos.y, planetPos.z);
	addScale(r + 1);
	uint rockLimit = addIdentity(0);

	uint rockSurface = addIntersection(rocks, rockLimit);
	addUnion(rockSurface, planet, true);



	uniforms.primitives[1] = Prim(PRIM_CYLINDER);
	addTranslate(0, 2, 5);
	addIdentity(1, true);

	uniforms.primitives[2] = Prim(PRIM_BOX);
	uniforms.primitives[3] = Prim(PRIM_CYLINDER);

	glm::mat4 csgModel = Primrose::transformMatrix(
		planetPos + glm::vec3(0, 0.7071, 0.7071) * (r+2),
		glm::vec3(1),
		3.1415/4, glm::vec3(1, 0, 0));

	addTransform(csgModel);
	unsigned int cube = addIdentity(2);
	addTransform(csgModel);
	addScale(0.5f);
	unsigned int yCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(0.5f);
	addRotate(3.1415f/2.f, {1.f, 0.f, 0.f});
	unsigned int zCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(0.5f);
	addRotate(3.1415f/2.f, {0.f, 0.f, 1.f});
	unsigned int xCyl = addIdentity(3);
	addTransform(csgModel);
	addScale(1.2f);
	unsigned int sphere = addIdentity(0);

	uint xyCyl = addUnion(xCyl, yCyl);
	uint xyzCyl = addUnion(xyCyl, zCyl);
	uint sphCube = addIntersection(cube, sphere);
	addDifference(sphCube, xyzCyl, true);


	addTranslate(0,40, 5);
	addScale(10, 2, 10);
	addIdentity(2, true);

	uniforms.primitives[5] = Prim(PRIM_TORUS, 0.5);
	addTranslate(0,-2*r - 40, 5);
	addScale(10);
	addIdentity(5, true);
}

void update() {
	updatePosition();
}

int main() {
	setup(APP_NAME, APP_VERSION);
	mouseMovementCallback = mouseCallback;

//	uniforms.primitives[0] = Prim(PRIM_BOX);
//	addTranslate(0, 0, 5);
//	addIdentity(0, true);

	_planetScene();

//	UIImage* myImg = new UIImage(glm::vec2(-1, 1), glm::vec2(0.5, -0.5));
//	myImg->init("resources/include_dirs_stb.png");
//	uiScene.emplace_back(myImg);

	UIPanel* myPanel1 = new UIPanel({0, 0}, {0.2, 0.1}, 0);
	myPanel1->init(glm::vec3(0.1));
	uiScene.emplace_back(myPanel1);

	UIPanel* myPanel2 = new UIPanel({0, 0}, {0.2, 0.1}, 3.1415/4);
	myPanel2->init(glm::vec3(0.2));
	uiScene.emplace_back(myPanel2);

	UIPanel* myPanel3 = new UIPanel({0, 0}, {0.2, 0.1}, 3.1415/2);
	myPanel3->init(glm::vec3(0.3));
	uiScene.emplace_back(myPanel3);

	run(update);
}
