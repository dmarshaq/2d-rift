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



@Introspect;
typedef enum editor_state : u8 {
    EDITOR_STATE_SELECT,
    EDITOR_STATE_CUT,
} Editor_State;

static Editor_State editor_state;


// @Deprecated.
// static const u8 EDITOR_QUAD_BITMASK_ANY_POINT_SELECTED = EDITOR_QUAD_FLAG_P0_SELECTED | EDITOR_QUAD_FLAG_P1_SELECTED | EDITOR_QUAD_FLAG_P2_SELECTED | EDITOR_QUAD_FLAG_P3_SELECTED;


// @Deprecated.
//typedef struct editor_quad {
//    Editor_Quad_Flags flags;
//    Quad quad;
//    Vec4f color;
//} Editor_Quad;


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
// static Editor_Quad *quads_list;

static Editor_Edge *edges_list;
static const u32 EDITOR_INVALID_INDEX = 0xffffffff;




static Vec2f world_mouse_position;
static Vec2f world_mouse_position_change;
static Vec2f world_mouse_snapped_position;
static Vec2f world_mouse_left_click_origin;
static Vec2f world_mouse_right_click_origin;
static Vec2f world_mouse_snapped_left_click_origin;
static Vec2f world_mouse_snapped_right_click_origin;
static u32   selection_move_anchor_vertex_index;
static Vec2f selection_move_offset;


Vec2f editor_mouse_snap_to_grid(Vec2f mouse_position) {
    int x = (int)(mouse_position.x);
    int y = (int)(mouse_position.y);

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

    return vec2f_make((int)(mouse_position.x), (int)(mouse_position.y));
}


typedef enum editor_selected_type : u8 {
    EDITOR_NONE,
    EDITOR_EDGE,
    EDITOR_VERTEX,
} Editor_Selected_Type;



typedef struct editor_selected {
    Editor_Selected_Type type;

    union {
        // Editor_Quad *quad; // @Deprecated.
        u32 edge_index;
    };
} Editor_Selected;


static Editor_Selected *editor_selected_list;

static Vec2f editor_camera_current_vel;
static float editor_camera_current_zoom;
static float editor_camera_current_zoom_vel;


static bool editor_ui_mouse_menu_toggle;
static Vec2f editor_ui_mouse_menu_origin;


static u32 cut_selected_edge_index;
static Vec2f cut_position;



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
    editor_state = EDITOR_STATE_SELECT;
    editor_camera_current_vel = VEC2F_ORIGIN;
    editor_camera_current_zoom = 0.0f;
    editor_camera_current_zoom_vel = 0.0f;
    world_mouse_position = VEC2F_ORIGIN;
    world_mouse_position_change = VEC2F_ORIGIN;
    world_mouse_snapped_position = VEC2F_ORIGIN;
    world_mouse_left_click_origin = VEC2F_ORIGIN;
    world_mouse_right_click_origin = VEC2F_ORIGIN;
    world_mouse_snapped_left_click_origin = VEC2F_ORIGIN;
    world_mouse_snapped_right_click_origin = VEC2F_ORIGIN;
    selection_move_anchor_vertex_index = EDITOR_INVALID_INDEX;
    selection_move_offset = VEC2F_ORIGIN;
    editor_ui_mouse_menu_toggle = false;
    editor_ui_mouse_menu_origin = VEC2F_ORIGIN;
    cut_selected_edge_index = EDITOR_INVALID_INDEX;
    cut_position = VEC2F_ORIGIN;

    // Setting pointers to global state.
    quad_drawer_ptr    = &state->quad_drawer;
    grid_drawer_ptr    = &state->grid_drawer;
    ui_quad_drawer_ptr = &state->ui_quad_drawer;
    line_drawer_ptr    = &state->line_drawer;
    window_ptr         = &state->window;
    mouse_input_ptr    = &state->events.mouse_input;
    time_ptr           = &state->t;
    shader_table_ptr   = &state->shader_table;



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

