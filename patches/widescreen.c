#include "patches.h"

#if 0
RECOMP_PATCH Gfx* dynGetMasterDisplayList(void) {
    g_GfxRequestedDisplayList = TRUE;

    Gfx* gdl = (Gfx*) g_GfxBuffers[g_GfxActiveBufferIndex];

    // @recomp: Enable RT64 Extended GBI
    gEXEnable(gdl++);
    gEXSetRefreshRate(gdl++, 60 / speedgraphframes);

    return gdl;
}
#endif

// Set viewports (single player)
#if 1
RECOMP_PATCH Gfx* zbufClearCurrentPlayer(Gfx* gdl) {
    s32 start_x;
    s32 end_x;

    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, z_buffer_width, OS_K0_TO_PHYSICAL(z_buffer));
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));

    // @recomp: use gEXSetScissor instead
    // gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());
    gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);

    if (getPlayerCount() < 3) {
        start_x = 0;
        end_x = viGetX() - 1;
    } else if ((get_cur_playernum() == 0) || (get_cur_playernum() == 2)) {
        start_x = 0;
        end_x = (viGetX() / 2) - 1;
    } else {
        start_x = viGetX() / 2;
        end_x = viGetX() - 1;
    }

    gDPFillRectangle(gdl++, start_x, 0, end_x + 240, (z_buffer_height - 1));
    gDPPipeSync(gdl++);

    return gdl;
}
#endif

// A room seen through a portal is drawn cut to the portal's rectangle on screen, and so are its props and guards: the
// game's own occlusion. Rooms beyond a portal are drawn after the rooms in front of them, the walls around a portal
// don't hide what is behind them, and some of the game's geometry (its terrain and backdrops) is drawn without the Z
// buffer, so without the cut a room seen through a portal draws over what is in front of it: a building seen through
// Surface's trees, Frigate's bridge consoles on the bridge's outer wall.
//
// The rectangle is in the game's 4:3 coordinates, and a widescreen view goes past them on both sides
// (bgUpdateCurrentPlayerScreenMinMax widens the bounds the game makes it in): x 160 +- half, half = 120 * aspect. RT64
// draws a projection widened only if its draws' scissors together span the whole screen, and the game loads its
// projection again for every room, so a room cut to its portal would be drawn 4:3, squeezed into the middle. A room cut
// narrower than the screen is given one empty triangle (three times the same vertex) under the whole screen's scissor
// first, which keeps its projection widened, and then its rectangle in the widened projection's terms: RT64 spreads a
// scissor's 0-320 over the whole widescreen view there, so x is scaled by 160 / half about the middle.
static s32 scissorX(f32 x, f32 half, s32 roundup) {
    f32 scaled = 160.0f + (x - 160.0f) * (160.0f / half);
    s32 rounded = (s32) scaled;

    if (roundup && (rounded < scaled)) {
        rounded++;
    } else if (!roundup && (rounded > scaled)) {
        rounded--;
    }

    return (rounded < 0) ? 0 : ((rounded > 320) ? 320 : rounded);
}

#if 1
RECOMP_PATCH Gfx* bgScissorCurrentPlayerView(Gfx* gdl, s32 left, s32 top, s32 width, s32 height) {
    struct player* player = g_CurrentPlayer;
    f32 half = 120.0f * recomp_get_aspect_ratio(4.0f / 3.0f);

    // @recomp: the rectangle in the widescreen view, its edges at the view's edges opened to the whole screen
    if (top <= (s32) player->viewtop) {
        top = 0;
    }

    if (height >= (s32) (player->viewtop + player->viewy)) {
        height = 240;
    }

    if (height < top) {
        height = top;
    }

    left = scissorX(left, half, FALSE);
    width = scissorX(width, half, TRUE);

    if (width < left) {
        width = left;
    }

    if ((left > 0) || (width < 320)) {
        Vtx* vtx = dynAllocateVertices(1);
        u32* words = (u32*) vtx;

        words[0] = words[1] = words[2] = words[3] = 0;
        gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);
        gSPVertex(gdl++, OS_K0_TO_PHYSICAL(vtx), 1, 0);
        gSP1Triangle(gdl++, 0, 0, 0, 0);

        // @recomp: use gEXSetScissor instead
        // gDPSetScissor(gdl++, G_SC_NON_INTERLACE, left, top, width, height);
        gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, left, top, width, height);
    } else {
        gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, top, 0, height);
    }

    return gdl;
}

