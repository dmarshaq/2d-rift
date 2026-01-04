#include "game/editor.h"

#include "meta_generated.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/console.h"

#include "core/mathf.h"
#include "core/structs.h"
#include "core/arena.h"
#include "core/str.h"
#include "core/file.h"





// @Deprecated.
typedef enum editor_quad_flags : u8 {
    EDITOR_QUAD_FLAG_P0_SELECTED = 0x01, // 00000001
    EDITOR_QUAD_FLAG_P2_SELECTED = 0x02, // 00000010
    EDITOR_QUAD_FLAG_P3_SELECTED = 0x04, // 00000100
    EDITOR_QUAD_FLAG_P1_SELECTED = 0x08, // 00001000
} Editor_Quad_Flags;

// @Deprecated.
static const u8 EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED = EDITOR_QUAD_FLAG_P0_SELECTED | EDITOR_QUAD_FLAG_P1_SELECTED | EDITOR_QUAD_FLAG_P2_SELECTED | EDITOR_QUAD_FLAG_P3_SELECTED;


// @Deprecated.
typedef struct editor_quad {
    Editor_Quad_Flags flags;
    Quad quad;
    Vec4f color;
} Editor_Quad;


typedef enum editor_edge_flags : u8 {
    EDITOR_EDGE_SELECTED        = 0x01,
    EDITOR_EDGE_VERTEX_SELECTED = 0x02,
} Editor_Edge_Flags;

typedef struct editor_edge {
    Vec2f vertex;
    u32 previous_index;
    u32 next_index;
    bool flipped_normal;
    Editor_Edge_Flags flags;
} Editor_Edge;





@Introspect;
typedef struct editor_params {
    float selection_radius;

    float camera_speed;
    float camera_move_lerp_t;
    float camera_zoom_min;
    float camera_zoom_max;
    float camera_zoom_speed;
    float camera_zoom_lerp_t;

    s64 ui_mouse_menu_width;
    s64 ui_mouse_menu_element_height;
    s64 ui_mouse_menu_element_count;
} Editor_Params;

static Editor_Params editor_params;




// @Deprecated.
static Editor_Quad *quads_list;

static Editor_Edge *edges_list;
static const u32 EDITOR_INVALID_INDEX = 0xffffffff;




static Vec2f world_mouse_position;
static Vec2f world_mouse_position_change;
static Vec2f world_mouse_snapped_position;
static Vec2f world_mouse_snapped_left_click_origin;
static Vec2f world_mouse_left_click_origin;
static Vec2f world_mouse_right_click_origin;
static Vec2f selection_move_offset;


Vec2f editor_mouse_snap_to_grid(Vec2f mouse_position) {
    int x = (int)mouse_position.x;
    int y = (int)mouse_position.y;

    if (mouse_position.x >= 0.0f) {
        if (mouse_position.x > x + 0.5f) {
            mouse_position.x += 1.0f;
        }
    }
    else {
        if (mouse_position.x < x - 0.5f) {
            mouse_position.x -= 1.0f;
        }
    }

    if (mouse_position.y >= 0.0f) {
        if (mouse_position.y > y + 0.5f) {
            mouse_position.y += 1.0f;
        }
    }
    else {
        if (mouse_position.y < y - 0.5f) {
            mouse_position.y -= 1.0f;
        }
    }

    return vec2f_make((int)mouse_position.x, (int)mouse_position.y);
}



typedef enum editor_selected_type : u8 {
    EDITOR_NONE,
    EDITOR_QUAD, // @Deprecated.
    EDITOR_EDGE,
    EDITOR_VERTEX,
} Editor_Selected_Type;



typedef struct editor_selected {
    Editor_Selected_Type type;

    union {
        Editor_Quad *quad; // @Deprecated.
        u32 edge_index;
    };
} Editor_Selected;


static Editor_Selected *editor_selected_list;

static Vec2f editor_camera_current_vel;
static float editor_camera_current_zoom;
static float editor_camera_current_zoom_vel;


static bool editor_ui_mouse_menu_toggle;
static Vec2f editor_ui_mouse_menu_origin;



static Font_Baked font_small;
static Font_Baked font_medium;

static Camera editor_camera;




#define EDITOR_ARENA_SIZE 1024

static Arena arena;

static String info_buffer;
static Editor_Selected selection_arena;









