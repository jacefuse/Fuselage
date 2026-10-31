#pragma once

// Shared MISL ecall numbers and register contracts.
// a7: call number; a0..a6: arguments; a0 or a0:a1: result (low word first).
// Float values use IEEE-754 bits in integer registers. Pair positions are call-specific.
// Pointers are guest addresses, never host pointers. Strings are NUL-terminated.
// Declare MISL_ABI_VERSION before any other ecall; the host accepts an exact match.
// SEE: (MISL_ABI_MMIO.md, Calls and compatibility)
#define MISL_ABI_VERSION 2026080501u
#define MISL_VERSION "0.5.2026103001 ENTERON"

#define MISL_ECALL_SUBSYS(n) ((n) & 0xFF00)

// Retired call numbers must not be reused:
// 0x1013, 0x1018; 0x1116..0x1119, 0x112E, 0x1131..0x1132; 0x1211, 0x1213.

// System and presentation
#define MISL_SYS_SIGNAL 0x0100           // FuselageSignal(a0=FuselageSignalType)
#define MISL_SYS_WAITVBL 0x0101          // FuselageWaitVBL()            [yields slice]
#define MISL_SYS_YIELD 0x0102            // release rest of this slice   [yields slice]
#define MISL_SYS_SLEEP 0x0103            // sleep(a0=ticks): yield, then sit out a0 sim ticks  [yields slice]
#define MISL_SYS_GET_FPS 0x0104          // FuselageGetCurrentFPS() -> a0=float
#define MISL_SYS_SET_MOUSECAPTURE 0x0105 // FuselageSetMouseCapture(a0=bool)
#define MISL_SYS_GET_MOUSECAPTURE 0x0106 // FuselageGetMouseCapture() -> a0=bool
#define MISL_SYS_TOGGLE_MOUSECAPTURE 0x0107 // FuselageToggleMouseCapture() -> a0=bool
#define MISL_SYS_SET_CURSORVISIBLE 0x0108   // FuselageSetCursorVisible(a0=bool)
#define MISL_SYS_GET_CURSORVISIBLE 0x0109   // FuselageGetCursorVisible() -> a0=bool
#define MISL_SYS_SET_VSYNC 0x010A           // FuselageSetVSync(a0=bool)
#define MISL_SYS_GET_VSYNC 0x010B           // FuselageGetVSync() -> a0=bool
#define MISL_SYS_SET_TARGETFPS 0x010C       // FuselageSetTargetFPS(a0:a1=double)
#define MISL_SYS_GET_TARGETFPS 0x010D       // FuselageGetTargetFPS() -> a0:a1=double
#define MISL_SYS_GET_SIMRATE 0x010E         // FuselageGetSimRate() -> a0:a1=double
#define MISL_SYS_ABI_DECLARE 0x010F         // declare guest ABI version (a0=version) -> a0=bool
#define MISL_SYS_SET_CLEARCOLOR 0x0110      // GDMF_SetClearColor(a0=packed Color)
#define MISL_SYS_DESCRIPTOR 0x0111          // -> a0 = MislDescriptor*, 0 if none
#define MISL_SYS_GET_CANVAS_W 0x0112        // GDMF_GetCanvasWidth() -> a0
#define MISL_SYS_GET_CANVAS_H 0x0113        // GDMF_GetCanvasHeight() -> a0
#define MISL_SYS_GET_CLEARCOLOR 0x0114      // GDMF_GetClearColor() -> a0=packed Color
#define MISL_SYS_ELAPSED_US 0x0115          // DICE_GetElapsedMicroseconds() -> a0:a1=uint64
#define MISL_SYS_CAPTURE 0x0116             // FuselageCaptureFrame(a0=nameptr) -> a0=bool
#define MISL_SYS_CAPTURE_PENDING 0x0117     // FuselageCapturePending() -> a0=bool
#define MISL_SYS_IS_FOCUSED 0x0118          // GDMF_IsFocused() -> a0=bool

// Diagnostic text
#define MISL_TL_PRINT 0x1000          // tlPrint(a0=strptr) -> a0=count
#define MISL_TL_PRINT_C 0x1001        // tlPrintC(a0=strptr, a1=Color) -> a0=count
#define MISL_TL_PRINT_CP 0x1002       // tlPrintCP(a0=strptr, a1=Color, a2=x, a3=y) -> a0
#define MISL_TL_PRINTCHAR 0x1003      // tlPrintChar(a0=char) -> a0
#define MISL_TL_PRINTCHAR_C 0x1004    // tlPrintCharC(a0=char, a1=Color) -> a0
#define MISL_TL_PRINTCHAR_CP 0x1005   // tlPrintCharCP(a0=char, a1=Color, a2=x, a3=y) -> a0
#define MISL_TL_PRINTINT 0x1006       // tlPrintInt(a0=int) -> a0
#define MISL_TL_PRINTINT_C 0x1007     // tlPrintIntC(a0=int, a1=Color) -> a0
#define MISL_TL_PRINTINT_CP 0x1008    // tlPrintIntCP(a0=int, a1=Color, a2=x, a3=y) -> a0
#define MISL_TL_NEWLINE 0x1009        // tlNewLine() -> a0
#define MISL_TL_CLS 0x100A            // tlCLS()
#define MISL_TL_HOME 0x100B           // tlHome()
#define MISL_TL_SET_COLOR 0x100C      // tlSetColor(a0=Color)
#define MISL_TL_GET_COLOR 0x100D      // tlGetColor() -> a0=Color
#define MISL_TL_SET_CURSOR 0x100E     // tlSetCursor(a0=x, a1=y)
#define MISL_TL_GET_CURSOR 0x100F     // tlGetCursor() -> a0
#define MISL_TL_GET_CURSOR_X 0x1010   // tlGetCursorX() -> a0
#define MISL_TL_GET_CURSOR_Y 0x1011   // tlGetCursorY() -> a0
#define MISL_TL_SCROLL_UP 0x1012      // tlScrollUp() -> a0
#define MISL_TL_ACTIVE 0x1014         // tlActivate() -> a0=bool
#define MISL_TL_INACTIVE 0x1015       // tlDeactivate() -> a0=bool
#define MISL_TL_TOGGLE 0x1016         // tlToggle() -> a0=bool
#define MISL_TL_STATUS 0x1017         // tlStatus() -> a0=bool
#define MISL_TL_GET_CELL_CHAR 0x1019  // tlGetCellCharacter(a0=x, a1=y) -> a0=character
#define MISL_TL_GET_CELL_COLOR 0x101A // tlGetCellColor(a0=x, a1=y) -> a0=packed Color