// The whole view, which the game gives as the 4:3 view's rectangle: the widescreen one.
RECOMP_PATCH Gfx* bgScissorCurrentPlayerViewDefault(Gfx* gdl) {
    gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);

    return gdl;
}
#endif

#if 1
RECOMP_PATCH void modelSetDistanceDisabled(s32 param_1) {
    // @recomp: ModelDistance always disabled
    if ((g_StageNum == LEVELID_JUNGLE)) {
        g_ModelDistanceDisabled = 0;
    } else {
        g_ModelDistanceDisabled = 1;
    }
}
#endif

#if 1
RECOMP_PATCH Gfx* currentPlayerDrawFade(Gfx* gdl) {
    f32 frac = g_CurrentPlayer->colourscreenfrac;
    s32 r = g_CurrentPlayer->colourscreenred;
    s32 g = g_CurrentPlayer->colourscreengreen;
    s32 b = g_CurrentPlayer->colourscreenblue;
    if ((cameraFrameCounter1 != 0) || (cameraFrameCounter2 != 0)) {
        frac = 1.0f;
        b = 0;
        g = 0;
        r = 0;
    }
    if (frac > 0) {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetColorDither(gdl++, G_CD_DISABLE);
        gDPSetTexturePersp(gdl++, G_TP_NONE);
        gDPSetAlphaCompare(gdl++, G_AC_NONE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetTextureConvert(gdl++, G_TC_FILT);
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, r, g, b, (s32) (frac * 255.0f));

        // gDPFillRectangle(gdl++, viGetViewLeft(), viGetViewTop(), (viGetViewLeft() + viGetViewWidth()),
        // (viGetViewTop() + viGetViewHeight()));

        // @recomp: Remove margins
        gDPFillRectangle(gdl++, 0, 0, 320, 240);

        gDPPipeSync(gdl++);
        gDPSetColorDither(gdl++, G_CD_BAYER);
        gDPSetTexturePersp(gdl++, G_TP_PERSP);
        gDPSetTextureLOD(gdl++, G_TL_LOD);
    }

    return gdl;
}
#endif

// @recomp: Culling of objects on the sides.
#if 1

extern int demoMode;

RECOMP_PATCH void bgUpdateCurrentPlayerScreenMinMax(void) /* @theboy181 - WS hacks */
{
    f32 fx, fy, fwidth, fheight;

    if (demoMode != 0 || (g_StageNum == LEVELID_JUNGLE && intro_camera_index == CAMERAMODE_INTRO)) {
        /* Use global bgViewRelated values (original min/max) */
        fx = (f32) bgViewRelated[0];
        fy = (f32) bgViewRelated[1];
        fwidth = (f32) viGetX() + (f32) bgViewRelated[2];
        fheight = (f32) viGetY() + (f32) bgViewRelated[3];

        /* X min */
        g_CurrentPlayer->screenxminf = (f32) viGetViewLeft();
        if (g_CurrentPlayer->screenxminf < fx)
            g_CurrentPlayer->screenxminf = fx;
        if (fwidth < g_CurrentPlayer->screenxminf)
            g_CurrentPlayer->screenxminf = fwidth;

        /* Y min */
        g_CurrentPlayer->screenyminf = (f32) viGetViewTop();
        if (g_CurrentPlayer->screenyminf < fy)
            g_CurrentPlayer->screenyminf = fy;
        if (fheight < g_CurrentPlayer->screenyminf)
            g_CurrentPlayer->screenyminf = fheight;

        /* X max */
        g_CurrentPlayer->screenxmaxf = (f32) (viGetViewLeft() + viGetViewWidth());
        if (g_CurrentPlayer->screenxmaxf < fx)
            g_CurrentPlayer->screenxmaxf = fx;
        if (fwidth < g_CurrentPlayer->screenxmaxf)
            g_CurrentPlayer->screenxmaxf = fwidth;

        /* Y max */
        g_CurrentPlayer->screenymaxf = (f32) (viGetViewTop() + viGetViewHeight());
        if (g_CurrentPlayer->screenymaxf < fy)
            g_CurrentPlayer->screenymaxf = fy;
        if (fheight < g_CurrentPlayer->screenymaxf)
            g_CurrentPlayer->screenymaxf = fheight;
    } else {
        /* Widescreen hack/fps fix path */
        fx = -320.0f * 2.0f;
        fy = 0.0f;
        fwidth = (f32) viGetX() * 3.0f;
        fheight = (f32) viGetY();

        /* X min */
        g_CurrentPlayer->screenxminf = (f32) viGetViewLeft() + 320.0f * -4.0f;
        if (g_CurrentPlayer->screenxminf < fx)
            g_CurrentPlayer->screenxminf = fx;
        if (fwidth < g_CurrentPlayer->screenxminf)
            g_CurrentPlayer->screenxminf = fwidth;

        /* Y min */
        g_CurrentPlayer->screenyminf = (f32) viGetViewTop();
        if (g_CurrentPlayer->screenyminf < fy)
            g_CurrentPlayer->screenyminf = fy;
        if (fheight < g_CurrentPlayer->screenyminf)
            g_CurrentPlayer->screenyminf = fheight;

        /* X max */
        g_CurrentPlayer->screenxmaxf = (f32) (viGetViewLeft() + viGetViewWidth() + 320.0f * 4.0f);
        if (g_CurrentPlayer->screenxmaxf < fx)
            g_CurrentPlayer->screenxmaxf = fx;
        if (fwidth < g_CurrentPlayer->screenxmaxf)
            g_CurrentPlayer->screenxmaxf = fwidth;

        /* Y max */
        g_CurrentPlayer->screenymaxf = (f32) (viGetViewTop() + viGetViewHeight());
        if (g_CurrentPlayer->screenymaxf < fy)
            g_CurrentPlayer->screenymaxf = fy;
        if (fheight < g_CurrentPlayer->screenymaxf)
            g_CurrentPlayer->screenymaxf = fheight;
    }
}
#endif

