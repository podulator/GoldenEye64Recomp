#include "patches.h"

// The in-game HUD at the window's edges in a wider window: the ammo counters, the messages at the top and bottom and
// the health and armour gauges sit as far from the window's sides as they sit from the 4:3 view's, rather than at the
// 4:3 view's positions in its middle. The crosshair, the gun and the countdown timer stay centred, and so do the
// watch's own gauges, which draw the same arcs through the watch's own display list. How far out is the HUD Placement
// setting's, as for every recomp using RT64's alignment: a window at most 16:9 wide by default.
//
// The game draws the counters' digits and icons and the messages' text as texture rectangles, which RT64's rectangle
// alignment moves to a side of the window, and the gauges as triangles under an orthographic projection, which its
// viewport alignment moves. A scissor as wide as the window lets them past the 4:3 view's edges. Each function is the
// decompilation's (src/game/gunfire.c, src/game/bondview2.c) with the changes marked @recomp. Split screen and a window
// no wider than 4:3 draw as the game does, command for command.

// The game's own, declared in no header the patches include.
typedef struct AmmoStats {
    u32 MaxAmmo;
    u32 IconImage;
    f32 IconYOffset;
} AmmoStats;

typedef struct {
    u8 labels[9][3];
} DirectionLabels;

extern AmmoStats ammo_related[];
extern u32 globalbank_rdram_offset;
extern char stringbuffer_lowerleft[5][0x65];
extern char stringbuffer_top[2][0x97];
extern s32 status_bar_text_buffer_index;
extern s32 copy_1stfonttable;
extern s32 copy_2ndfonttable;
extern s32 upper_text_buffer_index;
extern s32 upper_text_window_timer;
extern s32 g_UpperTextDisplayFlag;
extern void* ptrFontZurichBoldChars;
extern void* ptrFontZurichBold;
extern void* ptrFontBankGothicChars;
extern void* ptrFontBankGothic;
extern DirectionLabels g_DebugCompassLabels;
extern f32 D_800364CC;
extern u8 g_ViBackIndex;

s32 getCurrentPlayerWeaponId(s32 hand);
s32 get_ammo_type_for_weapon(s32 item);
s32 bondwalkItemCheckBitflags(s32 item, u32 flags);
s32 is_clock_drawn_onscreen(void);
s32 get_debug_testingmanpos_flag(void);
s8* bgDebPrintROOMID(s32 room);
f32 atan2f(f32 y, f32 x);
Gfx* set_rgba_redirect_generate_microcode(Gfx* gdl, void* tconfig, f32 x, f32 y, f32 arg4, s32 arg5, f32 arg6,
                                          s32 arg7);
Gfx* gunDrawHudInteger(Gfx* gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline);
Gfx* microcode_constructor(Gfx* gdl);
Gfx* combiner_bayer_lod_perspective(Gfx* gdl);
Gfx* draw_blackbox_to_screen(Gfx* gdl, s32* ulx, s32* uly, s32* lrx, s32* lry);
Gfx* microcode_constructor_related_to_menus(Gfx* gdl, s32 ulx, s32 uly, s32 lrx, s32 lry, u32 color);
void textMeasure(s32* textheight, s32* textwidth, char* text, void* chars, void* font, s32 lineheight);
Gfx* textRender(Gfx* gdl, s32* x, s32* y, char* text, void* chars, void* font, u32 colour, s32 width, s32 height,
                u32 yOffset, s32 lineheight);
Gfx* textRenderOutlined(Gfx* gdl, s32* x, s32* y, char* text, void* chars, void* font, u32 colour, u32 colour2,
                        s32 width, s32 height, s32 yOffset, s32 lineheight);
