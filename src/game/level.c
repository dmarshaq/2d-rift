#include "game/level.h"

#include "meta_generated.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/console.h"
#include "game/physics.h"
#include "game/vars.h"

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



@Introspect;
typedef struct level_params {
    float camera_zoom;
} Level_Params;

static Level_Params level_params;



void level_manager_init(State *s) {
    // Tweak vars default values.
    level_params.camera_zoom = 1.0f;
    
    vars_tree_add(TYPE_OF(level_params), (u8 *)&level_params, CSTR("level_params"));

    
    // Intializing other stuff.

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
    player = NULL; // Find a better way to reference player?

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

    u32 edge_counter = 0;
    u32 polygon_edge_count;
    while (edge_counter < edge_count) {
        polygon_edge_count = read_u32(&ptr);
        
        array_list_append(&polygon_list, ((Phys_Polygon) { .edges_count = polygon_edge_count, .edges = edges_allocation + edge_counter }));
        for (u32 i = 0; i < polygon_edge_count; i++) {
            (edges_allocation + edge_counter + i)->vertex.x = read_float(&ptr);
            (edges_allocation + edge_counter + i)->vertex.y = read_float(&ptr);
            (edges_allocation + edge_counter + i)->normal.x = read_float(&ptr);
            (edges_allocation + edge_counter + i)->normal.y = read_float(&ptr);
        }

        edge_counter += polygon_edge_count;
    }





    u32 entity_count = read_u32(&ptr);

    OBB obb;
    Entity e;

    memset(entities_allocation, 0, sizeof(Entity) * MAX_ENTITIES);
    array_list_clear(&entities_free_addresses);
    state->level.entities_count = 0;
    state->level.entities = entities_allocation;

    for (u32 i = 0; i < entity_count; i++) {

        e.type = read_byte(&ptr);

        obb.center.x = read_float(&ptr);
        obb.center.y = read_float(&ptr);
        obb.dimensions.x = read_float(&ptr);
        obb.dimensions.y = read_float(&ptr);
        obb.rot = read_float(&ptr);

        switch(e.type) {
            case PLAYER:
                if (player != NULL) {
                    break;
                }
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, 0.0f, 65.0f, 0.0f, 0.7f, 0.4f, true, false, false, true);
                
                player = level_add_entity(e);
                break;
            case PROP_PHYSICS:
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, obb.rot, 55.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, true, true, false, true);

                level_add_entity(e);
                break;
            case RAY_EMITTER:
                e.ray_emitter.ray_points_list = array_list_make(Vec2f, 4, &std_allocator);
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, obb.rot, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false);

                level_add_entity(e);
                break;
            case RAY_HARVESTER:
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, obb.rot, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false);

                level_add_entity(e);
                break;
            case MIRROR:
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, obb.rot, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false);

                level_add_entity(e);
                break;
            case GLASS:
                e.phys_box = phys_box_make(obb.center, obb.dimensions.x, obb.dimensions.y, obb.rot, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false);

                level_add_entity(e);
                break;
        }

    }

    free(buffer);


    console_log("Read %llu bytes into the game from '%s' level file.\n", ptr - buffer, file_name);


    state->level.name = name;


    state->level.phys_polygons_count = array_list_length(&polygon_list);
    state->level.phys_polygons = polygon_list;
    state->level.flags |= LEVEL_LOADED;

    

    // Player
    // player = level_add_entity((Entity) { 
    //         .phys_box = phys_box_make(vec2f_make(-1.0f, 0.7f), 0.8f, 1.4f, 0.0f, 65.0f, 0.0f, 0.7f, 0.4f, true, false, false, true),
    //         .type = PLAYER, 
    //         .player = {}, 
    //         });

    // // Laser emitter.
    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(-4.0f, -4.5f), 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = RAY_EMITTER, 
    //         .ray_emitter = { .ray_points_list = array_list_make(Vec2f, 4, &std_allocator) }, 
    //         });
    // 
    // // Laser harvester.
    // 

    // // Mirror.
    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(4.0f, 4.5f), 0.4f, 3.0f, PI / 4, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = MIRROR, 
    //         .mirror = {}, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(4.0f, -4.5f), 0.4f, 3.0f, -PI / 4, 0.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, false, false, false, false),
    //         .type = MIRROR, 
    //         .mirror = {}, 
    //         });


    // // Physics objects
    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(1.0f, 1.0f), 1.0f, 1.0f, 0.0f, 55.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, true, true, false, true),
    //         .type = PROP_PHYSICS, 
    //         .prop_physics = {}, 
    //         });

    // level_add_entity((Entity) {
    //         .phys_box = phys_box_make(vec2f_make(-1.0f, 1.0f), 0.5f, 1.4f, PI / 4.0f, 40.0f, 0.0f, LEVEL_GEOMETRY_STATIC_FRICTION, LEVEL_GEOMETRY_DYNAMIC_FRICTION, true, true, false, true),
    //         .type = PROP_PHYSICS, 
    //         .prop_physics = (Prop_Physics) { .color = VEC4F_CYAN, }, 
    //         });



}