#if 1
extern int demoMode;

RECOMP_PATCH f32 getinstsize(Model* arg0) // @theboy181
{
    f32 ret = 0.0f;

#if defined(LEFTOVERDEBUG)
    if (arg0 == NULL) {
        osSyncPrintf("getinstsize: no objinst!\n");
        return_null();
    }

    if (arg0->obj == NULL) {
        osSyncPrintf("getinstsize: no objdesc!\n");
        return_null();
    }
#endif

    if (demoMode == 0) { // @theboy181 - fps fix - Demo timing fix
        ret = arg0->obj->BoundingVolumeRadius * arg0->scale * 4;
    } else {
        ret = arg0->obj->BoundingVolumeRadius * arg0->scale;
    } // @recomp:

    return ret;
}
#endif

#if 1
RECOMP_PATCH f32 getPlayer_c_lodscalez(void) {
    return g_CurrentPlayer->c_lodscalez / 8; // @theboy181 - Proper LOD fix?
}
#endif

#if 0
RECOMP_PATCH s16 get_curplayer_viewport_ulx(void)
{
    if (2 < getPlayerCount())
    {
        if ((get_cur_playernum() == 1) || (get_cur_playernum() == 3))
        {
                return 161 ;
        }
    }

    return 0;
}
#endif

#if 0
extern ModelRenderData D_80031FD0;
s32 fogGetPropDistColor(PropRecord* prop, struct rgba_f32* color);
f32 chrobjFogVisRangeRelated(PropRecord* prop, f32 size);
f32 getinstsize(Model* arg0);
s32 sub_GAME_7F054A64(PropRecord* prop, bbox2d* bbox);
Gfx* bgScissorCurrentPlayerViewF(Gfx* arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4);
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* arg0);
s32 objGetShotsTaken(ObjectRecord* obj);
void sub_GAME_7F040384(rgba_s32* arg0, s32 arg1, rgba_f32* arg2);
void sub_GAME_7F04AC20(PropRecord* prop, ModelRenderData*, s32 arg2);

