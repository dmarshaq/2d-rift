#ifndef CONSOLE_H
#define CONSOLE_H

// #include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"

#include "core/structs.h"
#include "core/str.h"
#include "core/mathf.h"

#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>

typedef struct state State;


/**
 * Should be called one time, inits console.
 */
void console_init(State *state);

/**
 * Updates console, should be called every frame after console was initted.
 */
void console_update();

/**
 * Draws console to the screen, should be called at the end of every frame.
 */
void console_draw();

/**
 * Frees memory allocated for the console.
 */
void console_free();

/**
 * Prints message to the console.
 */
void console_log(char *format, ...);

/**
 * Prints error message to the console.
 */
void console_error(char *format, ...);

/**
 * Returns true if console is active (open).
 */
bool console_active();



/**
 * Adds two integers, returns the result.
 */
@Introspect;
@RegisterCommand;
s64 add(s32 a, s32 b);

/**
 * Clears console's history.
 */
@Introspect;
@RegisterCommand;
void clear();


#endif