// Pointers to global state.
static Quad_Drawer *quad_drawer_ptr;
static Quad_Drawer *grid_drawer_ptr;
static Quad_Drawer *ui_quad_drawer_ptr;
static Line_Drawer *line_drawer_ptr;
static Window_Info *window_ptr;
static Mouse_Input *mouse_input_ptr;
static Time_Info   *time_ptr;
static Shader      **shader_table_ptr;


void editor_init(State *state) {
    // Tweak vars default values.
    editor_params.selection_radius              = 0.1f;
    editor_params.camera_speed                  = 1.0f;
    editor_params.camera_move_lerp_t            = 0.8f;
    editor_params.camera_zoom_min               = 1.0f;
    editor_params.camera_zoom_max               = 1.0f;
    editor_params.camera_zoom_speed             = 1.0f;
    editor_params.camera_zoom_lerp_t            = 0.8f;
    editor_params.ui_mouse_menu_width           = 160;
    editor_params.ui_mouse_menu_element_height  = 20;
    editor_params.ui_mouse_menu_element_count   = 1;
    
    vars_tree_add(TYPE_OF(editor_params), (u8 *)&editor_params, CSTR("editor_params"));


    // Setting non tweak vars.
    editor_camera_current_vel = VEC2F_ORIGIN;
    editor_camera_current_zoom = 0.0f;
    editor_camera_current_zoom_vel = 0.0f;
    world_mouse_position = VEC2F_ORIGIN;
    world_mouse_position_change = VEC2F_ORIGIN;
    world_mouse_snapped_position = VEC2F_ORIGIN;
    world_mouse_left_click_origin = VEC2F_ORIGIN;
    world_mouse_right_click_origin = VEC2F_ORIGIN;
    world_mouse_snapped_left_click_origin = VEC2F_ORIGIN;
    selection_move_offset = VEC2F_ORIGIN;
    editor_ui_mouse_menu_toggle = false;
    editor_ui_mouse_menu_origin = VEC2F_ORIGIN;

    // Setting pointers to global state.
    quad_drawer_ptr    = &state->quad_drawer;
    grid_drawer_ptr    = &state->grid_drawer;
    ui_quad_drawer_ptr = &state->ui_quad_drawer;
    line_drawer_ptr    = &state->line_drawer;
    window_ptr         = &state->window;
    mouse_input_ptr    = &state->events.mouse_input;
    time_ptr           = &state->t;
    shader_table_ptr   = &state->shader_table;



    quads_list = array_list_make(Editor_Quad, 8, &std_allocator);
    edges_list = array_list_make(Editor_Edge, 8, &std_allocator);
    editor_selected_list = array_list_make(Editor_Selected, 8, &std_allocator);

    
    // @Copypasta: From console.c ... 
    // Get resources.

    // Load needed font... Hard coded...
    u8* font_data = read_file_into_buffer("res/font/Consolas-Regular.ttf", NULL, &std_allocator);

    font_small  = font_bake(font_data, 14.0f);
    font_medium = font_bake(font_data, 20.0f);

    free(font_data);
    

    // Copying main camera for editor.
    editor_camera = state->main_camera;


    // Make arena and allocate space for the buffers.
    arena = arena_make(EDITOR_ARENA_SIZE);

    info_buffer.data   = arena_alloc(&arena, 256);
    info_buffer.length = 256;
}







void editor_update_camera() {

    // Position control.
    Vec2f vel = VEC2F_ORIGIN;

    if (hold(SDLK_d)) {
        vel.x += 1.0f;
    } 
    if (hold(SDLK_a)) {
        vel.x -= 1.0f;
    }
    if (hold(SDLK_w)) {
        vel.y += 1.0f;
    }
    if (hold(SDLK_s)) {
        vel.y -= 1.0f;
    }
    
    if (!(fequal(vel.x, 0.0f) && fequal(vel.y, 0.0f))) {
        vel = vec2f_normalize(vel);
        vel = vec2f_multi_constant(vel, editor_params.camera_speed);
    }

    editor_camera_current_vel = vec2f_lerp(editor_camera_current_vel, vel, editor_params.camera_move_lerp_t);

    editor_camera.center = vec2f_sum(editor_camera.center, vec2f_multi_constant(editor_camera_current_vel, time_ptr->delta_time));



    // Zoom control.
    editor_camera_current_zoom_vel = lerp(editor_camera_current_zoom_vel, mouse_input_ptr->scrolled_y * editor_params.camera_zoom_speed, editor_params.camera_zoom_lerp_t);

    editor_camera_current_zoom += editor_camera_current_zoom_vel * time_ptr->delta_time;

    editor_camera_current_zoom = clamp(editor_camera_current_zoom, 0.0f, 1.0f);

    editor_camera.unit_scale = lerp(editor_params.camera_zoom_min, editor_params.camera_zoom_max, editor_camera_current_zoom);




}