RECOMP_PATCH Gfx* chrobjRenderProp(PropRecord* prop, Gfx* gdl, s32 arg2) __attribute__((optnone)) {
    struct rgba_f32 spB0;
    s32 spAC;
    s32 spA8;
    ModelRenderData mrData;
    struct view4f sp58;
    struct rgba_s32 sp48;
    s32 sp44;
    ObjectRecord* obj;
    s32 objAlpha;
    f32 temp_f0;
    s32 temp_v0_4;
    s32 phi_a0;

    obj = prop->obj;

    mrData = D_80031FD0;

    objAlpha = 0xFF;
    spAC = fogGetPropDistColor(prop, &spB0);

    if (spAC == 0) {
        return gdl;
    }

    if ((u8) (((PropDefHeaderRecord*)obj)->type) != PROPDEF_TINTED_GLASS) {
        temp_f0 = chrobjFogVisRangeRelated(prop, getinstsize(obj->model));

        if (((s32) prop->timetoregen > 0) && ((s32) prop->timetoregen < CHROBJ_TIMETOREGEN)) {
            temp_f0 *= ((CHROBJ_TIMETOREGEN_F - (f32) prop->timetoregen) / CHROBJ_TIMETOREGEN_F);
        }

        objAlpha = (s32) (temp_f0 * 255.0f);

        if (objAlpha <= 0) {
            return gdl;
        }
    }

    if ((objAlpha < 0xFF) || (obj->flags2 & 0x10000)) {
        if (arg2 == 0) {
            return gdl;
        }

        sp44 = 3;
    } else {

        sp44 = (arg2 == 0) ? 1 : 2;
    }

    if ((sub_GAME_7F054A64(prop, &sp58) > 0) && (((s32) obj->flags2 << 5) >= 0)) {
        gdl = bgScissorCurrentPlayerViewF(gdl, sp58.left, sp58.top, sp58.width, sp58.height);
    } else {
        gdl = bgScissorCurrentPlayerViewDefault(gdl);
    }

    mrData.flags = sp44;
    mrData.zbufferenabled = (obj->flags2 & 0x10000) == 0;

    mrData.gdl = gdl;

    if (objAlpha < 0xFF) {
        mrData.PropType = 5;
        mrData.envcolour.word = objAlpha;
    } else {
        mrData.PropType = 9;

        if (((PropDefHeaderRecord*)obj)->type == PROPDEF_TINTED_GLASS) {
            mrData.envcolour.word = ((struct TintedGlassRecord*) obj)->calculatedopacity << 8;
        } else if ((((PropDefHeaderRecord*)obj)->type == PROPDEF_DOOR) && ((((struct DoorRecord*) obj)->doorFlags & 2) != 0)) {
            mrData.envcolour.word = ((struct DoorRecord*) obj)->calculatedopacity << 8;
        } else {
            mrData.envcolour.word = 0;
        }
    }

    temp_v0_4 = objGetShotsTaken(obj);
    phi_a0 = 0xFF - (temp_v0_4 * 0x15);

    if (phi_a0 < 0) {
        phi_a0 = 0;
    }

    sp48.r = (s32) (obj->shadecol.rgba[0] * phi_a0) >> 8;
    sp48.g = (s32) (obj->shadecol.rgba[1] * phi_a0) >> 8;
    sp48.b = (s32) (obj->shadecol.rgba[2] * phi_a0) >> 8;
    sp48.a = obj->shadecol.rgba[3] + temp_v0_4 * 0xF;

    if (sp48.a >= 0x100) {
        sp48.a = 0xFF;
    }

    sub_GAME_7F040384(&sp48, spAC, &spB0);

    mrData.fogcolour.word =
        (sp48.rgba[0] << 0x18) | (sp48.rgba[1] << 0x10) | (sp48.rgba[2] << 0x08) | (sp48.rgba[3] << 0x00);

    sub_GAME_7F04AC20(prop, &mrData, arg2);

    return mrData.gdl;
}

#endif

// The gun barrel's spiral, in the intro and behind the file select's folders: a picture 440 wide, the front end's 4:3
// width, which the game draws a row at a time from x to the screen's right edge, x sliding it in from the right in the
// intro. Its first and last columns are black (the file select tints them grey), and the 4:3 view shows its spiral
// end in black on both sides as the intro's barrel pans, so a wider view gets the rows to the window's edges, the
// first column repeated left of the picture and the last right of it (s clamps to them).
#if 1
extern f32 D_8002BB00;
extern f32 D_8002BB04;
extern f32 D_8002BB08;
extern f32 D_8002BB0C;
extern f32 D_8002BB10;
extern f32 D_8002BB14;

struct FolderSelect {
    s32 unk00;
    s32 unk04;
    s32 unk08;
};

#define BARREL_WIDTH 440
#define BARREL_ROWS 299

