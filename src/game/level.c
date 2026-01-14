#include "game/level.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/console.h"
#include "game/physics.h"

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
static Entity **entities_free_addresses;
static Entity *entities_allocation;
static Phys_Edge *edges_allocation;
static Phys_Polygon *polygon_list;


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

    // Setting up entities stuff.
    entities_allocation = malloc(MAX_ENTITIES * sizeof(Entity));
    entities_free_addresses = array_list_make(Entity *, 8, &std_allocator);

    edges_allocation = NULL;
    polygon_list = array_list_make(Phys_Polygon, 8, &std_allocator);

    // All values in global state are defaulted to 0.
    // state->level.flags = 0;
}



const String LEVEL_FILE_PATH   = STR_BUFFER("res/level/");
const String LEVEL_FILE_FORMAT = STR_BUFFER(".level");

void level_load(String name) {
    console_log("Loading '%.*s' level.\n", UNPACK(name));

    state->level = (Level) {0};

    char file_name[LEVEL_FILE_PATH.length + name.length + LEVEL_FILE_FORMAT.length + 1];
    str_copy_to(LEVEL_FILE_PATH, file_name);
    str_copy_to(name, file_name + LEVEL_FILE_PATH.length);
    str_copy_to(LEVEL_FILE_FORMAT, file_name + LEVEL_FILE_PATH.length + name.length);
    file_name[LEVEL_FILE_PATH.length + name.length + LEVEL_FILE_FORMAT.length] = '\0';

    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        console_log("Couldn't open the level file for loading in game '%s'.\n", file_name);
        return;
    }
    
    (void)fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    rewind(file);

    u8 *buffer = malloc(size);
    if (buffer == NULL) {
        console_log("Memory allocation for buffer failed while reading the file '%s'.\n", file_name);
        (void)fclose(file);
        return;
    }

    if (fread(buffer, 1, size, file) != size) {
        console_log("Failure reading the file '%s'.\n", file_name);
        (void)fclose(file);
        free(buffer);
        return;
    }

    (void)fclose(file);


    u8 *ptr = buffer;

    // @Copypasta from editor.c
    if (read_u32(&ptr) != LEVEL_FORMAT_HEADER) {
        console_log("Failure reading the level file '%s' into the game, format header doesn't match.\n", file_name);
        free(buffer);
        return;
    }

    u32 edge_count = read_u32(&ptr);

    // @Temporary: Find better approach for level geometry memory management.
    if (edges_allocation != NULL) {
        console_log("Reallocating memory for level geometry.\n");
        edges_allocation = realloc(edges_allocation, edge_count * sizeof(Phys_Edge));
    } else {
        console_log("Allocating memory for level geometry.\n");
        edges_allocation = malloc(edge_count * sizeof(Phys_Edge));
    }

    array_list_clear(&polygon_list);

    // Reusing edge count variable.
    edge_count = 0;
    u32 polygon_edge_count;
    while (ptr < buffer + size) {
        polygon_edge_count = read_u32(&ptr);
        
        array_list_append(&polygon_list, ((Phys_Polygon) { .edges_count = polygon_edge_count, .edges = edges_allocation + edge_count }));
        for (u32 i = 0; i < polygon_edge_count; i++) {
            (edges_allocation + edge_count + i)->vertex.x = read_float(&ptr);
            (edges_allocation + edge_count + i)->vertex.y = read_float(&ptr);
            (edges_allocation + edge_count + i)->normal.x = read_float(&ptr);
            (edges_allocation + edge_count + i)->normal.y = read_float(&ptr);
        }

        edge_count += polygon_edge_count;
    }

    free(buffer);


    console_log("Read %llu bytes into the game from '%s' level file.\n", ptr - buffer, file_name);


    state->level.name = name;

    memset(entities_allocation, 0, sizeof(Entity) * MAX_ENTITIES);
    array_list_clear(&entities_free_addresses);
    state->level.entities_count = 0;
    state->level.entities = entities_allocation;

    state->level.phys_polygons_count = array_list_length(&polygon_list);
    state->level.phys_polygons = polygon_list;
    state->level.flags |= LEVEL_LOADED;
    

    // Player
    player = level_add_entity((Entity) { 
            .phys_box = phys_box_make(vec2f_make(-1.0f, 0.7f), 0.8f, 1.4f, 0.0f, 65.0f, 0.0f, 0.7f, 0.4f, true, false, false, true),
            .type = PLAYER, 
            .player = (Player) { .color = VEC4F_YELLOW, }, 
            });


    // Physics objects
    level_add_entity((Entity) {
            .phys_box = phys_box_make(vec2f_make(1.0f, 1.0f), 1.0f, 1.0f, 0.0f, 55.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, true, true, false, true),
            .type = PROP_PHYSICS, 
            .prop_physics = (Prop_Physics) { .color = VEC4F_CYAN, }, 
            });

    level_add_entity((Entity) {
            .phys_box = phys_box_make(vec2f_make(-1.0f, 1.0f), 0.5f, 1.4f, PI / 4.0f, 40.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, true, true, false, true),
            .type = PROP_PHYSICS, 
            .prop_physics = (Prop_Physics) { .color = VEC4F_CYAN, }, 
            });


    // Level blockout
    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(2.0f, -1.5f), 12.0f, 3.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(-8.0f, -3.0f), 8.0f, 12.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(-9.5f, 7.0f), 5.0f, 8.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(2.5f, -6.0f), 13.0f, 6.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(13.5f, 0.5f), 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(20.0f, -6.0f), 12.0f, 6.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(21.5f, -2.0f), 9.0f, 2.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(23.0f, 1.0f), 6.0f, 4.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(19.5f, 2.5f), 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, GEOMETRY_STATIC_FRICTION, GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = PROP_STATIC, 
    //         .prop_static = (Prop_Static) { .color = VEC4F_GREY, }, 
    //         });


    // // Triggers, in future make triggers send events, it is usefull for 1. in editor set up of logic, and in more organized game logic, that can be more controlled, especially if events can be delayed and so on...
    // level_add_entity((Entity) {
    //         .phys_box = (Phys_Box) {0},
    //         .type = TRIGGER, 
    //         .trigger = (Trigger) { .name = CSTR("finish"), .bound_box = obb_make(vec2f_make(24.0f, 5.0f), 2.0f, 4.0f, 0.0f)}, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = (Phys_Box) {0},
    //         .type = TRIGGER, 
    //         .trigger = (Trigger) { .name = CSTR("pit"), .bound_box = obb_make(vec2f_make(8.0f, -11.0f), 24.0f, 4.0f, 0.0f)}, 
    //         });

}



