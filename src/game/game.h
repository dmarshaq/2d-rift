#ifndef GAME_H
#define GAME_H

#include "core/core.h"
#include "core/mathf.h"

#include "game/graphics.h"
#include "game/input.h"
#include "game/event.h"
#include "game/imui.h"
#include "game/vars.h"


@Introspect;
typedef enum game_state : u8 {
    GAME_STATE_MENU,
    GAME_STATE_EDITOR,
    GAME_STATE_LEVEL,
} Game_State;



typedef struct state {
    Window_Info window;
    Events_Info events;
    Time_Info t;
    Game_State game_state;
    UI_State ui_state;

    Vars_Tree vars_tree;

    Camera main_camera;
    
    Shader     *shader_table;
    Font_Baked *font;

    Quad_Drawer quad_drawer;
    Quad_Drawer grid_drawer;
    Quad_Drawer ui_quad_drawer;
    Line_Drawer line_drawer;
} State;


/**
 * Inits all neccessary game structures and sets game ready to be updated.
 */
void game_init(State *state);

/**
 * Updates game, should be called one time a frame.
 * @Important: There is no divide between drawing and updating here, cause this function literally represents a whole frame in the game, no more, no less.
 */
void game_update();

/**
 * Simply frees all initted game memory.
 */
void game_free();


/**
 * This function will notify app to quit at the end of the next frame.
 */
@Introspect;
@RegisterCommand;
void quit();

/**
 * This function will change game state to the state specified.
 */
@Introspect;
@RegisterCommand;
void game_set_state(Game_State game_state);

#endif
