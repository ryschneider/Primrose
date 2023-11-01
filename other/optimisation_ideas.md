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
* Noise texture sampling
  * One large, smooth texture
    * Smooth means it can be compressed - large region represented without too much memory
  * One smaller, more detailed, repeating texture
    * Smaller details won't be noticed repeating, higher detail & continuous
  * Then have a one-to-one mapping with the first texture, and a repeating mapping with the second, overlayed to give the illusion of high-detail continuous noise