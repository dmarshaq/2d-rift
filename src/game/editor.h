#ifndef EDITOR_H
#define EDITOR_H

#include "core/mathf.h"
#include "core/str.h"

typedef struct state State;

// 0x65646974 stands for 'edit' in ascii.
#define EDITOR_FORMAT_HEADER 0x65646974

extern const String EDITOR_FILE_PATH;
extern const String EDITOR_FILE_FORMAT;


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
void editor_build(String name);

@Introspect;
@RegisterCommand;
void editor_add_quad();


void editor_add_quad_at(Vec2f position);

#endif