// Sprites
#define MISL_SPR_UPLOAD_BITMAP 0x1100    // UploadSpriteBitmap(a0=id, a1=dataptr) -> a0=bool
#define MISL_SPR_ASSIGN 0x1101           // AssignSprite(a0=idx, a1=bitmapID) -> a0=bool
#define MISL_SPR_ASSIGN_FROM 0x1102      // AssignSpriteBitmapFromSprite(a0=src, a1=dst) -> a0=bool
#define MISL_SPR_GET_BITMAP_ID 0x1103    // GetSpriteBitmapID(a0=idx) -> a0
#define MISL_SPR_CLEAR 0x1104            // ClearSprite(a0=idx)
#define MISL_SPR_TEST_PATTERN 0x1105     // AssignSpriteTestPattern(a0=idx)
#define MISL_SPR_GET_ENABLED 0x1106      // GetSpriteEnabled(a0=idx) -> a0=bool
#define MISL_SPR_SET_ENABLED 0x1107      // SetSpriteEnabled(a0=idx, a1=bool) -> a0=bool
#define MISL_SPR_TOGGLE_ENABLED 0x1108   // ToggleSpriteEnabled(a0=idx) -> a0=bool
#define MISL_SPR_GET_VISIBLE 0x1109      // GetSpriteVisible(a0=idx) -> a0=bool
#define MISL_SPR_SET_VISIBLE 0x110A      // SetSpriteVisible(a0=idx, a1=bool) -> a0=bool
#define MISL_SPR_TOGGLE_VISIBLE 0x110B   // ToggleSpriteVisible(a0=idx) -> a0=bool
#define MISL_SPR_SET_POSITION 0x110C     // SetSpritePosition(a0=idx, a1=fx, a2=fy)
#define MISL_SPR_OFFSET_POSITION 0x110D  // OffsetSpritePosition(a0=idx, a1=fdx, a2=fdy)
#define MISL_SPR_GET_X 0x110E            // GetSpriteX(a0=idx) -> a0=float
#define MISL_SPR_GET_Y 0x110F            // GetSpriteY(a0=idx) -> a0=float
#define MISL_SPR_SET_SCALE 0x1110        // SetSpriteScale(a0=idx, a1=fscale)
#define MISL_SPR_GET_SCALE 0x1111        // GetSpriteScale(a0=idx) -> a0=float
#define MISL_SPR_OFFSET_SCALE 0x1112     // OffsetSpriteScale(a0=idx, a1=fdelta) -> a0=bool
#define MISL_SPR_SET_ROTATION 0x1113     // SetSpriteRotation(a0=idx, a1=frot)
#define MISL_SPR_GET_ROTATION 0x1114     // GetSpriteRotation(a0=idx) -> a0=float
#define MISL_SPR_OFFSET_ROTATION 0x1115  // OffsetSpriteRotation(a0=idx, a1=fdelta) -> a0=bool
#define MISL_SPR_SET_PRIORITY 0x111A     // SetSpritePriority(a0=idx, a1=priority)
#define MISL_SPR_GET_PRIORITY 0x111B     // GetSpritePriority(a0=idx) -> a0
#define MISL_SPR_SET_PALETTE 0x111C      // SetSpriteColorPalette(a0=idx, a1=palette) -> a0=bool
#define MISL_SPR_GET_PALETTE 0x111D      // GetSpriteColorPalette(a0=idx) -> a0
#define MISL_SPR_SET_TRANSPARENCY 0x111E // SetSpriteTransparency(a0=idx, a1=t)
#define MISL_SPR_GET_TRANSPARENCY 0x111F // GetSpriteTransparency(a0=idx) -> a0
#define MISL_SPR_SHOW_ZERO 0x1120        // SetSpriteShowZero(a0=idx, a1=bool)
#define MISL_SPR_SET_COLLIDABLE 0x1121   // SetSpriteCollidableColors(a0=idx, a1=mask) -> a0
#define MISL_SPR_GET_COLLIDABLE 0x1122   // GetSpriteCollidableColors(a0=idx) -> a0
#define MISL_SPR_SET_COLLTYPES 0x1123    // SetSpriteCollisionTypes(a0=idx, a1=mask) -> a0
#define MISL_SPR_GET_COLLTYPES 0x1124    // GetSpriteCollisionTypes(a0=idx) -> a0
#define MISL_SPR_HAS_COLLISION 0x1125    // HasSpriteCollision(a0=idx) -> a0=bool
#define MISL_SPR_GET_COLL_COUNT 0x1126   // GetSpriteCollisionCount(a0=idx) -> a0
#define MISL_SPR_COPY_COLLISIONS                                                                             \
    0x1127                                 // copy GetSpriteCollisions into guest buf
                                           //   (a0=idx, a1=outbufptr, a2=maxcount) -> a0=count
