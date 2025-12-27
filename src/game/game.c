#include "game/game.h"

#include "core/core.h"
#include "core/type.h"
#include "core/structs.h"
#include "core/file.h"
#include "core/mathf.h"
#include "core/typeinfo.h"
#include "core/log.h"

#include "game/graphics.h"
#include "game/level.h"
#include "game/input.h"
#include "game/draw.h"
#include "game/event.h"
#include "game/console.h"
#include "game/editor.h"
#include "game/command.h"
#include "game/vars.h"
#include "game/imui.h"
#include "game/asset.h"

#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_keyboard.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>






/**
 * Global state.
 *
 * @Important: Address of state structure doesn't change no matter what.
 * It exists through out the game execution and it's data changes accordingly to the current game state.
 */
static State *state;



// Keybinds (Simple).
// @Temporary: Will be replaced with meta program + keybinds config solution.
#define BIND_PRESSED(keybind)   (!SDL_IsTextInputActive() && pressed(keybind))
#define BIND_HOLD(keybind)      (!SDL_IsTextInputActive() && hold(keybind))
#define BIND_UNPRESSED(keybind) (!SDL_IsTextInputActive() && unpressed(keybind))










// File formats.

static const String SHADER_FILE_FORMAT  = STR_BUFFER("glsl");
static const String FONT_FILE_FORMAT    = STR_BUFFER("ttf");



