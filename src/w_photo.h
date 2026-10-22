// w_photo.h -- photographed frames: PNGs in, a sprite file out (cf. w_wad.h)
//
// A photo creature's frames are gathered while its creature file is read,
// then processed together: background removal, one scale for every frame,
// one shared 31-colour palette, hotspot at the feet. The result is an
// in-memory gra_t, so the rest of the engine treats photographed clay the
// same as Primal Rage's art.

#ifndef __W_PHOTO__
#define __W_PHOTO__

#include "pr_def.h"
#include "w_gra.h"

typedef struct photoset_s photoset_t;

photoset_t *W_PhotoBegin(const char *name);
// Register a photo (paths are deduplicated); returns its frame index.
int W_PhotoFrame(photoset_t *ps, const char *path);
gra_t *W_PhotoGra(photoset_t *ps); // valid at once; frames fill in at finish
// Process everything. backdrop: empty-scene shot or NULL; reference: frame
// whose figure gets `height` pixels tall.
void W_PhotoFinish(photoset_t *ps, const char *backdrop, int reference, int height);

byte *W_LoadPNG(const char *path, int *width, int *height, char *err, size_t errlen); // RGBA

#endif