#define MISL_SPR_CHECK_PAIR 0x1128         // CheckSpritePairCollision(a0=A, a1=B, a2=types) -> a0
#define MISL_SPR_WORLD_POINT_ON 0x1129     // IsSpriteWorldPointInside(a0=idx, a1=fx, a2=fy) -> a0=bool
#define MISL_SPR_TOGGLE_ATLAS 0x112A       // ToggleSpriteAtlasView()
#define MISL_SPR_GET_ATLAS_ACTIVE 0x112B   // GetSpriteAtlasViewActive() -> a0=bool
#define MISL_SPR_GET_RENDERED_COUNT 0x112C // GetSpriteRenderedCount() -> a0
#define MISL_SPR_SET_HOTSPOT 0x112D        // SetSpriteHotspot(a0=idx, a1=fhotX, a2=fhotY) -> a0=bool
#define MISL_SPR_SET_ROT_HOTSPOT 0x112F    // SetSpriteRotateAroundHotspot(a0=idx, a1=bool) -> a0=bool
#define MISL_SPR_GET_ROT_HOTSPOT 0x1130    // GetSpriteRotateAroundHotspot(a0=idx) -> a0=bool
#define MISL_SPR_GET_SHOWZERO 0x1133       // GetSpriteShowZero(a0=idx) -> a0=bool
#define MISL_SPR_SET_FLIP_X 0x1134         // SetSpriteFlipX(a0=idx, a1=bool) -> a0=bool
#define MISL_SPR_SET_FLIP_Y 0x1135         // SetSpriteFlipY(a0=idx, a1=bool) -> a0=bool
#define MISL_SPR_GET_FLIP_X 0x1136         // GetSpriteFlipX(a0=idx) -> a0=bool
#define MISL_SPR_GET_FLIP_Y 0x1137         // GetSpriteFlipY(a0=idx) -> a0=bool
#define MISL_SPR_SET_SCALE_X 0x1138        // SetSpriteScaleX(a0=idx, a1=fscale) -> a0=bool
#define MISL_SPR_SET_SCALE_Y 0x1139        // SetSpriteScaleY(a0=idx, a1=fscale) -> a0=bool
#define MISL_SPR_GET_SCALE_X 0x113A        // GetSpriteScaleX(a0=idx) -> a0=float (1.0 on a bad index)
#define MISL_SPR_GET_SCALE_Y 0x113B        // GetSpriteScaleY(a0=idx) -> a0=float
#define MISL_SPR_SET_SKEW_X 0x113C         // SetSpriteSkewX(a0=idx, a1=fskew) -> a0=bool
#define MISL_SPR_SET_SKEW_Y 0x113D         // SetSpriteSkewY(a0=idx, a1=fskew) -> a0=bool
#define MISL_SPR_GET_SKEW_X 0x113E         // GetSpriteSkewX(a0=idx) -> a0=float (0.0 on a bad index)
#define MISL_SPR_GET_SKEW_Y 0x113F         // GetSpriteSkewY(a0=idx) -> a0=float
#define MISL_SPR_SET_HOTSPOT_X 0x1140      // SetSpriteHotspotX(a0=idx, a1=fhot) -> a0=bool
#define MISL_SPR_SET_HOTSPOT_Y 0x1141      // SetSpriteHotspotY(a0=idx, a1=fhot) -> a0=bool
#define MISL_SPR_GET_HOTSPOT_X 0x1142      // GetSpriteHotspotX(a0=idx) -> a0=float
#define MISL_SPR_GET_HOTSPOT_Y 0x1143      // GetSpriteHotspotY(a0=idx) -> a0=float
#define MISL_SPR_CHECK_PAIR_BATCH                                                                            \
    0x1144 // CheckSpritePairCollisionBatch(a0=ptr SpritePairRef[a1], a1=count,
           //   a2=types, a3=outptr results[a1]) -> a0=pairs matched
#define MISL_SPR_FIND_PAIRS                                                                                  \
    0x1145 // FindSpriteCollisionPairs(a0=ptr int16[a1], a1=count, a2=types,
           //   a3=outptr SpritePairRef[a4], a4=max, a5=outptr types[a4] or 0)
           //   -> a0=pairs found (may exceed a4), or -1 refused

// Tiles
#define MISL_TILE_INIT                                                                                       \
    0x1200 // InitTileLayer(a0=layer, a1=mapW, a2=mapH, a3=tileW, a4=tileH, a5=tileCount, a6=float scale) ->
           // a0=bool
#define MISL_TILE_RELEASE 0x1201            // ReleaseTileLayer(a0=layer) -> a0=bool
#define MISL_TILE_UPLOAD_BITMAP 0x1202      // UploadTileBitmap(a0=layer, a1=tileID, a2=dataptr) -> a0=bool
#define MISL_TILE_GET_VISIBLE 0x1203        // GetTileLayerVisible(a0=layer) -> a0=bool
#define MISL_TILE_SET_VISIBLE 0x1204        // SetTileLayerVisible(a0=layer, a1=bool) -> a0=bool
#define MISL_TILE_TOGGLE_VISIBLE 0x1205     // ToggleTileLayerVisible(a0=layer) -> a0=bool
#define MISL_TILE_GET_ENABLED 0x1206        // GetTileLayerEnabled(a0=layer) -> a0=bool
#define MISL_TILE_SET_ENABLED 0x1207        // SetTileLayerEnabled(a0=layer, a1=bool) -> a0=bool
#define MISL_TILE_TOGGLE_ENABLED 0x1208     // ToggleTileLayerEnabled(a0=layer) -> a0=bool
#define MISL_TILE_PLACE 0x1209              // PlaceTile(a0=layer, a1=x, a2=y, a3=tileID) -> a0=bool
#define MISL_TILE_FILL 0x120A               // FillTileLayer(a0=layer, a1=tileID) -> a0=bool
#define MISL_TILE_SET_CELL_PALETTE 0x120B   // SetTileCellPalette(a0=layer, a1=x, a2=y, a3=pal) -> a0=bool
#define MISL_TILE_SET_PALETTE 0x120C        // SetTileLayerPalette(a0=layer, a1=pal) -> a0=bool
#define MISL_TILE_SET_CELL_TRANSP 0x120D    // SetTileCellTransparency(a0=layer, a1=x, a2=y, a3=t) -> a0=bool
#define MISL_TILE_SET_TRANSP 0x120E         // SetTileLayerTransparency(a0=layer, a1=t) -> a0=bool
#define MISL_TILE_SET_CELL_SHOWZERO 0x120F  // SetTileCellShowZero(a0=layer, a1=x, a2=y, a3=bool) -> a0=bool
#define MISL_TILE_SET_SHOWZERO 0x1210       // SetTileLayerShowZero(a0=layer, a1=bool) -> a0=bool
#define MISL_TILE_SET_CELL_COLLISION 0x1212 // SetTileCellCollision(a0=layer, a1=x, a2=y, a3=bool)
#define MISL_TILE_SET_SCALE 0x1214          // SetTileLayerScale(a0=layer, a1=fscale) -> a0=bool
#define MISL_TILE_SET_VIEWPORT                                                                               \
    0x1215 // SetTileViewport(a0=layer, a1..a4=x,y,w,h as float) -> a0=bool  (a1/a2 signed: may be off-canvas)