void editor_clear_selection() {
    // Clear selected flags.
    for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
        switch (editor_selected_list[i].type) {
            case EDITOR_VERTEX:
                edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_VERTEX_SELECTED;
                break;
            case EDITOR_EDGE:
                edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_SELECTED;
                break;
        }
    }
    array_list_clear(&editor_selected_list);

}

// Chain selection, selects every vertex and edge that is connected together starting with given index.
void editor_chain_select(u32 start_edge_index) {
    // edges_list[start_edge_index].flags |= EDITOR_EDGE_VERTEX_SELECTED;
    // array_list_append(&editor_selected_list, ((Editor_Selected) {
    //             .type = EDITOR_VERTEX,
    //             .edge_index = start_edge_index,
    //             }));

    u32 j = start_edge_index;

    while (true) {

        if (!(edges_list[j].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
            edges_list[j].flags |= EDITOR_EDGE_VERTEX_SELECTED;
            array_list_append(&editor_selected_list, ((Editor_Selected) {
                        .type = EDITOR_VERTEX,
                        .edge_index = j,
                        }));
        }

        if (edges_list[j].next_index == EDITOR_INVALID_INDEX) {
            // @Incomplete: Make backward selection when working with non looped chains of edges.
            break;
        }


        if (!(edges_list[j].flags & EDITOR_EDGE_SELECTED)) {
            edges_list[j].flags |= EDITOR_EDGE_SELECTED;
            array_list_append(&editor_selected_list, ((Editor_Selected) {
                        .type = EDITOR_EDGE,
                        .edge_index = j,
                        }));
        }

        if (edges_list[j].next_index == start_edge_index) {
            break;
        }

        j = edges_list[j].next_index;

    }
}


void editor_select_handle_mouse_left_click() {
        // Saving information of where user clicked.
        world_mouse_left_click_origin = world_mouse_position;
        world_mouse_snapped_left_click_origin = world_mouse_snapped_position;

        // Reset vertex that acts as move acnhor.
        selection_move_anchor_vertex_index = EDITOR_INVALID_INDEX;

        if (!hold(SDLK_LSHIFT)) {
            editor_clear_selection();
        }

        // Selecting closest vertex or edge if any.
        u32 closest_selected_edge_index = EDITOR_INVALID_INDEX;
        for (u32 i = 0; i < array_list_length(&edges_list); i++) {

            // If found a vertex which user clicked, process it and return.
            if (vec2f_distance(edges_list[i].vertex, world_mouse_position) < editor_params.selection_radius) {
                selection_move_anchor_vertex_index = i;
                if (!(edges_list[i].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
                    if (hold(SDLK_LALT)) {
                        editor_chain_select(i);
                    } else {
                        edges_list[i].flags |= EDITOR_EDGE_VERTEX_SELECTED;
                        array_list_append(&editor_selected_list, ((Editor_Selected) {
                                    .type = EDITOR_VERTEX,
                                    .edge_index = i,
                                    }));
                    }
                    return;
                }
            }

            // If found an edge which user clicked, process it and return.
            if (edges_list[i].next_index != EDITOR_INVALID_INDEX && point_segment_min_distance(world_mouse_position, edges_list[i].vertex, edges_list[edges_list[i].next_index].vertex) < editor_params.selection_radius && !(edges_list[i].flags & EDITOR_EDGE_SELECTED)) {

                if (hold(SDLK_LALT)) {
                    editor_chain_select(i);
                    return;
                } else {
                    if (closest_selected_edge_index == EDITOR_INVALID_INDEX) {
                        closest_selected_edge_index = i;
                    }
                }

            }
        }

        // If nothing was selected and returned check if there is close edge that got selected. Plus selecting verticies it is made up of, because why not.
        if (closest_selected_edge_index != EDITOR_INVALID_INDEX) {
            edges_list[closest_selected_edge_index].flags |= EDITOR_EDGE_SELECTED;
            array_list_append(&editor_selected_list, ((Editor_Selected) {
                        .type = EDITOR_EDGE,
                        .edge_index = closest_selected_edge_index,
                        }));

            // if (!(edges_list[closest_selected_edge_index].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
            //     edges_list[closest_selected_edge_index].flags |= EDITOR_EDGE_VERTEX_SELECTED;
            //     array_list_append(&editor_selected_list, ((Editor_Selected) {
            //                 .type = EDITOR_VERTEX,
            //                 .edge_index = closest_selected_edge_index,
            //                 }));
            // }

            // if (!(edges_list[edges_list[closest_selected_edge_index].next_index].flags & EDITOR_EDGE_VERTEX_SELECTED)) {
            //     edges_list[edges_list[closest_selected_edge_index].next_index].flags |= EDITOR_EDGE_VERTEX_SELECTED;
            //     array_list_append(&editor_selected_list, ((Editor_Selected) {
            //                 .type = EDITOR_VERTEX,
            //                 .edge_index = edges_list[closest_selected_edge_index].next_index,
            //                 }));
            // }
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
}


void editor_select_handle_mouse_right_click() {
    // Saving information of where user clicked.
    world_mouse_right_click_origin = world_mouse_position;
    world_mouse_snapped_right_click_origin = world_mouse_snapped_position;

    editor_ui_mouse_menu_toggle = !editor_ui_mouse_menu_toggle;
    editor_ui_mouse_menu_origin = vec2f_make(mouse_input_ptr->position.x, mouse_input_ptr->position.y - editor_params.ui_mouse_menu_element_height * editor_params.ui_mouse_menu_element_count);
}


void editor_update_mouse() {
    // Updating positions and all necessary mouse movement information.
    world_mouse_position_change = world_mouse_position;
    world_mouse_position = screen_to_camera(mouse_input_ptr->position, &editor_camera, window_ptr->width, window_ptr->height);
    world_mouse_position_change = vec2f_difference(world_mouse_position, world_mouse_position_change);
    world_mouse_snapped_position = editor_mouse_snap_to_grid(world_mouse_position);

    switch (editor_state) {
        case EDITOR_STATE_SELECT:

            // Handle clicks.
            if (mouse_input_ptr->left_pressed) {
                editor_select_handle_mouse_left_click();
            } else if (mouse_input_ptr->right_pressed) {
                editor_select_handle_mouse_right_click();
            }

            // Handle holds.
            if (mouse_input_ptr->left_hold && selection_move_anchor_vertex_index != EDITOR_INVALID_INDEX) {
                selection_move_offset = vec2f_difference(world_mouse_snapped_position, edges_list[selection_move_anchor_vertex_index].vertex);
            }

            // Handle unpresses.
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
                        .p0 = vec2f_make(fminf(world_mouse_position.x, world_mouse_left_click_origin.x), fminf(world_mouse_position.y, world_mouse_left_click_origin.y)),
                        .p1 = vec2f_make(fmaxf(world_mouse_position.x, world_mouse_left_click_origin.x), fmaxf(world_mouse_position.y, world_mouse_left_click_origin.y)),
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

                            if (aabb_touches_point(&selection_region, edges_list[edges_list[i].next_index].vertex) && !(edges_list[i].flags & EDITOR_EDGE_SELECTED)) {
                                edges_list[i].flags |= EDITOR_EDGE_SELECTED;
                                array_list_append(&editor_selected_list, ((Editor_Selected) {
                                            .type = EDITOR_EDGE,
                                            .edge_index = i,
                                            }));
                            }
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


            // Flipping normal if n is pressed on any selected edges.
            if (pressed(SDLK_n)) {
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_EDGE:
                            edges_list[editor_selected_list[i].edge_index].flipped_normal = !edges_list[editor_selected_list[i].edge_index].flipped_normal;
                            break;
                    }
                }
            }

            if (pressed(SDLK_x)) {
                u32 index = EDITOR_INVALID_INDEX;
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_EDGE:
                            edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_SELECTED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                        case EDITOR_VERTEX:
                            if (index == EDITOR_INVALID_INDEX) {
                                index = editor_selected_list[i].edge_index;
                            }
                            edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_VERTEX_SELECTED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                        default:
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                    }
                }
                if (index != EDITOR_INVALID_INDEX) {
                    // Assumes any legal lines are looped.
                    int verticies_count = 1;
                    for (u32 j = index; j != edges_list[index].previous_index; j = edges_list[j].next_index) {
                        verticies_count++;
                    }

                    if (verticies_count > 3) {
                        edges_list[edges_list[index].previous_index].next_index = edges_list[index].next_index;
                        edges_list[edges_list[index].next_index].previous_index = edges_list[index].previous_index;
                        array_list_unordered_remove(&edges_list, index);

                        // After remove reroute the references of the moved edge.
                        if (index < array_list_length(&edges_list)) {
                            edges_list[edges_list[index].previous_index].next_index = index;
                            edges_list[edges_list[index].next_index].previous_index = index;
                        }
                    }
                }
            }

            if (pressed(SDLK_c)) {
                // Filtering out every edge, removing other editor objects.
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_EDGE:
                            break;
                        case EDITOR_VERTEX:
                            edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_VERTEX_SELECTED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                        default:
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                    }
                }

                selection_move_anchor_vertex_index = EDITOR_INVALID_INDEX;

                if (array_list_length(&editor_selected_list) > 0) {
                    editor_state = EDITOR_STATE_CUT;
                }
            }

            break;
        case EDITOR_STATE_CUT:

            // Only edges are selected, since before the cut state was entered other editor things were deselected.
            cut_selected_edge_index = editor_selected_list[0].edge_index;
            float min_distance = point_segment_min_distance(world_mouse_position, edges_list[cut_selected_edge_index].vertex, edges_list[edges_list[cut_selected_edge_index].next_index].vertex);

            for (u32 i = 1; i < array_list_length(&editor_selected_list); i++) {
                u32 j = editor_selected_list[i].edge_index;
                float next_distance = point_segment_min_distance(world_mouse_position, edges_list[j].vertex, edges_list[edges_list[j].next_index].vertex);
                if (next_distance < min_distance) {
                    cut_selected_edge_index = j;
                    min_distance = next_distance;
                }
            }


            // Projecting potential cut position.
            Vec2f relative_mouse_position = vec2f_difference(world_mouse_position, edges_list[cut_selected_edge_index].vertex);
            Vec2f relative_edge_vector = vec2f_difference(edges_list[edges_list[cut_selected_edge_index].next_index].vertex, edges_list[cut_selected_edge_index].vertex);
            Vec2f relative_edge_vector_dir = vec2f_normalize(relative_edge_vector);

            float relative_cut_position_magnitude = clamp(vec2f_dot(relative_mouse_position, relative_edge_vector_dir), 0.0f, vec2f_magnitude(relative_edge_vector));

            cut_position = vec2f_sum(edges_list[cut_selected_edge_index].vertex, vec2f_multi_constant(relative_edge_vector_dir, relative_cut_position_magnitude));

            

    
            

            // Handle clicks.
            if (mouse_input_ptr->left_pressed) {
                // Saving information of where user clicked.
                world_mouse_left_click_origin = world_mouse_position;
                world_mouse_snapped_left_click_origin = world_mouse_snapped_position;
                

                array_list_append(&edges_list, ((Editor_Edge) {
                            .vertex = cut_position,
                            .previous_index = cut_selected_edge_index,
                            .next_index     = edges_list[cut_selected_edge_index].next_index,
                            .flipped_normal = edges_list[cut_selected_edge_index].flipped_normal,
                            .flags = EDITOR_EDGE_SELECTED,
                            }));

                edges_list[cut_selected_edge_index].next_index = array_list_length(&edges_list) - 1;
                array_list_append(&editor_selected_list, ((Editor_Selected) {
                            .type = EDITOR_EDGE,
                            .edge_index = array_list_length(&edges_list) - 1,
                            }));

            }

            if (pressed(SDLK_ESCAPE)) {
                // Saving information of where user clicked.
                world_mouse_right_click_origin = world_mouse_position;
                world_mouse_snapped_right_click_origin = world_mouse_snapped_position;
                
                cut_selected_edge_index = EDITOR_INVALID_INDEX;
                editor_state = EDITOR_STATE_SELECT;
            }


            break;
    }

}





