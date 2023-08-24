## Definite Optimisations
* Pre-record Command Buffers

## Possible Optimisations
* VK_KHR_dynamic_rendering
  * Not really an optimisation, should be in Vulkan 1.3 without KHR
  * idk if it's faster/slower, reduces unnecessary code though
* Async Reprojection
  * https://www.youtube.com/watch?v=VvFyOFacljg
* Frame Caching
  * In each frame, see if fragment was rendered in a previous frame
  * If so, reuse lightning not dependant on view direction
    * Reduce cost of sampling for rough surfaces
* VK_KHR_pipeline_library
  * Faster for having tons of shaders in one ray tracing pipeline