void hudMakeDamageSegments(void* segments, s32 numsegments, s32 isarmour, f32 value);
Gfx* buildGaugeBarDL(Gfx* gdl, void* vtxaddr, s32 numvertices);
void guOrtho(Mtx* m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void matrix_4x4_set_lookat_target(Mtxf* matrix, f32 posx, f32 posy, f32 posz, f32 lookx, f32 looky, f32 lookz,
                                  f32 upx, f32 upy, f32 upz);

#define GUN_ANIM_STATE_SWITCH_SWAP 6
#define GUN_ANIM_STATE_SWITCH_HOLD 7
#define WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY 0x80000
#define WEAPONSTATBITFLAG_NO_CLIP_RELOADS 0x400000
#define GUNRIGHT 0
#define GUNLEFT 1

#ifndef M_PI_F
#define M_PI_F 3.1415927f
#endif

// Whether the HUD goes to the window's edges: one player, in a window wider than 4:3.
static s32 hudWide(void) {
    return (getPlayerCount() == 1) && (recomp_get_aspect_ratio(4.0f / 3.0f) > (4.0f / 3.0f) + 0.001f);
}

// Starts HUD drawn at a side of the window (G_EX_ORIGIN_LEFT or G_EX_ORIGIN_RIGHT): rectangles keep their distance
// from the frame's side on the window's, inside a scissor as wide as the window.
static Gfx* hudAlignBegin(Gfx* gdl, u32 origin) {
    s32 offset = (origin == G_EX_ORIGIN_RIGHT) ? (-viGetX() * 4) : 0;

    gEXPushScissor(gdl++);
    gEXSetScissor(gdl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, viGetY());
    gEXSetRectAlign(gdl++, origin, origin, offset, 0, offset, 0);

    return gdl;
}

// Ends what hudAlignBegin started.
static Gfx* hudAlignEnd(Gfx* gdl) {
    gEXSetRectAlign(gdl++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXPopScissor(gdl++);

    return gdl;
}

// The ammo counter of the gun in the right hand, at the bottom right, and of a second gun in the left, at the bottom
// left.
RECOMP_PATCH Gfx* generate_ammo_total_microcode(Gfx* gdl) {
    s32 weapon_left;
    s32 weapon_right;
    s32 ammotype;
    s32 leftx;
    s32 rightx;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset_r;
    s32 textwidth_r;
    u32 imageoffset_l;
    s32 textwidth_l;
    s32 wide = hudWide();

    if (g_CurrentPlayer->gunammooff == 0) {
        if (g_CurrentPlayer->mpmenuon == 0) {
            weapon_left = getCurrentPlayerWeaponId(GUNLEFT);
            weapon_right = getCurrentPlayerWeaponId(GUNRIGHT);

            if (getPlayerCount() < 3) {
                leftx = 59;
                rightx = 59;
            } else if (get_cur_playernum() & 1) {
                leftx = 43;
                rightx = 127;
            } else {
                leftx = 59;
                rightx = 109;
            }

            if (weapon_right != ITEM_UNARMED) {
                ammotype = get_ammo_type_for_weapon(weapon_right);

                if (ammotype != 0 &&
                    g_CurrentPlayer->hands[GUNRIGHT].when_detonating_mines_is_0 != GUN_ANIM_STATE_SWITCH_SWAP &&
                    g_CurrentPlayer->hands[GUNRIGHT].when_detonating_mines_is_0 != GUN_ANIM_STATE_SWITCH_HOLD &&
                    !bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY)) {
                    // @recomp: at the window's right edge
                    if (wide) {
                        gdl = hudAlignBegin(gdl, G_EX_ORIGIN_RIGHT);
                    }

                    imageoffset_r = ammo_related[ammotype].IconImage;
                    textwidth_r = 5;

                    if (imageoffset_r != 0) {
                        imageoffset_r += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(
                            gdl, (u8*) imageoffset_r, (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) - (f32) rightx,
                            -1.0f, (viGetViewTop() + viGetViewHeight()) - 20, 0, ammo_related[ammotype].IconYOffset, 1);
                        textwidth_r = ((u8*) imageoffset_r)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] +
                                      g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right) {
                            reserveammo += g_CurrentPlayer->hands[GUNLEFT].weapon_ammo_in_magazine;
                        }
                    } else {
                        magammo = g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        gdl = gunDrawHudInteger(gdl, magammo,
                                                (((viGetViewLeft() + viGetViewWidth()) - rightx) - (textwidth_r / 2)) - 4,
                                                0, (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        gdl = gunDrawHudInteger(
                            gdl, reserveammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) + ((textwidth_r + 1) / 2)) + 3,
                            1, (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);

                    if (wide) {
                        gdl = hudAlignEnd(gdl);
                    }
                }
            }

            if (weapon_left != ITEM_UNARMED) {
                ammotype = get_ammo_type_for_weapon(weapon_left);

                if (ammotype != 0 &&
                    g_CurrentPlayer->hands[GUNLEFT].when_detonating_mines_is_0 != GUN_ANIM_STATE_SWITCH_SWAP &&
                    g_CurrentPlayer->hands[GUNLEFT].when_detonating_mines_is_0 != GUN_ANIM_STATE_SWITCH_HOLD &&
                    !bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY)) {
                    // @recomp: at the window's left edge
                    if (wide) {
                        gdl = hudAlignBegin(gdl, G_EX_ORIGIN_LEFT);
                    }

                    imageoffset_l = ammo_related[ammotype].IconImage;
                    textwidth_l = 5;

                    if (imageoffset_l != 0) {
                        imageoffset_l += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(
                            gdl, (u8*) imageoffset_l, getPlayer_c_screenleft() + (f32) leftx, -1.0f,
                            (viGetViewTop() + viGetViewHeight()) - 20, 1, ammo_related[ammotype].IconYOffset, 1);
                        textwidth_l = ((u8*) imageoffset_l)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] +
                                      g_CurrentPlayer->hands[GUNLEFT].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right) {
                            reserveammo += g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine;
                        }
                    } else {
                        magammo = g_CurrentPlayer->hands[GUNLEFT].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        gdl = gunDrawHudInteger(gdl, magammo, ((viGetViewLeft() + leftx) + (textwidth_l / 2)) + 3, 1,
                                                (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS)) {
                        gdl = gunDrawHudInteger(gdl, reserveammo, ((viGetViewLeft() + leftx) - ((textwidth_l + 1) / 2)) - 4,
                                                0, (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);

                    if (wide) {
                        gdl = hudAlignEnd(gdl);
                    }
                }
            }
        }
    }

    return gdl;
}

// The message at the bottom left ("Picked up ...").
RECOMP_PATCH Gfx* hudmsgBottomRender(Gfx* gdl) {
    s32 show;
    s32 view_left;
    s32 view_vert;
    s32 view_horiz;
    s32 view_top;
    s32 textheight;
    s32 textwidth;

    if ((g_CurrentPlayer->hudmessoff == FALSE) && (g_CurrentPlayer->bondmesscnt >= 0) &&
        (g_CurrentPlayer->mpmenuon == FALSE)) {
        show = 0;
        if (getPlayerCount() == 1) {
            if ((u8) *stringbuffer_lowerleft[status_bar_text_buffer_index] != 0) {
                show = 1;
            }
        } else if (g_CurrentPlayer->bondmesscnt >= 0) {
            status_bar_text_buffer_index = get_cur_playernum();
            show = 1;
        }

        if (show != 0) {
            gdl = microcode_constructor(gdl);
            textwidth = 0;
            textheight = 0;
            textMeasure(&textheight, &textwidth, stringbuffer_lowerleft[status_bar_text_buffer_index],
                        (void*) copy_2ndfonttable, (void*) copy_1stfonttable, 0);

            if (getPlayerCount() < 3) {
                view_left = viGetViewLeft() + 0x1E;
            } else if (get_cur_playernum() & 1) {
                view_left = viGetViewLeft() + 0xA;
            } else {
                view_left = viGetViewLeft() + 0x1E;
            }

            view_horiz = view_left + textwidth;

            if (getPlayerCount() < 3) {
                if ((get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == 0) &&
                    (is_clock_drawn_onscreen() == 0)) {
                    view_top = (viGetViewTop() + viGetViewHeight()) - 0x0C;
                } else {
                    view_top = (viGetViewTop() + viGetViewHeight()) - 0x28;
                }

                if (get_cur_playernum() == 1) {
                    view_top -= 8;
                }
            } else {
                view_top = viGetViewTop() + 0x10;
            }

            view_vert = view_top - textheight;

            // @recomp: at the window's left edge
            if (hudWide()) {
                gdl = hudAlignBegin(gdl, G_EX_ORIGIN_LEFT);
            }

            gdl = draw_blackbox_to_screen(gdl, &view_left, &view_vert, &view_horiz, &view_top);
            gdl = combiner_bayer_lod_perspective(textRenderOutlined(
                gdl, &view_left, &view_vert, stringbuffer_lowerleft[status_bar_text_buffer_index],
                (void*) copy_2ndfonttable, (void*) copy_1stfonttable, -1, 0x646464FFU, (s16) (s32) viGetX(),
                (s16) viGetY(), 0, 0));

            if (hudWide()) {
                gdl = hudAlignEnd(gdl);
            }
        }
    }

    return gdl;
}

// The message at the top left (an objective's, a cutscene's), and the position readout of the debug cheat. Their
// bands stay across the view, which RT64 widens to the window's width with it.
RECOMP_PATCH Gfx* sub_GAME_7F08AAE8(Gfx* gdl) {
    s32 textwidth;
    s32 textheight;
    s32 bottom;
    s32 x;
    s32 y;
    char room[0x10];
    char posx[0x10];
    char posy[0x10];
    char posz[0x10];
    char angle[0x10];
    s32 debug_boxbottom;
    s32 debug_angle;
    s8* roomid;
    DirectionLabels directions;
    s32 wide = hudWide();

    if (g_UpperTextDisplayFlag == 0) {
        if (upper_text_window_timer >= 0) {
            if (stringbuffer_top[upper_text_buffer_index][0] != '\0') {
                if (g_CurrentPlayer->mpmenuon == 0) {
                    gdl = microcode_constructor(gdl);
                    textwidth = 0;
                    textheight = 0;
                    textMeasure(&textheight, &textwidth, stringbuffer_top[upper_text_buffer_index],
                                ptrFontZurichBoldChars, ptrFontZurichBold, 0);

                    if (cameraBufferToggle != 0) {
                        x = viGetViewLeft() + 0x46;
                        y = viGetViewTop() + 0x10;
                        y += 0x10;
                        y = y / 11;
                        y *= 11;
                        y -= 2;
                    } else {
                        x = viGetViewLeft() + 0x1e;
                        y = viGetViewTop() + 0xd;
                    }

                    bottom = y + textheight;
                    gdl = microcode_constructor_related_to_menus(gdl, 0, y - 2, viGetX(), bottom, 0x64);

                    // @recomp: the text at the window's left edge
                    if (wide) {
                        gdl = hudAlignBegin(gdl, G_EX_ORIGIN_LEFT);
                    }

                    gdl = textRender(gdl, &x, &y, stringbuffer_top[upper_text_buffer_index], ptrFontZurichBoldChars,
                                     ptrFontZurichBold, -1, viGetX(), viGetY(), 0, 0);

                    if (wide) {
                        gdl = hudAlignEnd(gdl);
                    }

                    return combiner_bayer_lod_perspective(gdl);
                }
            }
        }
    }

    if (get_debug_testingmanpos_flag()) {
        debug_angle = (s32) ((atan2f(-g_CurrentPlayer->field_488.theta_transform.x,
                                     g_CurrentPlayer->field_488.theta_transform.z) *
                              180.0f) /
                             M_PI_F);
        directions = g_DebugCompassLabels;
        roomid = bgDebPrintROOMID(g_CurrentPlayer->field_488.current_tile_ptr->room);

        sprintf_recomp(room, "%8s", roomid);
        sprintf_recomp(posx, "x %4.0f", g_CurrentPlayer->field_488.collision_position.x);
        sprintf_recomp(posy, "y %4.0f", g_CurrentPlayer->field_488.collision_position.y);
        sprintf_recomp(posz, "z %4.0f", g_CurrentPlayer->field_488.collision_position.z);
        sprintf_recomp(angle, "%s %3d", ((char*) (&directions)) + (((debug_angle + 0x16) / 0x2d) * 3), debug_angle);

        x = viGetViewLeft() + 0x11;
        y = viGetViewTop() + 0x11;
        debug_boxbottom = y + 0xa;
        gdl = microcode_constructor(gdl);
        gdl = microcode_constructor_related_to_menus(gdl, 0, y, viGetX(), debug_boxbottom + 1, 0x64);

        // @recomp: the text at the window's left edge
        if (wide) {
            gdl = hudAlignBegin(gdl, G_EX_ORIGIN_LEFT);
        }

        gdl = textRender(gdl, &x, &y, room, ptrFontBankGothicChars, ptrFontBankGothic, -1, viGetX(), viGetY(), 0, 0);
        x = viGetViewLeft() + 0x57;
        gdl = textRender(gdl, &x, &y, posx, ptrFontBankGothicChars, ptrFontBankGothic, -1, viGetX(), viGetY(), 0, 0);
        x = viGetViewLeft() + 0x8d;
        gdl = textRender(gdl, &x, &y, posy, ptrFontBankGothicChars, ptrFontBankGothic, -1, viGetX(), viGetY(), 0, 0);
        x = viGetViewLeft() + 0xc3;
        gdl = textRender(gdl, &x, &y, posz, ptrFontBankGothicChars, ptrFontBankGothic, -1, viGetX(), viGetY(), 0, 0);
        x = viGetViewLeft() + 0xf9;
        gdl = textRender(gdl, &x, &y, angle, ptrFontBankGothicChars, ptrFontBankGothic, -1, viGetX(), viGetY(), 0, 0);

        if (wide) {
            gdl = hudAlignEnd(gdl);
        }

        gdl = combiner_bayer_lod_perspective(gdl);
    }

    return gdl;
}

// The health gauge, an arc at the view's left, and the armour gauge at its right, shown a while after Bond is hit.
// Each is drawn through the view's viewport aligned to its side of the window.
static Gfx* gaugeBar(Gfx* gdl, void* bar, u32 origin, s32 wide) {
    if (wide) {
        gEXPushViewport(gdl++);
        gEXSetViewportAlign(gdl++, origin, (origin == G_EX_ORIGIN_RIGHT) ? (-viGetX() * 4) : 0, 0);
        gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->viewports[g_ViBackIndex]));
    }

    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(bar));

    if (wide) {
        gEXSetViewportAlign(gdl++, G_EX_ORIGIN_NONE, 0, 0);
        gEXPopViewport(gdl++);
    }

    return gdl;
}

