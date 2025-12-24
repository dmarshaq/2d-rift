#ifndef EDITOR_H
#define EDITOR_H

// #include "game/game.h"

typedef struct state State;

#include "core/mathf.h"

/**
 * Should be called one time, inits editor.
 */
void editor_init(State *state);

/**
 * Following function updates editor logic, should be called every frame when editor is active.
 * Returns true if editor is exitted.
 */
bool editor_update();

/**
 * Draws editor to the screen.
 */
void editor_draw();

/**
 * Outputs all verticies into supplied verticies array.
 * Outputs count of verticies into supplied verticies_count varaible.
 */
void editor_get_verticies(Vec2f **verticies, s64 *verticies_count);

@Introspect;
@RegisterCommand;
void editor_add_quad();

#endif