// One row of the picture, from left to right (the front end's x, 0-440 the 4:3 view) and top to bottom (quarter
// pixels), its texels from s (texels) by dsdx a front-end unit.
static Gfx* barrelRow(Gfx* gdl, f32 left, f32 right, s32 top, s32 bottom, f32 s, f32 dsdx) {
    gEXTextureRectangle(gdl++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (s32) ((left - 220.0f) * 4.0f), top,
                        (s32) ((right - 220.0f) * 4.0f), bottom, G_TX_RENDERTILE, (s32) (s * 32.0f), 0,
                        (s32) (dsdx * 1024.0f), 1 << 10);

    return gdl;
}

// The picture is 299 rows, drawn from row 16 of the front end's 330, so the window shows black above and below it.
// Behind the file select it is zoomed by 330 / 299 about the view's middle, to fill the window's height (its rows 0-299
// over 0-330), and wider in proportion; not in the intro, whose sight and iris are drawn at the picture's hole.
RECOMP_PATCH Gfx* titleRenderFolderMenuBackgroundLines(Gfx* gdl, s32 image, s32 x, struct FolderSelect* top,
                                                       struct FolderSelect* bottom) {
    f32 r0 = top->unk00;
    f32 g0 = top->unk04;
    f32 b0 = top->unk08;
    f32 r1 = bottom->unk00;
    f32 g1 = bottom->unk04;
    f32 b1 = bottom->unk08;
    s32 zoomed = current_menu == MENU_FILE_SELECT;
    f32 zoom = zoomed ? (330.0f / BARREL_ROWS) : 1.0f;
    // The view's edges in the front end's x, a column past them.
    s32 half = (s32) (165.0f * recomp_get_aspect_ratio(4.0f / 3.0f)) + 1;
    f32 left = 220 - half;
    f32 right = 220 + half;
    // The picture's left edge zoomed, and its column at the view's left edge.
    f32 start = 220.0f + ((x - 220.0f) * zoom);
    f32 s = (left - start) / zoom;
    s32 i;

    D_8002BB0C = r1;
    D_8002BB10 = g1;
    D_8002BB14 = b1;
    gEXEnable(gdl++);
    gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, viGetY());

    for (i = 0; i < BARREL_ROWS; i++) {
        s32 rowTop = zoomed ? ((i * 330 * 4) / BARREL_ROWS) : ((i + 0x10) * 4);
        s32 rowBottom = zoomed ? (((i + 1) * 330 * 4) / BARREL_ROWS) : ((i + 0x11) * 4);

        gDPLoadTextureBlock(gdl++, image + (i * BARREL_WIDTH), G_IM_FMT_I, G_IM_SIZ_8b, BARREL_WIDTH, 1, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPSetPrimColor(gdl++, 0, 0, r0 + (((r1 - r0) * i) / 299.0f), g0 + (((g1 - g0) * i) / 299.0f),
                        b0 + (((b1 - b0) * i) / 299.0f), 255);

        // The picture, its first column left of it and its last right of it.
        gdl = barrelRow(gdl, left, right, rowTop, rowBottom, s, 1.0f / zoom);
    }

    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());
    D_8002BB08 = b0;
    D_8002BB04 = g0;
    D_8002BB00 = r0;

    return gdl;
}
#endif

// Mode, mission and difficulty select, the 007 options and the briefing draw the open folder close up, under the
// front end's 4:3 projection, which RT64 widens: the folder's cover, which fills the 4:3 view, ends short of a wider
// view's edges. A wider view gets the cover's texture across the whole view first, behind the folder, so the cover goes
// on to the window's edges: the folder model's own texture (its 11th), at the size, phase and shade the model draws it,
// measured from the game's own frames. The model maps it with t upward, so the rectangle steps t back down the screen.
// Its texels a front-end unit (5.10) and phase at the view's middle and top (10.5) are for a texture that repeats every
// 64 texels.
#if 1
#define COVER_TEXTURE 10
#define COVER_DSDX 281
#define COVER_DTDY 303
#define COVER_S_MIDDLE 1773
#define COVER_T_TOP 781

extern struct coord3d folderpositions[];
extern ModelRenderData D_8002AF84;
extern f32 D_8002AFC4;
extern f32 D_8002AFC8;
extern f32 D_8002AFCC;
void matrix_4x4_set_lookat_target(Mtxf* matrix, f32 posx, f32 posy, f32 posz, f32 lookx, f32 looky, f32 lookz,
                                  f32 upx, f32 upy, f32 upz);
void subdraw(ModelRenderData* renderdata, struct Model* model);