#define MISL_TILE_SCROLL 0x1216         // ScrollTileMap(a0=layer, a1:a2=dDX, a3:a4=dDY) -> a0=bool
#define MISL_TILE_SET_MAP_OFFSET 0x1217 // SetTileMapOffset(a0=layer, a1:a2=dX, a3:a4=dY) -> a0=bool
#define MISL_TILE_SET_CELL_OFFSET                                                                            \
    0x1218 // SetTileCellOffset(a0=layer, a1=mapX, a2=mapY, a3=offX, a4=offY) -> a0=bool
           // (was MISL_TILE_SET_TILE_OFFSET / SetTileOffset until 2026-09-13; same number, same meaning)
#define MISL_TILE_SET_COLLIDABLE 0x1219    // SetTileLayerCollidableColors(a0=layer, a1=mask)
#define MISL_TILE_GET_COLLIDABLE 0x121A    // GetTileLayerCollidableColors(a0=layer) -> a0
#define MISL_TILE_TEST_PATTERN 0x121B      // UploadTileTestPattern(a0=layer, a1=tileID)
#define MISL_TILE_BOX_PATTERN 0x121C       // UploadTileBoxPattern(a0=layer, a1=tileID)
#define MISL_TILE_TOGGLE_ATLAS 0x121D      // ToggleTileAtlasView(a0=layer)
#define MISL_TILE_GET_ATLAS_ACTIVE 0x121E  // GetTileAtlasViewActive(a0=layer) -> a0=bool
#define MISL_TILE_SET_PRIORITY 0x121F      // SetTileLayerPriority(a0=layer, a1=priority) -> a0=bool
#define MISL_TILE_GET_PRIORITY 0x1220      // GetTileLayerPriority(a0=layer) -> a0
#define MISL_TILE_SET_ROTATION 0x1221      // SetTileCellRotation(a0=layer, a1=x, a2=y, a3=rotate) -> a0=bool
#define MISL_TILE_GET_ROTATION 0x1222      // GetTileCellRotation(a0=layer, a1=x, a2=y) -> a0=bool
#define MISL_TILE_GET_TILE 0x1223          // GetTile(a0=layer, a1=x, a2=y) -> a0=tileID
#define MISL_TILE_GET_CELL_PALETTE 0x1224  // GetTileCellPalette(a0=layer, a1=x, a2=y) -> a0
#define MISL_TILE_GET_CELL_TRANSP 0x1225   // GetTileCellTransparency(a0=layer, a1=x, a2=y) -> a0
#define MISL_TILE_GET_CELL_SHOWZERO 0x1226 // GetTileCellShowZero(a0=layer, a1=x, a2=y) -> a0=bool
#define MISL_TILE_GET_CELL_COLLIS 0x1228   // GetTileCellCollision(a0=layer, a1=x, a2=y) -> a0=bool
#define MISL_TILE_SET_CELL_TAG 0x1229      // SetTileCellTag(a0=layer, a1=x, a2=y, a3=tag) -> a0=bool
#define MISL_TILE_GET_CELL_TAG 0x122A      // GetTileCellTag(a0=layer, a1=x, a2=y) -> a0=tag
#define MISL_TILE_GET_SCALE 0x122C         // GetTileLayerScale(a0=layer) -> a0=float
#define MISL_TILE_GET_VIEWPORT 0x122D      // GetTileViewport(a0=layer, a1..a4 = outptr float x,y,w,h)
#define MISL_TILE_GET_MAP_OFFSET 0x122E // GetTileMapOffset(a0=layer, a1=outptr double x, a2=outptr double y)
#define MISL_TILE_GET_CELL_OFFSET                                                                            \
    0x122F // GetTileCellOffset(a0=layer, a1=mapX, a2=mapY, a3=outptr int8 x, a4=outptr int8 y)
           // (was MISL_TILE_GET_TILE_OFFSET / GetTileOffset until 2026-09-13; same number, same meaning)
#define MISL_TILE_SET_CELL_FLIP_X 0x1230 // SetTileCellFlipX(a0=layer, a1=x, a2=y, a3=bool) -> a0=bool
#define MISL_TILE_SET_CELL_FLIP_Y 0x1231 // SetTileCellFlipY(a0=layer, a1=x, a2=y, a3=bool) -> a0=bool
#define MISL_TILE_GET_CELL_FLIP_X 0x1232 // GetTileCellFlipX(a0=layer, a1=x, a2=y) -> a0=bool
#define MISL_TILE_GET_CELL_FLIP_Y 0x1233 // GetTileCellFlipY(a0=layer, a1=x, a2=y) -> a0=bool
#define MISL_TILE_SET_WRAP_X 0x1234      // SetTileMapWrapX(a0=layer, a1=bool) -> a0=bool
#define MISL_TILE_SET_WRAP_Y 0x1235      // SetTileMapWrapY(a0=layer, a1=bool) -> a0=bool
#define MISL_TILE_GET_WRAP_X 0x1236      // GetTileMapWrapX(a0=layer) -> a0=bool
#define MISL_TILE_GET_WRAP_Y 0x1237      // GetTileMapWrapY(a0=layer) -> a0=bool
#define MISL_TILE_FILL_ROW 0x1238        // FillTileRow(a0=layer, a1=mapY, a2=tileID) -> a0=bool
#define MISL_TILE_FILL_COLUMN 0x1239     // FillTileColumn(a0=layer, a1=mapX, a2=tileID) -> a0=bool
#define MISL_TILE_FILL_REGION                                                                                \
    0x123A // FillTileRegion(a0=layer, a1=mapX, a2=mapY, a3=width, a4=height, a5=tileID) -> a0=bool