void level_update() {
    if (!(state->level.flags & LEVEL_LOADED)) {
        return;
    }

    // Camera setting zoom.
    state->main_camera.unit_scale = level_params.camera_zoom;

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
    state->main_camera.center = vec2f_lerp(state->main_camera.center, player->phys_box.bound_box.center, 0.9f * state->t.delta_time);

    static float rotation = 0.0f;

    rotation -= PI / 12 * state->t.delta_time;
    if (rotation < -PI)
        rotation = 0.0f;

    bool harvester_already_hit = false;
    for (s64 i = 0; i < state->level.entities_count; i++) {
        switch(state->level.entities[i].type) {
            case RAY_HARVESTER:
                state->level.entities[i].ray_harvester.ray_hit = harvester_already_hit;
                break;
            case RAY_EMITTER:
                // state->level.entities[i].phys_box.bound_box.rot = - PI / 12;

                Ray_Emitter *e = &state->level.entities[i].ray_emitter;
                array_list_clear(&e->ray_points_list);

                Vec2f v0 = obb_p1(&state->level.entities[i].phys_box.bound_box);
                Vec2f v1 = obb_p2(&state->level.entities[i].phys_box.bound_box);

                array_list_append(&e->ray_points_list, vec2f_make(v0.x + (v1.x - v0.x) / 2, v0.y + (v1.y - v0.y) / 2));
                
                Vec2f direction = obb_right(&state->level.entities[i].phys_box.bound_box);


                Vec2f hit, normal;
                float distance;
                Entity *hit_entity;
                
                // Ray logic.
                while (true) {
                    distance = FLT_MAX;
                    hit_entity = NULL;

                    for (s64 j = 0; j < state->level.entities_count; j++) {
                        switch(state->level.entities[j].type) {
                            case PROP_PHYSICS:
                                if (phys_ray_cast_obb(e->ray_points_list[array_list_length(&e->ray_points_list) - 1], direction, &state->level.entities[j].phys_box.bound_box, &hit, &distance, &normal)) {
                                    hit_entity = state->level.entities + j;
                                }
                                break;
                            case MIRROR:
                                if (phys_ray_cast_obb(e->ray_points_list[array_list_length(&e->ray_points_list) - 1], direction, &state->level.entities[j].phys_box.bound_box, &hit, &distance, &normal)) {
                                    hit_entity = state->level.entities + j;
                                }
                                break;
                            case RAY_HARVESTER:
                                if (phys_ray_cast_obb(e->ray_points_list[array_list_length(&e->ray_points_list) - 1], direction, &state->level.entities[j].phys_box.bound_box, &hit, &distance, &normal)) {
                                    hit_entity = state->level.entities + j;
                                }
                                break;
                            default:
                                break;
                        }
                    }



                    if (hit_entity->type == RAY_HARVESTER) {
                        Vec2f face_dir = obb_right(&hit_entity->phys_box.bound_box);
                        if (fequal(normal.x, face_dir.x) && fequal(normal.y, face_dir.y)) {
                            hit_entity->ray_harvester.ray_hit = true;
                            harvester_already_hit = true;
                        }
                        break;
                    }

                    if (hit_entity == NULL) {
                        hit = vec2f_sum(e->ray_points_list[array_list_length(&e->ray_points_list) - 1], vec2f_multi_constant(direction, LEVEL_RAY_EMITTER_CUT_OFF_DISTANCE));
                        break;
                    }

                    if (hit_entity->type == MIRROR) {
                        array_list_append(&e->ray_points_list, hit);
                        direction = vec2f_negate(direction);
                        Vec2f reflection_offset = vec2f_multi_constant(vec2f_difference(vec2f_multi_constant(normal, vec2f_dot(normal, direction)), direction), 2);
                        direction = vec2f_sum(direction, reflection_offset);

                        // Reset since now we cast again but from different point.
                        continue;
                    }
                    break;
                }



                array_list_append(&e->ray_points_list, hit);

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
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = LEVEL_COLOR_PLAYER);
                break;
            case PROP_PHYSICS:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = LEVEL_COLOR_PROP_PHYSICS, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                break;
            case RAY_EMITTER:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = LEVEL_COLOR_RAY_EMITTER, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                break;
            case RAY_HARVESTER:
                if (state->level.entities[i].ray_harvester.ray_hit) {
                    draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = VEC4F_GREEN, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                } else {
                    draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = VEC4F_RED, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                }
                break;
            case MIRROR:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = LEVEL_COLOR_MIRROR, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
                break;
            case GLASS:
                draw_rect(obb_p0(&state->level.entities[i].phys_box.bound_box), obb_p1(&state->level.entities[i].phys_box.bound_box), .color = LEVEL_COLOR_GLASS, .offset_angle = state->level.entities[i].phys_box.bound_box.rot);
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

    for (s64 i = 0; i < state->level.entities_count; i++) {
        switch(state->level.entities[i].type) {
            case RAY_EMITTER:
                Ray_Emitter *e = &state->level.entities[i].ray_emitter;
                for (u32 i = 0; i < array_list_length(&e->ray_points_list) - 1; i++) {
                    draw_line(e->ray_points_list[i], e->ray_points_list[i + 1], VEC4F_RED, NULL);
                }
                break;
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