static Gfx* folderCover(Gfx* gdl) {
    ModelFileHeader* obj = walletinst[0]->obj;
    s32 half = (s32) (165.0f * recomp_get_aspect_ratio(4.0f / 3.0f)) + 1;

    gEXEnable(gdl++);
    gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, viGetY());
    gDPPipeSync(gdl++);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
    texSelect(&gdl, (struct sImageTableEntry*) &obj->Textures[COVER_TEXTURE], 4, 0, 0);
    // texSelect's own modes are for triangles (its combiner takes the vertices' shade, which a rectangle hasn't).
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPSetPrimColor(gdl++, 0, 0, 212, 212, 162, 255);
    gEXTextureRectangle(gdl++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -half * 4, 0, half * 4, 330 * 4,
                        G_TX_RENDERTILE, COVER_S_MIDDLE - ((half * COVER_DSDX) >> 5), COVER_T_TOP, COVER_DSDX,
                        -COVER_DTDY);
    gDPPipeSync(gdl++);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());

    return gdl;
}

RECOMP_PATCH Gfx* frontSetupMenuBackground(Gfx* DL) {
    f32 x;
    f32 y;
    ModelRenderData renderdata;
    s32 i;
    Mtxf view;
    Mtxf model;
    Mtxf tmp;

    // @recomp: the cover across the whole view, behind the folder
    DL = folderCover(DL);

    x = folderpositions[selected_folder_num].f[0];
    y = folderpositions[selected_folder_num].f[1];

    renderdata = D_8002AF84;

    x += D_8002AFC4;
    y += D_8002AFC8;

    matrix_4x4_set_lookat_target(&view, x, y, 4000.0f + D_8002AFCC, x, y, 0.0f, 0.0f, 1.0f, 0.0f);
    matrix_4x4_set_identity_and_position(&folderpositions[selected_folder_num], &model);
    matrix_scalar_multiply(0.25f, model.m[0]);
    matrix_4x4_multiply_in_place(&view, &model);

    renderdata.unk_matrix = &model;
    renderdata.mtxlist = dynAllocate(walletinst[0]->obj->numMatrices << 6);

    matrix_4x4_copy(&model, renderdata.mtxlist);

    walletinst[0]->render_pos = (RenderPosView*) renderdata.mtxlist;

    renderdata.flags = 3;
    renderdata.zbufferenabled = FALSE;
    renderdata.gdl = DL;

    subdraw(&renderdata, walletinst[0]);

    DL = renderdata.gdl;

    for (i = 0; i < walletinst[0]->obj->numMatrices; i++) {
        matrix_4x4_copy(&((Mtxf*) walletinst[0]->render_pos)[i], &tmp);
        matrix_4x4_f32_to_s32(&tmp, &((Mtxf*) walletinst[0]->render_pos)[i]);
    }

    return DL;
}
#endif

#if 1
extern s32 g_ContRumblePakInitState[4];
extern OSMesgQueue g_ContInputMessageQueue;
extern OSPfs g_ContPfs[4];
extern OSContStatus g_ContStatus[4];
RECOMP_PATCH void joyRumblePakInit(s32 index) {
    s32 ret;

#if 0
   if (g_ContRumblePakInitState[index] > -1) {
       recomp_printf("if (g_ContRumblePakInitState[index] > -1) {\n");
       if ((g_ContStatus[index].type & CONT_JOYPORT) && (g_ContStatus[index].status & CONT_CARD_ON)) {
           ret = osPfsInit_recomp(&g_ContInputMessageQueue, &g_ContPfs[index], index);
           recomp_printf ("ret: %d\n", ret);
           recomp_printf("if ((g_ContStatus[index].type & CONT_JOYPORT) && (g_ContStatus[index].status & CONT_CARD_ON)) {\n");

           if ((ret == PFS_ERR_ID_FATAL) || (ret == PFS_ERR_DEVICE)) {
               if (osMotorInit_recomp(&g_ContInputMessageQueue, &g_ContPfs[index], index) == 0) {
                   g_ContRumblePakInitState[index] = 1;
                   recomp_printf("RUMBLEPAKINITSTATE_READY\n");
               } else {
                   g_ContRumblePakInitState[index] = -1;
                   recomp_printf("RUMBLEPAKINITSTATE_ERROR\n");
               }
           }
       }
   }
#endif

    // @recomp: force rumble pak plugged in.
    g_ContRumblePakInitState[index] = 1;
}
#endif