#define MISL_TILE_GET_TILE_WIDTH 0x123B  // GetTileLayerTileWidth(a0=layer) -> a0 (0 if invalid)
#define MISL_TILE_GET_TILE_HEIGHT 0x123C // GetTileLayerTileHeight(a0=layer) -> a0 (0 if invalid)
#define MISL_TILE_CHECK_POINT_COLLISION                                                                      \
    0x123D                                // CheckTilePointCollision(a0=layer, a1=x, a2=y as float) -> a0=bool
#define MISL_TILE_SET_MAP_ROTATION 0x123E // SetTileMapRotation(a0=layer, a1=radians as float) -> a0=bool
#define MISL_TILE_GET_MAP_ROTATION 0x123F // GetTileMapRotation(a0=layer) -> a0=radians as float
#define MISL_TILE_SET_MAP_PIVOT 0x1240    // SetTileMapPivot(a0=layer, a1=x, a2=y as float) -> a0=bool
#define MISL_TILE_GET_MAP_PIVOT_X 0x1241  // GetTileMapPivot(a0=layer) -> a0=x as float
#define MISL_TILE_GET_MAP_PIVOT_Y 0x1242  // GetTileMapPivot(a0=layer) -> a0=y as float

// Pixies
#define MISL_PIX_INIT 0x1300     // InitPixie(a0=id, a1=mode, a2=outW, a3=outH) -> a0=bool
#define MISL_PIX_SHUTDOWN 0x1301 // ReleasePixie(a0=id)
#define MISL_PIX_COMMAND 0x1302  // IssuePixieCommand(a0=id, a1=opcode, a2=flags, a3=args[4]ptr) -> a0=bool
#define MISL_PIX_WRITE 0x1303    // WritePixieRAM(a0=id, a1=offset, a2=dataptr, a3=size) -> a0=bool
#define MISL_PIX_GET_RAM_SIZE 0x1304       // GetPixieRAMSize(a0=id) -> a0
#define MISL_PIX_READ_STRING 0x1305        // ReadPixieString(a0=id, a1=outbufptr, a2=maxlen) -> a0=len
#define MISL_PIX_SET_POSITION 0x1306       // SetPixiePosition(a0=id, a1=x, a2=y as float) -> a0=bool
#define MISL_PIX_SET_DISPLAY_SIZE 0x1307   // SetPixieDisplaySize(a0=id, a1=w, a2=h as float) -> a0=bool
#define MISL_PIX_SET_PRIORITY 0x1308       // SetPixiePriority(a0=id, a1=priority) -> a0=bool
#define MISL_PIX_SET_ENABLED 0x1309        // SetPixieEnabled(a0=id, a1=bool) -> a0=bool
#define MISL_PIX_SHOW 0x130A               // ShowPixie(a0=id) -> a0=bool
#define MISL_PIX_HIDE 0x130B               // HidePixie(a0=id) -> a0=bool
#define MISL_PIX_SET_DRAW_PATTERN 0x130C   // SetPixieDrawPattern(a0=id, a1=pattern) -> a0=bool
#define MISL_PIX_GET_DRAW_PATTERN 0x130D   // GetPixieDrawPattern(a0=id) -> a0
#define MISL_PIX_GET_INITIALIZED 0x130E    // GetPixieInitialized(a0=id) -> a0=bool
#define MISL_PIX_GET_MODE 0x130F           // GetPixieMode(a0=id) -> a0
#define MISL_PIX_GET_OUT_WIDTH 0x1310      // GetPixieOutputWidth(a0=id) -> a0
#define MISL_PIX_GET_OUT_HEIGHT 0x1311     // GetPixieOutputHeight(a0=id) -> a0
#define MISL_PIX_GET_X 0x1312              // GetPixieX(a0=id) -> a0=float
#define MISL_PIX_GET_Y 0x1313              // GetPixieY(a0=id) -> a0=float
#define MISL_PIX_GET_DISPLAY_WIDTH 0x1314  // GetPixieDisplayWidth(a0=id) -> a0=float
#define MISL_PIX_GET_DISPLAY_HEIGHT 0x1315 // GetPixieDisplayHeight(a0=id) -> a0=float
#define MISL_PIX_GET_PRIORITY 0x1316       // GetPixiePriority(a0=id) -> a0
#define MISL_PIX_GET_ENABLED 0x1317        // GetPixieEnabled(a0=id) -> a0=bool
#define MISL_PIX_GET_SHOWN 0x1318          // GetPixieShown(a0=id) -> a0=bool
#define MISL_PIX_SET_PATTERN_BITMAP                                                                          \
    0x1319 // SetPixiePatternBitmap(a0=id, a1=ramOffset, a2=w, a3=h) -> a0=bool
#define MISL_PIX_CLEAR_PATTERN_BITMAP 0x131A // ClearPixiePatternBitmap(a0=id) -> a0=bool
#define MISL_PIX_SUBMIT 0x131B               // RunPixieCommandList(a0=count) -> a0=succeeded
#define MISL_PIX_SET_TRANSFORM                                                                               \
    0x131C // SetPixieTransform(a0=id, a1=slot, a2=tx, a3=ty, a4=sx, a5=sy, a6=degrees as float) -> a0=bool
#define MISL_PIX_SET_TRANSFORM_M                                                                             \
    0x131D // SetPixieTransformMatrix(a0=id, a1=slot, a2=PixieTransform ptr) -> a0=bool