RECOMP_PATCH Gfx* bondviewRenderGaugeBars(Gfx* gdl) {
    Mtx* lookatmtx;
    Mtx* orthomtx;
    Mtxf lookatmtxf;
    s32 wide = hudWide();

    // Set up armor bars.
    hudMakeDamageSegments(&g_CurrentPlayer->armor_display_values[0], 0x2e, 1, g_CurrentPlayer->apparentarmour);
    buildGaugeBarDL((Gfx*) &g_CurrentPlayer->watch_body_armor_bar_gdl,
                    OS_PHYSICAL_TO_K0(&g_CurrentPlayer->armor_display_values[0]), 0x2e);

    // Set up health bars.
    hudMakeDamageSegments(&g_CurrentPlayer->health_display_values[0], 0x2e, -1, g_CurrentPlayer->apparenthealth);
    buildGaugeBarDL((Gfx*) &g_CurrentPlayer->watch_health_bar_gdl,
                    OS_PHYSICAL_TO_K0(&g_CurrentPlayer->health_display_values[0]), 0x2e);

    // Create an orthographic render state for the gauge.
    lookatmtx = dynAllocateMatrix();
    orthomtx = dynAllocateMatrix();

    guOrtho(orthomtx, -800.0f * D_800364CC, 800.0f * D_800364CC, -600.0f * D_800364CC, 600.0f * D_800364CC, -100.0f,
            1000.0f, 1.0f);

    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(orthomtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    matrix_4x4_set_lookat_target(&lookatmtxf, 0.0f, 500.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
    matrix_4x4_f32_to_s32(&lookatmtxf, (Mtxf*) lookatmtx);

    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(lookatmtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetPrimColor(gdl++, 0, 0, 0xe6, 0xe6, 0xe6, 0x00);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH);

    // @recomp: the armour gauge at the window's right edge, the health gauge at its left
    gdl = gaugeBar(gdl, &g_CurrentPlayer->watch_body_armor_bar_gdl, G_EX_ORIGIN_RIGHT, wide);
    gdl = gaugeBar(gdl, &g_CurrentPlayer->watch_health_bar_gdl, G_EX_ORIGIN_LEFT, wide);

    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(currentPlayerGetProjectionMatrix()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    return gdl;
}
