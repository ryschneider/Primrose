#include "planet_scene.hpp"

#include <Primrose/core.hpp>
#include <iostream>

using namespace Primrose;

float r = 80;
glm::vec3 planetPos(0, -r, 5);
void planetScene() {
	uint spherePrim = addPrim(PRIM::SPHERE);
	uint cyl = addPrim(PRIM::CYLINDER);
	uint box = addPrim(PRIM::BOX);
	uint prim4 = addPrim(PRIM::P4);
	uint torus = addPrim(PRIM::TORUS, 0.5);

    addTranslate(planetPos.x, planetPos.y, planetPos.z);
    addScale(r);
    uint planet = addIdentity(spherePrim);

    addScale(1);
    uint rocks = addIdentity(prim4);

    addTranslate(planetPos.x, planetPos.y, planetPos.z);
    addScale(r + 1);
    uint rockLimit = addIdentity(spherePrim);

    uint rockSurface = addIntersection(rocks, rockLimit);
    addRender(addUnion(rockSurface, planet));

    addTranslate(0, 2, 5);
	addRender(addIdentity(cyl));

//	return;

    glm::mat4 csgModel = Primrose::transformMatrix(
            planetPos + glm::vec3(0, 0.7071, 0.7071) * (r+2),
            glm::vec3(1),
            3.1415/4, glm::vec3(1, 0, 0));

    addTransform(csgModel);
    unsigned int cube = addIdentity(box);

    addTransform(csgModel);
    addScale(0.5f);
    unsigned int yCyl = addIdentity(cyl);



    addTransform(csgModel);
    addScale(0.5f);
    addRotate(3.1415f/2.f, {1.f, 0.f, 0.f});
    unsigned int zCyl = addIdentity(cyl);
    addTransform(csgModel);
    addScale(0.5f);
    addRotate(3.1415f/2.f, {0.f, 0.f, 1.f});
    unsigned int xCyl = addIdentity(cyl);
    addTransform(csgModel);
    addScale(1.2f);
    unsigned int sphere = addIdentity(spherePrim);

    uint xyCyl = addUnion(xCyl, yCyl);
    uint xyzCyl = addUnion(xyCyl, zCyl);
    uint sphCube = addIntersection(cube, sphere);
	addRender(addDifference(sphCube, xyzCyl));


    addTranslate(0,40, 5);
    addScale(10, 2, 10);
	addRender(addIdentity(box));

    addTranslate(0,-2*r - 40, 5);
    addScale(10);
	addRender(addIdentity(torus));
}
