# Colors and palettes

Include `gdmf_colors.h` for `Color`, palette operations and predefined colors.
`Color` contains four RGBA bytes. `PackColorsRGBA8` places R, G, B and A in bits
0-7, 8-15, 16-23 and 24-31 of a `uint32_t`, respectively.

## Two palette sets

| Set | Palettes | Entries per palette | Use |
|---|---|---|---|
| Standard | 256 | 16 | Sprite and Tile indexed colors; supported Pixie formats |
| Deep | 16 | 256 | Pixie `PALETTE8BPP` colors |

The sets have independent storage and change markers. Updating one does not
update the other. `InitColorsPalettes` fills every entry in standard palette N
with `GDMF_COLORN`. `InitColorsDeepPalettes` fills deep palette zero with opaque
grayscale and the remaining palettes with channel-offset ramps. The engine calls
initialization at startup; calling it again replaces the current palette values.

## Standard palette writes

`SetColorsPaletteColor` changes one entry, including entry zero. All byte-sized
palette indices are valid; entry indices 16 or greater return false.
`GetColorsPaletteColor` returns `BLANK` for an invalid entry index; BLANK can
also be a stored color.

`SetColorsPalette` takes a 16-entry array and copies entries 1-15, preserving the
destination's entry zero. An input entry exactly equal to that destination entry,
including alpha, becomes `BLANK`. NULL input returns false; otherwise it returns
true. This call is not a literal copy of all sixteen colors.

Entry zero normally retains a default value. Change it explicitly with the
individual setter when needed. This is a usage convention, not a universal
restriction: deep palette bulk writes include entry zero. Rendering transparency
and collision coverage have their own rules; assigning a color does not replace
those rules.

## Deep palette writes

`SetColorsDeepPalette` copies all 256 input colors, including entry zero. It returns
false for NULL input or a palette index of 16 or greater. Input must contain the
complete array. `SetColorsDeepPaletteColor` changes one entry; every byte-sized
entry index is valid, but the palette index must be below 16.
`GetColorsDeepPaletteColor` returns BLANK for an invalid palette index.

## Change markers and access

Use setters for palette mutations. The exported arrays remain accessible in this
version, but direct writes bypass renderer change tracking. Synchronize reads
and writes with rendering; the palette API does not supply concurrent access.

`GetColorsGeneration` and `GetColorsDeepGeneration` advance for accepted setter
calls and initialization, including writes that leave the stored color unchanged.
They are 32-bit unsigned markers and can wrap to zero. Compare snapshots for
inequality rather than ordering. They are not permanent unique revision IDs;
equal values after a complete counter cycle cannot prove unchanged contents.

## Predefined colors

`GDMF_COLOR0` through `GDMF_COLOR255` are base constants.
`GetColorsColorFromChar` looks them up independently of mutable palette storage.
Named colors and hardware-inspired tables are declared in the headers. Their
values describe the shipped definitions; a name is not a guarantee of a measured
display color. `GetColorsCommodoreColor`, `GetColorsTandyColor` and
`GetColorsANSIColor` accept indices 0-15 and return opaque black above that range.
The ANSI helper uses Tandy/CGA index order, not ANSI SGR numbering.

See [Pixie formats](GDMF_PIXIES_FORMATS.md), [Sprites](GDMF_SPRITES.md) and
[Tiles](GDMF_TILES.md) for palette selection and transparency behavior.
