#ifndef EDITOR_H
#define EDITOR_H

// #include "game/game.h"

typedef struct state State;

#include "core/mathf.h"
#include "core/str.h"

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
 * Writes current editor level by the name provided.
 */
@Introspect;
@RegisterCommand;
void editor_write(String name);

@Introspect;
@RegisterCommand;
void editor_read(String name);

@Introspect;
@RegisterCommand;
void editor_add_quad();


void editor_add_quad_at(Vec2f position);

#endif