void level_update() {
    if (!(state->level.flags & LEVEL_LOADED)) {
        return;
    }

    // Player control stuff.
    if (!console_active()) {
        float x_vel = 0.0f;
        // float y_vel = 0.0f;

        if (hold(SDLK_d)) {
            x_vel += 1.0f;
        } 
        if (hold(SDLK_a)) {
            x_vel -= 1.0f;
        }
        // if (hold(SDLK_w)) {
        //     y_vel += 1.0f;
        // } 
        // if (hold(SDLK_s)) {
        //     y_vel -= 1.0f;
        // }

        x_vel *= 5.0f;
        // y_vel *= 5.0f;

        player->phys_box.body.velocity.x = x_vel;
        // player->phys_box.body.velocity.y = y_vel;

        if (pressed(SDLK_SPACE) && player->phys_box.grounded) {
            phys_apply_force(&player->phys_box.body, vec2f_make(0.0f, 425.0f));
        }
    }


    // Simulating physics.
    phys_update(&state->level.entities->phys_box, state->level.entities_count, sizeof(Entity));


    // Simple super smooth camera movement.
    state->main_camera.center = vec2f_lerp(state->main_camera.center, player->phys_box.bound_box.center, 0.8f * state->t.delta_time);


    for (s64 i = 0; i < state->level.entities_count; i++) {
        switch(state->level.entities[i].type) {
            case TRIGGER:
                if (phys_sat_check_collision_obb(&state->level.entities[i].trigger.bound_box, &player->phys_box.bound_box)) {
                    console_log("Trigger triggered.\n");
                    if (str_equals(state->level.entities[i].trigger.name, CSTR("finish"))) {
                        console_log("Level finished!\n");
                        player->phys_box.bound_box.center = vec2f_make(-1.0f, 0.7f);
                        continue;
                    }
                    if (str_equals(state->level.entities[i].trigger.name, CSTR("pit"))) {
                        console_log("Player fell and died :(\n");
                        player->phys_box.bound_box.center = vec2f_make(-1.0f, 0.7f);
                        continue;
                    }
                }
                break;
        }
    }
}

void level_draw() {
    if (!(state->level.flags & LEVEL_LOADED)) {
        return;
    }

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
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = state->level.entities[i].prop_physics.color, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                break;
            case TRIGGER:
                draw_rect(obb_p0(&state->level.entities[i].trigger.bound_box), obb_p1(&state->level.entities[i].trigger.bound_box), .color = vec4f_make(0.6f, 0.3f, 0.3f, 0.2f));
                break;
        }
    }



    draw_end();


    // Drawing lines. 
    shader_update_projection(state->line_drawer.program, &projection);

    line_draw_begin(&state->line_drawer);

    Vec2f midpoint, v0, v1;
    for (u32 i = 0; i < array_list_length(&polygon_list); i++) {
        for (u32 j = 0; j < polygon_list[i].edges_count; j++) {
            v0 = polygon_list[i].edges[j].vertex;
            v1 = polygon_list[i].edges[(j + 1) % polygon_list[i].edges_count].vertex;

            draw_line(v0, v1, VEC4F_WHITE, NULL);

            midpoint = vec2f_make(v0.x + (v1.x - v0.x) / 2, v0.y + (v1.y - v0.y) / 2);

            draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(polygon_list[i].edges[j].normal, 0.4f)), VEC4F_BLUE, NULL);

        }
    }

    line_draw_end();





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


Entity *level_add_entity(Entity entity) {
    if (state->level.entities_count >= MAX_ENTITIES) {
        return NULL;
    }

    state->level.entities_count++;

    if (array_list_length(&entities_free_addresses) > 0) {
        Entity *free_ptr = entities_free_addresses[0];
        *free_ptr = entity;
        array_list_unordered_remove(&entities_free_addresses, 0);
        return free_ptr;
    }


    state->level.entities[state->level.entities_count - 1] = entity;
    return state->level.entities + (state->level.entities_count - 1);
}

void level_remove_entity(Entity *entity) {
    if (entity >= state->level.entities + MAX_ENTITIES || entity < state->level.entities) {
        console_log("Attempted remove of entity at invalid address.\n");
        return;
    }

    if (entity->type == NONE) {
        console_log("Attempted remove entity of type NONE.\n");
        return;
    }

    *entity = ((Entity) {0});
    array_list_append(&entities_free_addresses, entity);
}
