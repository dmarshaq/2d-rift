#include "core/core.h"
#include "core/type.h"
#include "core/structs.h"
#include "core/log.h"

#include "game/game.h"
#include "game/graphics.h"
#include "game/input.h"



static State *state;

int main(int argc, char **argv) {
    log_set_minimum_level(LOG_LEVEL_INFO);
    log_set_output(stderr);



    // diagnostic_set_allowed_flags(ALLOC | RESIZE | ADD | SUBTRACT | CLEAR | FREE);

    // s32 *arr = array_list_make(s32, 2, &std_allocator);
    // array_list_append(&arr, 7);
    // array_list_append(&arr, -31);
    // array_list_append(&arr, 11);
    // array_list_append(&arr, 4);
    // array_list_append(&arr, -8);
    // array_list_free(&arr);


    // Allocate global state.
    state = allocator_alloc(&std_allocator, sizeof(State));
    
    state->t.delta_time_multi = 1.0f;
    state->t.time_slow_factor = 1;
    state->t.last_update_time = 0; 
    state->t.update_step_time = 10; // Milliseconds per one update time


    // Initting game.
    game_init(state);


    // Entering frame loop.
    while (!state->events.should_quit) {
        // Time management.
        state->t.current_time = SDL_GetTicks();
        state->t.accumilated_time += state->t.current_time - state->t.last_update_time;
        state->t.last_update_time = state->t.current_time;
        
        if (state->t.accumilated_time < state->t.update_step_time) {
            continue;
        }
        
        state->t.delta_time_milliseconds = (u32)((float)state->t.accumilated_time * state->t.delta_time_multi);
        state->t.delta_time = (float)(state->t.delta_time_milliseconds) / 1000.0f;
        state->t.accumilated_time = state->t.accumilated_time % state->t.update_step_time;

        // Updating game.
        game_update();
    }

    // Free's all allocated memory before exitting.
    // @Important: It is not super neccessary since program exits immediately after this anyway.
    // But the idea here is to understand and track any allocations to avoid unneccessary memory leaks.
    game_free();



    return 0;
}