#define MISL_PIX_GET_TRANSFORM_M 0x131E   // GetPixieTransformMatrix(a0=id, a1=slot, a2=out ptr) -> a0=bool
#define MISL_PIX_RESET_TRANSFORM 0x131F   // ResetPixieTransform(a0=id, a1=slot) -> a0=bool
#define MISL_PIX_SET_CUR_TRANSFORM 0x1320 // SetPixieCurrentTransform(a0=id, a1=slot) -> a0=bool
#define MISL_PIX_SET_MASK_INVERT 0x1321   // SetPixieMaskInvert(a0=id, a1=bool) -> a0=bool
#define MISL_PIX_GET_MASK_INVERT 0x1322   // GetPixieMaskInvert(a0=id) -> a0=bool
#define MISL_PIX_SET_ROTATION 0x1323      // SetPixieRotation(a0=id, a1=radians as float) -> a0=bool
#define MISL_PIX_GET_ROTATION 0x1324      // GetPixieRotation(a0=id) -> a0=radians as float
#define MISL_PIX_SET_HOTSPOT 0x1325       // SetPixieHotspot(a0=id, a1=x, a2=y as float) -> a0=bool
#define MISL_PIX_GET_HOTSPOT_X 0x1326     // GetPixieHotspotX(a0=id) -> a0=float
#define MISL_PIX_GET_HOTSPOT_Y 0x1327     // GetPixieHotspotY(a0=id) -> a0=float
#define MISL_PIX_SET_ROT_HOTSPOT 0x1328   // SetPixieRotateAroundHotspot(a0=id, a1=bool) -> a0=bool
#define MISL_PIX_GET_ROT_HOTSPOT 0x1329   // GetPixieRotateAroundHotspot(a0=id) -> a0=bool
#define MISL_PIX_SET_FILTER_LINEAR 0x132A // SetPixieFilterLinear(a0=id, a1=bool) -> a0=bool
#define MISL_PIX_GET_FILTER_LINEAR 0x132B // GetPixieFilterLinear(a0=id) -> a0=bool

// Colors
#define MISL_PAL_PACK_RGBA8 0x1400       // PackColorsRGBA8(a0=Color) -> a0=uint32
#define MISL_PAL_LOAD_FROM_SPRITE 0x1401 // SetColorsPalette(a0=palIdx, a1=Color[16]ptr) -> a0=bool
#define MISL_PAL_GET_FROM_CHAR 0x1402    // GetColorsColorFromChar(a0=char) -> a0=Color
#define MISL_PAL_GET_COMMODORE 0x1403    // GetColorsCommodoreColor(a0=idx) -> a0=Color
#define MISL_PAL_GET_TANDY 0x1404        // GetColorsTandyColor(a0=idx) -> a0=Color
#define MISL_PAL_GET_ANSI 0x1405         // GetColorsANSIColor(a0=idx) -> a0=Color
#define MISL_PAL_SET 0x1406              // SetColorsPaletteColor(a0=palIdx, a1=colorIdx, a2=Color) -> a0=bool
#define MISL_PAL_GET 0x1407              // GetColorsPaletteColor(a0=palIdx, a1=colorIdx) -> a0=Color
#define MISL_PAL_GET_GENERATION 0x1408   // GetColorsGeneration() -> a0
#define MISL_PAL_DEEP_SET_ALL 0x1409     // SetColorsDeepPalette(a0=palIdx 0-15, a1=Color[256]ptr) -> a0=bool
#define MISL_PAL_DEEP_SET 0x140A // SetColorsDeepPaletteColor(a0=palIdx, a1=colorIdx, a2=Color) -> a0=bool
#define MISL_PAL_DEEP_GET 0x140B // GetColorsDeepPaletteColor(a0=palIdx, a1=colorIdx) -> a0=Color
#define MISL_PAL_DEEP_GENERATION 0x140C // GetColorsDeepGeneration() -> a0

// Interactions
#define MISL_COLLISION_BOUNDING_BOX (1u << 0)
#define MISL_COLLISION_ANY_COLOR (1u << 1)
#define MISL_COLLISION_COLLIDABLE_COLORS (1u << 2)
#define MISL_IACT_DIST_SPRITES 0x1500 // GetInteractionsSpriteDistance(a0=A, a1=B) -> a0=float
#define MISL_IACT_DIR_SPRITES 0x1501  // GetInteractionsSpriteDirection(a0=A, a1=B) -> a0=float
#define MISL_IACT_LOCALPIX_TO_WORLD                                                                          \
    0x1502 // GetInteractionsSpritePixelWorldPosition(a0=idx, a1=lx, a2=ly,
           //   a3=outptr wx, a4=outptr wy) -> a0=bool
#define MISL_IACT_SPRITE_TILE_COLLISION                                                                      \
    0x1503 // CheckInteractionsSpriteTileCollision(a0=idx, a1=layer, a2=types) -> a0=mask
#define MISL_IACT_SPRITE_TILE_CELLS                                                                          \
    0x1504 // GetInteractionsSpriteTileCollisionCells(a0=idx, a1=layer,
           //   a2=outptr TileCellRef[a3], a3=max) -> a0=count found
#define MISL_IACT_SPRITE_TILE_BATCH                                                                          \
    0x1505 // CheckInteractionsSpriteTileCollisionBatch(a0=ptr int16[a1], a1=count, a2=layer,
           //   a3=types, a4=outptr results[a1]) -> a0=sprites matched (T164)

// Input
#define MISL_CAKE_GET_KEY 0x2000          // read CAKE_Keys[a0=scancode] -> a0=0/1
#define MISL_CAKE_GET_MOUSE_X 0x2001      // CAKE_MouseDeltaX -> a0
#define MISL_CAKE_GET_MOUSE_Y 0x2002      // CAKE_MouseDeltaY -> a0
#define MISL_CAKE_GET_MOUSE_WHEEL 0x2003  // CAKE_MouseWheel -> a0
#define MISL_CAKE_GET_MOUSE_BUTTON 0x2004 // read CAKE_MouseButtons[a0=btn] -> a0=0/1
#define MISL_CAKE_IS_SILENCED 0x2005      // CAKE_IsSilenced() -> a0
#define MISL_CAKE_CTRL_CONNECTED 0x2006   // CAKE_IsControllerConnected(a0=slot) -> a0
#define MISL_CAKE_CTRL_CONN_STATE 0x2007  // CAKE_GetControllerConnectionState(a0=slot) -> a0
#define MISL_CAKE_CTRL_BACKEND 0x2008     // CAKE_GetControllerBackend(a0=slot) -> a0
#define MISL_CAKE_CTRL_VENDOR 0x2009      // CAKE_GetControllerVendorID(a0=slot) -> a0
#define MISL_CAKE_CTRL_PRODUCT 0x200A     // CAKE_GetControllerProductID(a0=slot) -> a0
#define MISL_CAKE_CTRL_XINPUT_IDX 0x200B  // CAKE_GetControllerXInputIndex(a0=slot) -> a0
#define MISL_CAKE_CTRL_VIBRATION 0x200C   // CAKE_SetControllerVibration(a0=slot, a1=left, a2=right) -> a0
#define MISL_CAKE_CTRL_COPY_NAME                                                                             \
    0x200D // copy CAKE_GetControllerName(a0=slot) into guest
           //   (a1=outbufptr, a2=maxlen) -> a0=len