void editor_update_mouse() {
    world_mouse_position_change = world_mouse_position;
    world_mouse_position = screen_to_camera(mouse_input_ptr->position, &editor_camera, window_ptr->width, window_ptr->height);
    world_mouse_position_change = vec2f_difference(world_mouse_position, world_mouse_position_change);


    if (mouse_input_ptr->left_pressed) {
        world_mouse_left_click_origin = world_mouse_position;

        // @Old: Basically if left shift is not holding user doesn't want to save what was previously selected.
        // Previous me was wrong, this is not the behaviour I want editor to have, better solution will be to provide different "modes" user can operate in on selected things.
        // For example: Translation mode, Rotation mode, Scaling mode, etc.
        // And when mouse is in range of selected things, ability to perfom certain operation will be active.
        if (!hold(SDLK_LSHIFT)) {
            // Clear selected flags.
            for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                switch (editor_selected_list[i].type) {
                    case EDITOR_VERTEX:
                        edges_list[editor_selected_list[i].edge_index].flags &= ~(EDITOR_EDGE_SELECTED | EDITOR_EDGE_VERTEX_SELECTED);
                        break;
                }
            }
            array_list_clear(&editor_selected_list);
        }

        // Selecting closes vertex if any.
        for (u32 i = 0; i < array_list_length(&edges_list); i++) {
            if (vec2f_distance(edges_list[i].vertex, world_mouse_position) < editor_params.selection_radius && !(edges_list[i].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
                edges_list[i].flags |= EDITOR_EDGE_VERTEX_SELECTED;
                array_list_append(&editor_selected_list, ((Editor_Selected) {
                            .type = EDITOR_VERTEX,
                            .edge_index = i,
                            }));

                // Chain selection, eveything that connected by edges.
                if (hold(SDLK_LALT)) {
                    u32 j = i;
                    while (true) {
                        if (edges_list[j].next_index == EDITOR_INVALID_INDEX) {
                            // @Incomplete: Make backward selection when working with non looped chains of edges.
                            break;
                        }

                        if (edges_list[j].next_index == i) {
                            break;
                        }

                        j = edges_list[j].next_index;

                        if (!(edges_list[j].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
                            edges_list[j].flags |= EDITOR_EDGE_VERTEX_SELECTED;
                            array_list_append(&editor_selected_list, ((Editor_Selected) {
                                        .type = EDITOR_VERTEX,
                                        .edge_index = j,
                                        }));
                        }
                    }
                }

                goto editor_left_mouse_select_end;
            }
        }

        // @Deprecated.
        // for (u32 i = 0; i < array_list_length(&quads_list); i++) {
        //     // Selecting closes vertex if any.
        //     for (u32 j = 0; j < VERTICIES_PER_QUAD; j++) {
        //         if (vec2f_distance(quads_list[i].quad.verts[j], world_mouse_position) < editor_params.selection_radius) {
        //             // If a quad with vertex that user selected is not selected, it gets selected. Yeah goodluck reading this comment in the future.
        //             if (!(quads_list[i].flags & EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED)) {
        //                 array_list_append(&editor_selected_list, ((Editor_Selected) {
        //                             .type = EDITOR_QUAD,
        //                             .quad = &quads_list[i],
        //                             }));
        //             }

        //         
        //             // Selected vertex flags are in the order p0 -> p2 -> p3 -> p1
        //             // Same order as verticies in quad.
        //             // So this will select needed vertex.
        //             quads_list[i].flags |= 1 << j;
        //             

        //             goto editor_left_mouse_select_end;
        //         }
        //     }
        // }

        // @Deprecated.
        // @Temporary: In the future loop over the verticies that are inside camera view boundaries.
        // for (u32 i = 0; i < array_list_length(&quads_list); i++) {
        //     // Selecting quad if it's center is touched.
        //     if (vec2f_distance(quad_center(&quads_list[i].quad), world_mouse_position) < editor_params.selection_radius) {
        //         // @Redundant?
        //         // Quad is selected, add it only if it is not already selected.
        //         if (!(quads_list[i].flags & EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED)) {
        //             quads_list[i].flags |= EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED;
        //             array_list_append(&editor_selected_list, ((Editor_Selected) {
        //                         .type = EDITOR_QUAD,
        //                         .quad = &quads_list[i],
        //                         }));
        //         }

        //         goto editor_left_mouse_select_end;
        //         break;
        //     }
        // }

editor_left_mouse_select_end:
        // Empty goto.

    }

    // Snapping mouse position.
    world_mouse_snapped_position = editor_mouse_snap_to_grid(world_mouse_position);
    world_mouse_snapped_left_click_origin = editor_mouse_snap_to_grid(world_mouse_left_click_origin);
    if (mouse_input_ptr->left_hold) {
        selection_move_offset = vec2f_difference(world_mouse_snapped_position, world_mouse_snapped_left_click_origin);
    }


    if (mouse_input_ptr->left_unpressed) {
        // For all selected elements.
        for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
            switch(editor_selected_list[i].type) {
                // @Deprecated.
                // case EDITOR_QUAD:
                //     for (u32 j = 0; j < VERTICIES_PER_QUAD; j++) {
                //         if (editor_selected_list[i].quad->flags & (1 << j)) {
                //             editor_selected_list[i].quad->quad.verts[j].x += selection_move_offset.x;
                //             editor_selected_list[i].quad->quad.verts[j].y += selection_move_offset.y;
                //         }
                //     }
                //     break;
                case EDITOR_VERTEX:
                    edges_list[editor_selected_list[i].edge_index].vertex.x += selection_move_offset.x;
                    edges_list[editor_selected_list[i].edge_index].vertex.y += selection_move_offset.y;
                    break;
            }
        }

        // If there were no elements selected, select all elements in the region.
        if (array_list_length(&editor_selected_list) == 0) {
            AABB selection_region = (AABB) {
                .p0 = vec2f_make(fminf(world_mouse_snapped_position.x, world_mouse_snapped_left_click_origin.x), fminf(world_mouse_snapped_position.y, world_mouse_snapped_left_click_origin.y)),
                .p1 = vec2f_make(fmaxf(world_mouse_snapped_position.x, world_mouse_snapped_left_click_origin.x), fmaxf(world_mouse_snapped_position.y, world_mouse_snapped_left_click_origin.y)),
            };

            // @Speed: Unoptimized, currenly searches through all elements.
            // Linearlly.
            for (u32 i = 0; i < array_list_length(&edges_list); i++) {
                if (aabb_touches_point(&selection_region, edges_list[i].vertex) && !(edges_list[i].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
                    edges_list[i].flags |= EDITOR_EDGE_VERTEX_SELECTED;
                    array_list_append(&editor_selected_list, ((Editor_Selected) {
                                .type = EDITOR_VERTEX,
                                .edge_index = i,
                                }));
                }
            }

            // @Deprecated.
            // for (u32 i = 0; i < array_list_length(&quads_list); i++) {
            //     for (u32 j = 0; j < VERTICIES_PER_QUAD; j++) {
            //         if (aabb_touches_point(&selection_region, quads_list[i].quad.verts[j])) {
            //             // Selected vertex flags are in the order p0 -> p2 -> p3 -> p1
            //             // Same order as verticies in quad.
            //             // So this will select needed vertex.
            //             quads_list[i].flags |= 1 << j;
            //         }
            //     }

            //     if (quads_list[i].flags & EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED) {
            //         array_list_append(&editor_selected_list, ((Editor_Selected) {
            //                     .type = EDITOR_QUAD,
            //                     .quad = &quads_list[i],
            //                     }));
            //     }
            // }
        }

        // Resetting move offset.
        selection_move_offset = VEC2F_ORIGIN;
    }



    // Toggle mouse menu.
    if (mouse_input_ptr->right_pressed) {
        world_mouse_right_click_origin = world_mouse_position;

        editor_ui_mouse_menu_toggle = !editor_ui_mouse_menu_toggle;
        editor_ui_mouse_menu_origin = vec2f_make(mouse_input_ptr->position.x, mouse_input_ptr->position.y - editor_params.ui_mouse_menu_element_height * editor_params.ui_mouse_menu_element_count);
    }
}





bool editor_update() {
    
    editor_update_camera();

    editor_update_mouse();

    
    return false;
}

void editor_draw() {
    Matrix4f projection;

    projection = camera_calculate_projection(&editor_camera, window_ptr->width, window_ptr->height);
    
    
    // Draw grid, with grid shader.
    shader_update_projection(grid_drawer_ptr->program, &projection);

    draw_begin(grid_drawer_ptr);

    Vec2f editor_camera_p0 = vec2f_make(editor_camera.center.x - window_ptr->width * 0.5f / editor_camera.unit_scale, editor_camera.center.y - window_ptr->height * 0.5f / editor_camera.unit_scale);
    Vec2f editor_camera_p1 = vec2f_make(editor_camera.center.x + window_ptr->width * 0.5f / editor_camera.unit_scale, editor_camera.center.y + window_ptr->height * 0.5f / editor_camera.unit_scale);

    float grid_quad[36] = {
        -1.0f, -1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p0.x, editor_camera_p0.y,
         1.0f, -1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p1.x, editor_camera_p0.y,
        -1.0f,  1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p0.x, editor_camera_p1.y,
         1.0f,  1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p1.x, editor_camera_p1.y,
    };

    draw_quad_data(grid_quad, 1);

    draw_end();



    // Drawing quads.     
    shader_update_projection(quad_drawer_ptr->program, &projection);

    draw_begin(quad_drawer_ptr);

    // @Deprecated.
    // Draw editor quads.   
    // for (u32 i = 0; i < array_list_length(&quads_list); i++) {
    //     draw_quad(quads_list[i].quad.verts[0], quads_list[i].quad.verts[1], quads_list[i].quad.verts[2], quads_list[i].quad.verts[3], .color = quads_list[i].color);

    //     for (u32 j = 0; j < VERTICIES_PER_QUAD; j++) {
    //         if (quads_list[i].flags & (1 << j)) {
    //             draw_dot(quads_list[i].quad.verts[j], VEC4F_RED, &editor_camera, NULL);
    //         } else {
    //             draw_dot(quads_list[i].quad.verts[j], VEC4F_CYAN, &editor_camera, NULL);
    //         }
    //     }
    // }
    


    // Draw verticies.
    for (u32 i = 0; i < array_list_length(&edges_list); i++) {
        if (edges_list[i].flags & EDITOR_EDGE_VERTEX_SELECTED) {
            draw_dot(edges_list[i].vertex, VEC4F_RED, &editor_camera, NULL);
            draw_dot(vec2f_sum(edges_list[i].vertex, selection_move_offset), VEC4F_YELLOW, &editor_camera, NULL);
        } else {
            draw_dot(edges_list[i].vertex, VEC4F_CYAN, &editor_camera, NULL);
        }
    }
    


    // Drawing selection region if holding mouse, and nothing is selected.
    if (mouse_input_ptr->left_hold && array_list_length(&editor_selected_list) == 0) {
        draw_rect(world_mouse_snapped_left_click_origin, world_mouse_snapped_position, .color = vec4f_make(0.4f, 0.4f, 0.85f, 0.4f));
    }

    draw_end();





    // Drawing lines. 
    shader_update_projection(line_drawer_ptr->program, &projection);

    line_draw_begin(line_drawer_ptr);


    // Draw editor edges.
    float normal_length = 16.0f / (float)editor_camera.unit_scale; // Normal length is calculated so it is relative to the camera zoom.

    for (u32 i = 0; i < array_list_length(&edges_list); i++) {
        if (edges_list[i].next_index == EDITOR_INVALID_INDEX) {
            continue;
        }

        Vec2f v0 = edges_list[i].vertex;
        Vec2f v1 = edges_list[edges_list[i].next_index].vertex;
        
        draw_line(v0, v1, VEC4F_WHITE, NULL);


        // -2 * edges_list[i].flipped_normal + 1 ----> true maps to -1, false maps to 1.
        Vec2f normal = vec2f_normalize(vec2f_make( (-2 * edges_list[i].flipped_normal + 1) * (v1.y - v0.y), (2 * edges_list[i].flipped_normal - 1) * (v1.x - v0.x) ));

        Vec2f midpoint = vec2f_make(v0.x + (v1.x - v0.x) / 2, v0.y + (v1.y - v0.y) / 2);
        draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(normal, normal_length)), VEC4F_BLUE, NULL);

    }

    // @Deprecated
    // // Draw editor quads outlines.
    // for (u32 i = 0; i < array_list_length(&quads_list); i++) {
    //     if (quads_list[i].flags & EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED) {
    //         // Original selected.
    //         draw_cross(quad_center(&quads_list[i].quad), VEC4F_YELLOW, &editor_camera, NULL);
    //         draw_quad_outline(quads_list[i].quad.verts[0], quads_list[i].quad.verts[1], quads_list[i].quad.verts[2], quads_list[i].quad.verts[3], VEC4F_YELLOW, NULL);


    //         // Preview of where selected quad.
    //         Quad preview_quad;
    //         for (u32 j = 0; j < VERTICIES_PER_QUAD; j++) {
    //             if (quads_list[i].flags & (1 << j)) {
    //                 preview_quad.verts[j] = vec2f_sum(quads_list[i].quad.verts[j], selection_move_offset);
    //             } else {
    //                 preview_quad.verts[j] = quads_list[i].quad.verts[j];
    //             }
    //         }
    //         draw_cross(quad_center(&preview_quad), VEC4F_RED, &editor_camera, NULL);
    //         draw_quad_outline(preview_quad.verts[0], preview_quad.verts[1], preview_quad.verts[2], preview_quad.verts[3], VEC4F_RED, NULL);

    //     } else {
    //         draw_quad_outline(quads_list[i].quad.verts[0], quads_list[i].quad.verts[1], quads_list[i].quad.verts[2], quads_list[i].quad.verts[3], VEC4F_WHITE, NULL);
    //         draw_cross(quad_center(&quads_list[i].quad), VEC4F_WHITE, &editor_camera, NULL);
    //     }
    // }

    line_draw_end();





    // Set ui to use specific font.
    ui_set_font(&font_small);



    projection = screen_calculate_projection(window_ptr->width, window_ptr->height);
    shader_update_projection(ui_quad_drawer_ptr->program, &projection);

    draw_begin(ui_quad_drawer_ptr);

    // Draw editor ui.
    UI_WINDOW(0, 0, window_ptr->width, window_ptr->height, 
            
            ui_text(
                str_format(info_buffer, 
                    "Window size: %dx%d\n"
                    "Vert count: %u\n"
                    "World mouse position: (%2.2f, %2.2f)\n"
                    "World mouse snapped position: (%2.2f, %2.2f)\n"
                    "World mouse snapped left click origin: (%2.2f, %2.2f)\n"
                    "Selected count: %u\n"
                    "Camera unit scale: %d\n"
                    , window_ptr->width, window_ptr->height, array_list_length(&quads_list) * 4, world_mouse_position.x, world_mouse_position.y, world_mouse_snapped_position.x, world_mouse_snapped_position.y, world_mouse_snapped_left_click_origin.x, world_mouse_snapped_left_click_origin.y, array_list_length(&editor_selected_list), editor_camera.unit_scale)
            );
    );

    // Menu processing.
    if (editor_ui_mouse_menu_toggle) {
        UI_WINDOW(editor_ui_mouse_menu_origin.x, editor_ui_mouse_menu_origin.y, editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height * editor_params.ui_mouse_menu_element_count,
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Add quad"))) {
                editor_add_quad_at(editor_mouse_snap_to_grid(world_mouse_right_click_origin));
            }
        );
    }

    draw_end();
}

