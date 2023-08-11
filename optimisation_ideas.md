## Definite Optimisations
* Pre-record Command Buffers

## Possible Optimisations
* Async Reprojection
  * https://www.youtube.com/watch?v=VvFyOFacljg
* Frame Caching
  * In each frame, see if fragment was rendered in a previous frame
  * If so, reuse lightning not dependant on view direction
    * Reduce cost of sampling for rough surfaces