// SHARP audio
#define MISL_SHARP_AVAILABLE 0x3000   // SHARP_IsAvailable() -> a0=bool
#define MISL_SHARP_SAMPLE_RATE 0x3001 // SHARP_GetSampleRate() -> a0
#define MISL_SHARP_POSITION 0x3002    // SHARP_GetRenderPosition() -> a0:a1=uint64
#define MISL_SHARP_CREATE_CHIP 0x3003 // SHARP_CreateChip(a0=kind) -> a0=id or -1
#define MISL_SHARP_CHIP_COUNT 0x3004  // SHARP_GetChipCount() -> a0
#define MISL_SHARP_POOL_SIZE 0x3005   // SHARP_GetPoolSize() -> a0=bytes
#define MISL_SHARP_POOL_WRITE 0x3006  // SHARP_WritePool(a0=offset, a1=srcptr, a2=bytes) -> a0=bool
#define MISL_SHARP_WRITE 0x3007       // SHARP_Write(a0=chip, a1=reg, a2=value) -> a0=bool
#define MISL_SHARP_WRITE_AT                                                                                  \
    0x3008                               // SHARP_WriteAt(a0=chip, a1=reg, a2=value,
                                         //   a3:a4=sample position) -> a0=bool
#define MISL_SHARP_EVENTS_DROPPED 0x3009 // SHARP_GetDroppedEventCount() -> a0
#define MISL_SHARP_EVENTS_LATE 0x300A    // SHARP_GetLateEventCount() -> a0
#define MISL_SHARP_SET_CHIP_BUS 0x3010   // SHARP_SetChipBus(a0=chip, a1=bus) -> a0=bool
#define MISL_SHARP_CHIP_BUS 0x3011       // SHARP_GetChipBus(a0=chip) -> a0=bus or -1
#define MISL_SHARP_ENABLE_BUS 0x3012     // SHARP_EnableBus(a0=bus, a1=enabled) -> a0=bool
#define MISL_SHARP_BUS_VOLUME 0x3013     // SHARP_SetBusVolume(a0=bus, a1=float) -> a0=bool
#define MISL_SHARP_BUS_BALANCE 0x3014    // SHARP_SetBusBalance(a0=bus, a1=float) -> a0=bool
#define MISL_SHARP_MASTER_VOLUME 0x3015  // SHARP_SetMasterVolume(a0=float)

// DICE random values and timers
#define MISL_DICE_INIT_RNG 0x4000        // DICE_InitRNG(a0=handle, a1=mode, a2:a3=seed) -> a0=bool
#define MISL_DICE_RELEASE_RNG 0x4001     // DICE_ReleaseRNG(a0=handle)
#define MISL_DICE_RNG_MIX_ENTROPY 0x4002 // DICE_MixRNGEntropy(a0=handle, a1=dataptr, a2=len)
#define MISL_DICE_RNG_GET_SEED 0x4003    // DICE_GetRNGSeed(a0=handle) -> a0:a1=uint64
#define MISL_DICE_RAND_UINT 0x4004       // DICE_RandUint(a0=handle) -> a0
#define MISL_DICE_RAND_INT 0x4005        // DICE_RandInt(a0=handle, a1=min, a2=max) -> a0
#define MISL_DICE_RAND_FLOAT 0x4006      // DICE_RandFloat(a0=handle) -> a0=float
#define MISL_DICE_RAND_CHANCE 0x4007     // DICE_RandChance(a0=handle, a1=fprob) -> a0=bool
#define MISL_DICE_INIT_TIMER 0x4010 // DICE_InitTimerInterval(a0=handle, a1:a2=usec, a3=autostart) -> a0=bool
#define MISL_DICE_RELEASE_TIMER 0x4011    // DICE_ReleaseTimer(a0=handle)
#define MISL_DICE_TIMER_START 0x4012      // DICE_StartTimer(a0=handle) -> a0=bool
#define MISL_DICE_TIMER_STOP 0x4013       // DICE_StopTimer(a0=handle) -> a0=bool
#define MISL_DICE_TIMER_PAUSE 0x4014      // DICE_PauseTimer(a0=handle) -> a0=bool
#define MISL_DICE_TIMER_RESUME 0x4015     // DICE_ResumeTimer(a0=handle) -> a0=bool
#define MISL_DICE_TIMER_IS_RUNNING 0x4016 // DICE_IsTimerRunning(a0=handle) -> a0=bool
#define MISL_DICE_TIMER_CONSUME 0x4017    // DICE_ConsumeTimer(a0=handle) -> a0=ticks
#define MISL_DICE_TIMER_DELTA 0x4018      // DICE_GetTimerDelta(a0=handle) -> a0:a1=double
#define MISL_DICE_SEED_DIE 0x4020         // DICE_SeedDie(a0=die, a1:a2=seed) -> a0=bool
#define MISL_DICE_DIE_RANGE 0x4021        // DICE_RollDieRange(a0=die, a1=min, a2=max) -> a0
#define MISL_DICE_DIE_FLOAT 0x4022        // DICE_RollDieFloat(a0=die) -> a0=float
#define MISL_DICE_DIE_CHANCE 0x4023       // DICE_RollDieChance(a0=die, a1=fprob) -> a0=bool
#define MISL_DICE_SEED_SEQUENCE 0x4024    // DICE_SeedSequence(a0=seq, a1:a2=seed) -> a0=bool
#define MISL_DICE_SEQ_UINT 0x4025         // DICE_GetSequenceUint(a0=seq, a1:a2=pos) -> a0
#define MISL_DICE_SEQ_INT 0x4026          // DICE_GetSequenceInt(a0=seq, a1:a2=pos, a3=min, a4=max) -> a0
#define MISL_DICE_SEQ_FLOAT 0x4027        // DICE_GetSequenceFloat(a0=seq, a1:a2=pos) -> a0=float
#define MISL_DICE_SEQ_CHANCE 0x4028       // DICE_GetSequenceChance(a0=seq, a1:a2=pos, a3=fprob) -> a0=bool
#define MISL_DICE_ALLOC_TIMER 0x4029      // DICE_AllocTimer() -> a0 = handle, or -1 if none free
#define MISL_DICE_REAL_TIME                                                                                  \
    0x402A // DICE_GetRealTime(a0=MislClock*) -> a0=bool; the LIVE form of the clock page