void editor_get_verticies(Vec2f **verticies, s64 *verticies_count) {
    TODO("Getting editor result.");
}

void editor_add_quad() {
    u32 index = array_list_length(&edges_list);

    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { -1.0f, -1.0f }),
                .previous_index = index + 3,
                .next_index     = index + 1,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { 1.0f, -1.0f }),
                .previous_index = index,
                .next_index     = index + 2,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { 1.0f, 1.0f }),
                .previous_index = index + 1,
                .next_index     = index + 3,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { -1.0f, 1.0f }),
                .previous_index = index + 2,
                .next_index     = index,
                .flipped_normal = false,
                }));

}


void editor_add_quad_at(Vec2f position) {
    u32 index = array_list_length(&edges_list);

    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { -1.0f + position.x, -1.0f + position.y }),
                .previous_index = index + 3,
                .next_index     = index + 1,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { 1.0f + position.x, -1.0f + position.y }),
                .previous_index = index,
                .next_index     = index + 2,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { 1.0f + position.x, 1.0f + position.y }),
                .previous_index = index + 1,
                .next_index     = index + 3,
                .flipped_normal = false,
                }));
    array_list_append(&edges_list, ((Editor_Edge) {
                .vertex = ((Vec2f) { -1.0f + position.x, 1.0f + position.y }),
                .previous_index = index + 2,
                .next_index     = index,
                .flipped_normal = false,
                }));
}
