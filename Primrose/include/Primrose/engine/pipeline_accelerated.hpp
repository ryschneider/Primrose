#ifndef PRIMROSE_PIPELINE_ACCELERATED_HPP
#define PRIMROSE_PIPELINE_ACCELERATED_HPP

#include "../scene/scene.hpp"

namespace Primrose {
	void createAcceleratedPipelineLayout();

	void generateAcceleratedScene(Scene& scene);
	void destroyAcceleratedScene();
}

#endif
