# Choosing a collision query

Collision queries read host Sprite and Tile state. Keep transforms, bitmap data,
maps and collision masks unchanged until a query returns. Collision coverage
does not necessarily match visible drawing: opacity, palette alpha and visibility
are not collision masks.

## Choosing a test

| Need | Interface |
|---|---|
| Test two known sprites now | `CheckSpritePairCollision` |
| Read the last automatic Sprite pass | `HasSpriteCollision`, `GetSpriteCollisions` |
| Test a sprite against a Tile layer | `CheckInteractionsSpriteTileCollision` |
| Find flagged Tile cells under a Sprite box | `GetInteractionsSpriteTileCollisionCells` |
| Test a single canvas point against Tiles | `CheckTilePointCollision`, `GetTilePointInfo` |
| Test many chosen Sprite pairs | `CheckSpritePairCollisionBatch` |
| Find colliding pairs in a supplied Sprite set | `FindSpriteCollisionPairs` |
| Test many sprites against one Tile layer | `CheckInteractionsSpriteTileCollisionBatch` |

The requested `COLLISION_TYPE_*` bits select the tests. BOUNDING_BOX compares
enclosing boxes; ANY_COLOR requires nonzero bitmap indices on both sides;
COLLIDABLE_COLORS uses each owner's selected color-index mask. For Sprite/Tile
queries, the Tile cell must also have its collision flag set. Color index zero
can count for COLLIDABLE_COLORS when both masks include it.

## Known objects

For two sprites, request the types you need and inspect the returned bits:

```c
unsigned char collision = CheckSpritePairCollision(
    player, enemy, COLLISION_TYPE_COLLIDABLE_COLORS);
if (collision != 0) {
    /* Resolve the contact. */
}
```

This query ignores enabled/visible flags and automatic reporting masks. It does
not update the automatic collision reports. Zero can also mean invalid input or
an unavailable numeric query; it is not a separate error status.

For a Tile layer, enable collision on the relevant cells and choose the layer's
collidable-color mask when using COLLIDABLE_COLORS:

```c
SetTileCellCollision(layer, column, row, true);
SetTileLayerCollidableColors(layer, 0xFFFE);
unsigned char collision = CheckInteractionsSpriteTileCollision(
    player, layer, COLLISION_TYPE_COLLIDABLE_COLORS);
```

Tile point queries support rotated maps. Sprite/Tile overlap queries and cell
enumeration currently refuse rotated maps, including bounding-box requests. They
return zero and issue a diagnostic once per layer. Cell enumeration describes
flagged cells under the Sprite box; it does not enumerate precise pixel contacts.
See [Interactions](GDMF_INTERACTIONS.md) and [Tiles](GDMF_TILES.md).

## Automatic Sprite reports

`SetSpriteCollisionTypes` selects the types a sprite records during the automatic
pass. That pass runs during Sprite frame preparation and considers enabled
sprites, including invisible ones. The other sprite need not request reporting.
Read the latest results with `HasSpriteCollision`, `GetSpriteCollisionCount` or
`GetSpriteCollisions`; edits after the pass do not recompute them.

Reports hold at most 32 entries per sprite. Different matched types against the
same sprite occupy separate entries. Additional matches are dropped, so an absent
entry is not proof of no collision when the report is full. Returned arrays are
borrowed and overwritten by the next automatic pass. Copy results you need to
retain. Guests use `CopySpriteCollisions` to copy reports into guest storage.

## Many tests

Use batches when the application has many tests to perform. Chosen-pair and
Sprite/Tile batches write one result byte per input and return the number with
nonzero results. Supply enough output storage and keep it disjoint from input.
Small workloads can run inline; larger workloads can use the shared worker pool.
All dispatched work finishes before return.

`FindSpriteCollisionPairs` chooses candidate pairs from a supplied Sprite list.
Invalid indices and duplicates are skipped. Matches follow the sprites' first
positions in that list. The returned total can exceed output capacity; iterate
only over stored entries. Nonpositive capacity performs a count-only search.
Minus one means the search could not run, not that it found no collisions.

Guest wrappers additionally validate addresses, spans and alignment. Collision
batch outputs require ordinary guest RAM. A refused batch returns zero without
writing results; do not read old output bytes as fresh answers. Pair searches have
their own copied-input and aliasing rules. See the
[guest graphics boundary](../guest/MISL_HOST_GRAPHICS.md#sprites) and
[Sprite batch contracts](GDMF_SPRITES.md#batch-queries).

Current numeric guards do not establish a universal collision workload bound or
exact continuous coverage for every extreme transform. See
[Sprite sampling limits](GDMF_SPRITES.md#point-sampling). Measure application
workloads before choosing an optimization; historical benchmark numbers are not
performance guarantees for another machine or scene.
