// GDMF Sprites helpers -- relative (delta) conveniences over the absolute
// sprite API in gdmf_sprites.h.
//
// The core is deliberately absolute-only: a sprite is a state register you set
// to a value, never one you ask to change "by" some amount (see the design
// rationale in gdmf_sprites.h). These wrappers restore the common convenience
// of nudging a sprite each frame -- they read the current value, add a delta,
// and apply it through the absolute setter. Nothing here is new capability;
// it's caller-side arithmetic the caller could do itself, collected in one
// place because it comes up constantly. The "By" suffix marks them relative,
// distinguishing them from the absolute SetSprite* setters.

#ifndef GDMF_SPRITES_HELP_H
#define GDMF_SPRITES_HELP_H

#include "gdmf_sprites.h"

// Relative move: adds (dx, dy) to the sprite's current position. Returns true
// on success, false if the sprite index is invalid (propagated straight from
// SetSpritePosition, which no-ops an invalid index).
bool MoveSpriteBy(int spriteIndex, float dx, float dy);

// Relative scale: adds delta to the current scale. Scale must stay positive --
// SetSpriteScale rejects a non-positive result, so a delta that would drop the
// scale to <= 0 leaves it unchanged and returns false (the same clamp the old
// ChangeSpriteScale had). Also returns false on an invalid index.
bool ScaleSpriteBy(int spriteIndex, float delta);

// Relative rotation: adds delta (in degrees) to the current rotation. Returns
// false on an invalid index.
bool RotateSpriteBy(int spriteIndex, float delta);

#endif // GDMF_SPRITES_HELP_H