void game_init(State *global_state) {
    /**
     * The pointer to global state is copied only once and stored internally in this file.
     * @Important: It assumes, that this address will remain the same through ou the game and will not change no matter what.
     */
    state = global_state;


    // Setting random seed.
    srand((u32)time(NULL));


    // Init vars.
    vars_tree_begin();


    // Init assets observer.
    if (asset_observer_init("res") != 0) {
        LOG_ERROR("Couldn't init Asset Observer.");
        exit(1);
    }
    
    // Force load asset changes.
    if (asset_force_changes("res") != 0) {
        LOG_ERROR("Couldn't force load asset changes.");
        exit(1);
    }
    
    // Getting asset changes.
    u32 changes_count;
    const Asset_Change *changes;

    if (!asset_view_changes(&changes_count, &changes)) {
        LOG_ERROR("Couldn't view loaded asset changes.");
        exit(1);
    }



    // Init SDL and GL.
    if (init_sdl_gl()) {
        LOG_ERROR("Couldn't init SDL and GL.");
        exit(1);
    }
    
    // Init audio.
    if (init_sdl_audio()) {
        LOG_ERROR("Couldn't init audio.");
        exit(1);
    }


    // Create window.
    state->window = create_gl_window("Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 700);

    if (state->window.ptr == NULL) {
        LOG_ERROR("Couldn't create window.");
        exit(1);
    }




    // Initing graphics.
    graphics_init();



    // Keyboard input init.
    keyboard_state_init();



    // Initing events.
    event_init_handler(&state->events);




    /**
     * Setting hash tables for resources. 
     * Since they are dynamically allocated pointers will not change after hot reloading.
     */
    state->shader_table = hash_table_make(Shader, 8, &std_allocator);


    /**
     * This just goes through asset changes that are forced by 'asset_force_changes(...).
     * And loads each one using specific loading function.
     * The idea is to have each module that needs some resources to be loaded provide it's own loading function for one file at a time.
     * And such functions then can simply be called here.
     */

    // Loading Shaders specifically.
    for (u32 i = 0; i < changes_count; i++) {
        if (str_equals(changes[i].file_format, SHADER_FILE_FORMAT)) {
            LOG_INFO("Detected Shader Asset: '%.*s'.", UNPACK(changes[i].full_path));
            char _buffer[changes[i].full_path.length + 1]; 
            str_copy_to(changes[i].full_path, _buffer);
            _buffer[changes[i].full_path.length]     = '\0';

            Shader shader = shader_load(_buffer);
            if (shader.id == 0) {
                // If id is 0 this means shader failed to load, skipping it.
                continue;
            }

            shader_init_uniforms(&shader);

            String shader_name = str_substring(changes[i].file_name, 0, str_find_char_right(changes[i].file_name, '.'));
            hash_table_put(&state->shader_table, shader, UNPACK(shader_name));

            asset_remove_change(i);
            i--;
            changes_count--;

            continue;
        }
    }



    // Drawers init.
    drawer_init(&state->quad_drawer, hash_table_get(&state->shader_table, UNPACK_LITERAL("quad")));

    drawer_init(&state->grid_drawer, hash_table_get(&state->shader_table, UNPACK_LITERAL("grid")));

    drawer_init(&state->ui_quad_drawer, hash_table_get(&state->shader_table, UNPACK_LITERAL("ui_quad")));


    line_drawer_init(&state->line_drawer, hash_table_get(&state->shader_table, UNPACK_LITERAL("line")));


    // Main camera init.
    state->main_camera = camera_make(VEC2F_ORIGIN, 48);


    // Init commands.
    command_init();

    // Init console.
    console_init(state);

    // Init immediate ui.
    ui_init(&state->ui_state, &state->events.mouse_input);

    // Init physics.
    phys_init(state);

    // Init level manager.
    level_manager_init(state);

    // Init editor.
    editor_init(state);

    





    


    // Finishing vars tree.
    state->vars_tree = vars_tree_build();

    // Loading Vars files specifically. After the vars tree is built...
    for (u32 i = 0; i < changes_count; i++) {
        if (str_equals(changes[i].file_format, VARS_FILE_FORMAT)) {
            LOG_INFO("Detected Vars Asset: '%.*s'.", UNPACK(changes[i].full_path));

            vars_load_file(changes[i].full_path, &state->vars_tree);

            asset_remove_change(i);
            i--;
            changes_count--;

            continue;
        }
    }







    // Setting game state.
    state->game_state = GAME_STATE_LEVEL;


    // Setting clear color.
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Logging hello world to the console.
    console_log("Hello world!\n");

    String starting_level_name = CSTR("demo_01");
    console_log("Loading: '%.*s' level.\n", UNPACK(starting_level_name));
    
    level_load(starting_level_name);
}





void process_asset_changes() {
    u32 count;
    const Asset_Change *changes;

    if (asset_view_changes(&count, &changes)) {
        for (u32 i = 0; i < count; i++) {
            // Vars files.
            if (str_equals(changes[i].file_format, VARS_FILE_FORMAT)) {
                console_log("Vars detected Asset Change: '%.*s'\n", UNPACK(changes[i].full_path));
                vars_load_file(changes[i].full_path, &state->vars_tree);
            } 
            // Shader files.
            else if (str_equals(changes[i].file_format, SHADER_FILE_FORMAT)) {
                console_log("Shader detected Asset Change: '%.*s'\n", UNPACK(changes[i].full_path));
                char _buffer[changes[i].full_path.length + 1]; 
                str_copy_to(changes[i].full_path, _buffer);
                _buffer[changes[i].full_path.length]     = '\0';

    
                Shader shader = shader_load(_buffer);
                if (shader.id == 0) {
                    // If id is 0 this means shader failed to load, skipping it.
                    continue;
                }


                String shader_name = str_substring(changes[i].file_name, 0, str_find_char_right(changes[i].file_name, '.'));

                Shader *existing = hash_table_get(&state->shader_table, UNPACK(shader_name));
                if (existing != NULL) {
                    shader_unload(existing);
                }



                shader_init_uniforms(&shader);

                hash_table_put(&state->shader_table, shader, UNPACK(shader_name));
            }
        }
    }
}

/**
 * @Important: In game update loops the order of procedures is: Updating -> Drawing.
 * Where in updating all logic of the game loop is contained including inputs, sound and so on.
 * Drawing part is only responsible for putting pixels accrodingly with calculated data in "Updating" part.
 */
void game_update() {
    // Polling any asset changes.
    if (asset_observer_poll_changes() != 0) {
        LOG_ERROR("Couldn't poll asset changes.");
        exit(1);
    }

    process_asset_changes();

    
    // Handling events
    event_handle(&state->events, &state->window, &state->t);

    // Clearin screen.
    glClear(GL_COLOR_BUFFER_BIT);
    



    // Matrix4f projection = camera_calculate_projection(&state->main_camera, state->window.width, state->window.height);

    // shader_update_projection(state->quad_drawer.program, &projection);

    // draw_begin(&state->quad_drawer);

    // draw_quad(vec2f_make(-1.0f, -1.0f), vec2f_make(1.0f, 1.0f));

    // draw_end();



    switch (state->game_state) {
        case GAME_STATE_MENU:

            break;
        case GAME_STATE_EDITOR:

            // Updating editor, if console is not active.
            if (!console_active()) {
                if (editor_update()) {
                    TODO("Editor exitting.");
                    // Might be moved to editor itself since it is generally not part of the game?
                    // Editor is exitted.
                    // Rebuild the level.
                    // Load level.
                    // Switch to GAME_STATE_LEVEL.
                }
            }

            // Editor drawing.
            editor_draw();

            break;
        case GAME_STATE_LEVEL:

            
            level_update();

            level_draw();

            // Matrix4f projection;

            // projection = camera_calculate_projection(&state->main_camera, state->window.width, state->window.height);

            // shader_update_projection(state->quad_drawer.program, &projection);

            // draw_begin(&state->quad_drawer);

            // for (s64 i = 0; i < entities_count(); i++) {
            //     draw_rect(obb_p0(&entities[i].phys_box.bound_box), obb_p1(&entities[i].phys_box.bound_box), .offset_angle = entities[i].phys_box.bound_box.rot);
            // }

            // draw_end();



            break;
    }
    




    // Console update.
    console_update();

    // Console drawing.
    console_draw();
   


    // Checking for gl error.
    check_gl_error();


    // Swap buffers to display the rendered image.
    SDL_GL_SwapWindow(state->window.ptr);


    // Post updating input.
    keyboard_state_old_update();
}

void game_free() {
    console_free();

    shader_unload(hash_table_get(&state->shader_table, UNPACK_LITERAL("quad")));
    shader_unload(hash_table_get(&state->shader_table, UNPACK_LITERAL("ui_quad")));
    drawer_free(&state->quad_drawer);
}

void quit() {
    state->events.should_quit = true;
}

void game_set_state(Game_State game_state) {
    state->game_state = game_state;
}