bool editor_update() {

    // if (pressed(SDLK_LEFTBRACKET)) {
    // }

    // if (pressed(SDLK_RIGHTBRACKET)) {
    // }
    
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


    if (editor_state == EDITOR_STATE_CUT) {
        if (cut_selected_edge_index != EDITOR_INVALID_INDEX) {
            draw_dot(cut_position, VEC4F_RED, &editor_camera, NULL);
        }
    }
    


    // Drawing selection region if holding mouse, and nothing is selected.
    if (mouse_input_ptr->left_hold && array_list_length(&editor_selected_list) == 0) {
        draw_rect(world_mouse_left_click_origin, world_mouse_position, .color = vec4f_make(0.4f, 0.4f, 0.85f, 0.4f));
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
        
        if (edges_list[i].flags & EDITOR_EDGE_SELECTED) {
            draw_line(v0, v1, VEC4F_YELLOW, NULL);
        } else {
            draw_line(v0, v1, VEC4F_WHITE, NULL);
        }


        // -2 * edges_list[i].flipped_normal + 1 ----> true maps to -1, false maps to 1.
        Vec2f normal = vec2f_normalize(vec2f_make( (-2 * edges_list[i].flipped_normal + 1) * (v1.y - v0.y), (2 * edges_list[i].flipped_normal - 1) * (v1.x - v0.x) ));

        Vec2f midpoint = vec2f_make(v0.x + (v1.x - v0.x) / 2, v0.y + (v1.y - v0.y) / 2);
        if (edges_list[i].flipped_normal) {
            draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(normal, normal_length)), VEC4F_RED, NULL);
        } else {
            draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(normal, normal_length)), VEC4F_BLUE, NULL);
        }

    }

    if (editor_state == EDITOR_STATE_CUT) {
        if (cut_selected_edge_index != EDITOR_INVALID_INDEX) {
            draw_line(edges_list[cut_selected_edge_index].vertex, edges_list[edges_list[cut_selected_edge_index].next_index].vertex, VEC4F_PINK, NULL);
        }
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
                    "Current editor state: %d\n"
                    "Vert count: %u\n"
                    "World mouse position: (%2.2f, %2.2f)\n"
                    "World mouse snapped position: (%2.2f, %2.2f)\n"
                    "World mouse snapped left click origin: (%2.2f, %2.2f)\n"
                    "Selected count: %u\n"
                    "Camera unit scale: %d\n"
                    , window_ptr->width, window_ptr->height, (u8)editor_state, array_list_length(&edges_list), world_mouse_position.x, world_mouse_position.y, world_mouse_snapped_position.x, world_mouse_snapped_position.y, world_mouse_snapped_left_click_origin.x, world_mouse_snapped_left_click_origin.y, array_list_length(&editor_selected_list), editor_camera.unit_scale)
            );
    );

    // Menu processing.
    if (editor_ui_mouse_menu_toggle) {
        UI_WINDOW(editor_ui_mouse_menu_origin.x, editor_ui_mouse_menu_origin.y, editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height * editor_params.ui_mouse_menu_element_count,
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Add quad"))) {
                editor_add_quad_at(world_mouse_snapped_right_click_origin);
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
