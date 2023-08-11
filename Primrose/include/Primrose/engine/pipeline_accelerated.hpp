#ifndef PRIMROSE_PIPELINE_ACCELERATED_HPP
#define PRIMROSE_PIPELINE_ACCELERATED_HPP

namespace Primrose {
	void createAcceleratedPipelineLayout();
	void createAcceleratedPipeline();

	void createShaderTable();
	void createBottomAccelerationStructure();
	void createTopAccelerationStructure();
}

#endif