// System ROM services
#define MISL_ROM_MEMCPY 0x5000      // memcpy(a0=dst, a1=src, a2=n) -> a0=dst
#define MISL_ROM_MEMSET 0x5001      // memset(a0=dst, a1=c, a2=n) -> a0=dst
#define MISL_ROM_MEMMOVE 0x5002     // memmove(a0=dst, a1=src, a2=n) -> a0=dst
#define MISL_ROM_MEMCMP 0x5003      // memcmp(a0, a1, a2=n) -> a0=int
#define MISL_ROM_STRLEN 0x5004      // strlen(a0=s) -> a0
#define MISL_ROM_ASHLDI3 0x5005     // __ashldi3(a0:a1=u, a2=b) -> a0:a1
#define MISL_ROM_LSHRDI3 0x5006     // __lshrdi3(a0:a1=u, a2=b) -> a0:a1
#define MISL_ROM_ASHRDI3 0x5007     // __ashrdi3(a0:a1=u, a2=b) -> a0:a1
#define MISL_ROM_UDIVDI3 0x5008     // __udivdi3(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_UMODDI3 0x5009     // __umoddi3(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_DIVDI3 0x500A      // __divdi3(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_MODDI3 0x500B      // __moddi3(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_FLOATUNDIDF 0x500C // __floatundidf(a0:a1) -> a0:a1=double
#define MISL_ROM_FLOATDIDF 0x500D   // __floatdidf(a0:a1) -> a0:a1=double
#define MISL_ROM_FIXUNSDFDI 0x500E  // __fixunsdfdi(a0:a1=double) -> a0:a1
#define MISL_ROM_FIXDFDI 0x500F     // __fixdfdi(a0:a1=double) -> a0:a1
#define MISL_ROM_FLOATUNDISF 0x5010 // __floatundisf(a0:a1) -> a0=float
#define MISL_ROM_FLOATDISF 0x5011   // __floatdisf(a0:a1) -> a0=float
#define MISL_ROM_FIXUNSSFDI 0x5012  // __fixunssfdi(a0=float) -> a0:a1
#define MISL_ROM_FIXSFDI 0x5013     // __fixsfdi(a0=float) -> a0:a1
#define MISL_ROM_TRACE 0x5014       // MISL_Trace(a0=str) -> a0=bool printed; rate-limited, debug hosts only
#define MISL_ROM_APPENDSTR 0x5015   // append(a0=buffer, a1=capacity, a2=at, a3=string) -> a0=end offset
#define MISL_ROM_APPENDINT 0x5016   // append(a0=buffer, a1=capacity, a2=at, a3=int) -> a0=end offset
#define MISL_ROM_APPENDFIXED                                                                                 \
    0x5017                     // append(a0=buffer, a1=capacity, a2=at, a3=int, a4=decimals) -> a0=end offset
#define MISL_ROM_SINF 0x5018   // sinf(a0=float) -> a0=float
#define MISL_ROM_COSF 0x5019   // cosf(a0=float) -> a0=float
#define MISL_ROM_TANF 0x501A   // tanf(a0=float) -> a0=float
#define MISL_ROM_ATAN2F 0x501B // atan2f(a0, a1) -> a0
#define MISL_ROM_EXPF 0x501C   // expf(a0=float) -> a0=float
#define MISL_ROM_LOGF 0x501D   // logf(a0=float) -> a0=float
#define MISL_ROM_POWF 0x501E   // powf(a0, a1) -> a0
#define MISL_ROM_FLOORF 0x501F // floorf(a0=float) -> a0=float
#define MISL_ROM_CEILF 0x5020  // ceilf(a0=float) -> a0=float
#define MISL_ROM_TRUNCF 0x5021 // truncf(a0=float) -> a0=float
#define MISL_ROM_ROUNDF 0x5022 // roundf(a0=float) -> a0=float
#define MISL_ROM_SIN 0x5023    // sin(a0:a1=double) -> a0:a1
#define MISL_ROM_COS 0x5024    // cos(a0:a1=double) -> a0:a1=double
#define MISL_ROM_TAN 0x5025    // tan(a0:a1=double) -> a0:a1=double
#define MISL_ROM_ATAN2 0x5026  // atan2(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_EXP 0x5027    // exp(a0:a1=double) -> a0:a1=double
#define MISL_ROM_LOG 0x5028    // log(a0:a1=double) -> a0:a1=double
#define MISL_ROM_POW 0x5029    // pow(a0:a1, a2:a3) -> a0:a1
#define MISL_ROM_FLOOR 0x502A  // floor(a0:a1=double) -> a0:a1=double
#define MISL_ROM_CEIL 0x502B   // ceil(a0:a1=double) -> a0:a1=double
#define MISL_ROM_TRUNC 0x502C  // trunc(a0:a1=double) -> a0:a1=double
#define MISL_ROM_ROUND 0x502D  // round(a0:a1=double) -> a0:a1=double
#define MISL_ROM_SRAMWRITE                                                                                   \
    0x502E // MISL_SramWrite(a0=offset, a1=src, a2=len) -> a0=1 on success, 0 for zero bytes; invalid range
           // halts
