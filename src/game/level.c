#include "game/level.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/console.h"

#include "core/mathf.h"
#include "core/structs.h"
#include "core/arena.h"
#include "core/str.h"
#include "core/file.h"

#define MAX_ENTITIES 16
#define LEVEL_ARENA_SIZE 1024

static Font_Baked font_small;
static Font_Baked font_medium;
static Arena arena;
static String info_buffer;
static Entity *entities_allocation;


// Player controller related.
static Entity *player;




static State *state;





void level_manager_init(State *s) {
    state = s;
    //
    // @Copypasta: From editor.c, from console.c ...
    // Get resources.

    // Load needed font... Hard coded...
    u8* font_data = read_file_into_buffer("res/font/Consolas-Regular.ttf", NULL, &std_allocator);

    font_small  = font_bake(font_data, 14.0f);
    font_medium = font_bake(font_data, 20.0f);

    free(font_data);
    


    // Make arena and allocate space for the buffers.
    arena = arena_make(LEVEL_ARENA_SIZE);

    info_buffer.data   = arena_alloc(&arena, 256);
    info_buffer.length = 256;

    entities_allocation = calloc(MAX_ENTITIES, sizeof(Entity));
}

void level_load(String name) {
    state->level = (Level) {
        .name = name,
        .entities = entities_allocation,
        .entities_count = 0,
    };

    player = state->level.entities + level_add_entity((Entity) { 
            .phys_box = phys_box_make(VEC2F_ORIGIN, 0.4f, 0.7f, 0.0f, 65.0f, 0.0f, 0.7f, 0.4f, true, false, false, true),
            .type = PLAYER, 
            .player = (Player) { .color = VEC4F_YELLOW, }, 
            });
    
    level_add_entity((Entity) {
            .phys_box = phys_box_make(vec2f_make(0.0f, -1.5f), 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.4f, false, false, false, false),
            .type = PROP_STATIC, 
            .prop_static = (Prop_Static) { .color = VEC4F_CYAN, }, 
            });
}



void level_update() {
    // Player control stuff.
    if (!console_active()) {
        float x_vel = 0.0f;

        if (hold(SDLK_d)) {
            x_vel += 1.0f;
        } 
        if (hold(SDLK_a)) {
            x_vel -= 1.0f;
        }

        x_vel *= 8.0f;

        player->phys_box.body.velocity.x = x_vel;

        if (pressed(SDLK_SPACE) && player->phys_box.grounded) {
            phys_apply_force(&player->phys_box.body, vec2f_make(0.0f, 400.0f));
        }
    }




    // Simulating physics.
    phys_update(&state->level.entities->phys_box, state->level.entities_count, sizeof(Entity));
}

void level_draw() {
    Matrix4f projection;

    projection = camera_calculate_projection(&state->main_camera, state->window.width, state->window.height);
    
    // Drawing entities.
    shader_update_projection(state->quad_drawer.program, &projection);

    draw_begin(&state->quad_drawer);


    for (s64 i = 0; i < state->level.entities_count; i++) {
        switch(state->level.entities[i].type) {
            case PLAYER:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = state->level.entities[i].player.color);
                break;
            case PROP_STATIC:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = state->level.entities[i].prop_static.color);
                break;
            case PROP_PHYSICS:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = state->level.entities[i].prop_physics.color);
                break;
        }
    }



    draw_end();






    projection = screen_calculate_projection(state->window.width, state->window.height);
    shader_update_projection(state->ui_quad_drawer.program, &projection);

    draw_begin(&state->ui_quad_drawer);

    // Draw editor ui.
    
    // Set ui to use specific font.
    ui_set_font(&font_small);

    UI_WINDOW(0, 0, state->window.width, state->window.height, 
            
            ui_text(
                str_format(info_buffer, 
                    "Window size: %dx%d\n"
                    "Level name: %.*s\n"
                    "Entities count: %u\n"
                    "Camera unit scale: %d\n"
                    , state->window.width, state->window.height, UNPACK(state->level.name), 0, state->main_camera.unit_scale)
            );
    );

    draw_end();
}


s64 level_add_entity(Entity entity) {
    if (state->level.entities_count >= MAX_ENTITIES) {
        return -1;
    }

    state->level.entities[state->level.entities_count] = entity;
    return state->level.entities_count++;
}

Entity level_remove_entity(s64 index) {
    if (index >= state->level.entities_count) {
        return (Entity) {0};
    }

    Entity removed = state->level.entities[index];

    state->level.entities[index] = state->level.entities[--state->level.entities_count];
    
    return removed;
}
