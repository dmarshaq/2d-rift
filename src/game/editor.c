#include "game/editor.h"

#include "meta_generated.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/console.h"
#include "game/level.h"

#include "core/mathf.h"
#include "core/structs.h"
#include "core/arena.h"
#include "core/str.h"
#include "core/file.h"



@Introspect;
typedef enum editor_state : u8 {
    EDITOR_STATE_SELECT,
    EDITOR_STATE_CUT,
    EDITOR_STATE_ROTATE,
} Editor_State;

static Editor_State editor_state;




typedef enum editor_edge_flags : u8 {
    EDITOR_EDGE_SELECTED        = 0x01,
    EDITOR_EDGE_VERTEX_SELECTED = 0x02,
    EDITOR_EDGE_BUILT           = 0x04,
} Editor_Edge_Flags;

typedef struct editor_edge {
    Vec2f vertex;
    u32 previous_index;
    u32 next_index;
    bool flipped_normal;
    Editor_Edge_Flags flags;
} Editor_Edge;


typedef enum editor_entity_flags : u8 {
    EDITOR_ENTITY_SELECTED      = 0x01,
    EDITOR_ENTITY_REMOVED       = 0x02,
} Editor_Entity_Flags;

typedef struct editor_entity {
    Entity_Type type;
    OBB bound_box;
    Editor_Entity_Flags flags;
} Editor_Entity;





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





static Editor_Edge *edges_list;
static u32 *edges_deleted_indicies_list;

static Editor_Entity *entity_list;

static const u32 EDITOR_INVALID_INDEX = 0xffffffff;






static Vec2f world_mouse_position;
static Vec2f world_mouse_position_change;
static Vec2f world_mouse_snapped_position;
static Vec2f world_mouse_left_click_origin;
static Vec2f world_mouse_right_click_origin;
static Vec2f world_mouse_snapped_left_click_origin;
static Vec2f world_mouse_snapped_right_click_origin;
static Vec2f selection_move_anchor_vector;
static Vec2f selection_move_offset;
static float grid_scale;


Vec2f editor_mouse_snap_to_grid(Vec2f mouse_position) {
    mouse_position = vec2f_multi_constant(mouse_position, grid_scale);
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

    return vec2f_make((int)(mouse_position.x) / grid_scale, (int)(mouse_position.y) / grid_scale);
}


typedef enum editor_selected_type : u8 {
    EDITOR_NONE,
    EDITOR_EDGE,
    EDITOR_VERTEX,
    EDITOR_ENTITY,
} Editor_Selected_Type;



typedef struct editor_selected {
    Editor_Selected_Type type;

    union {
        u32 edge_index;
        u32 entity_index;
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

static Vec2f rotate_origin_vector;
static Vec2f rotate_anchor;
static float rotate_rad_offset;



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
    selection_move_anchor_vector = VEC2F_ORIGIN;
    selection_move_offset = VEC2F_ORIGIN;
    editor_ui_mouse_menu_toggle = false;
    editor_ui_mouse_menu_origin = VEC2F_ORIGIN;
    cut_selected_edge_index = EDITOR_INVALID_INDEX;
    cut_position = VEC2F_ORIGIN;
    rotate_origin_vector = VEC2F_ORIGIN;
    rotate_anchor = VEC2F_ORIGIN;
    grid_scale = 1.0f;

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
    edges_deleted_indicies_list = array_list_make(u32, 8, &std_allocator);

    entity_list = array_list_make(Editor_Entity, 8, &std_allocator);

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

    info_buffer.data   = arena_alloc(&arena, 512);
    info_buffer.length = 512;
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
            case EDITOR_ENTITY:
                entity_list[editor_selected_list[i].entity_index].flags &= ~EDITOR_ENTITY_SELECTED;
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
        selection_move_anchor_vector = world_mouse_snapped_position;

        if (!hold(SDLK_LSHIFT)) {
            editor_clear_selection();
        }

        // Selecting closest vertex or edge if any.
        u32 closest_selected_edge_index = EDITOR_INVALID_INDEX;
        for (u32 i = 0; i < array_list_length(&edges_list); i++) {
            // If found a vertex which user clicked, process it and return.
            if (vec2f_distance(edges_list[i].vertex, world_mouse_position) < editor_params.selection_radius) {
                selection_move_anchor_vector = edges_list[i].vertex;
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
            return;
        }

        // Selecting entities if nothing was selecting.
        AABB entity_aabb;
        for (u32 i = 0; i < array_list_length(&entity_list); i++) {
            entity_aabb = obb_enclose_in_aabb(&entity_list[i].bound_box);
            
            if (aabb_touches_point(&entity_aabb, world_mouse_position) && !(entity_list[i].flags & EDITOR_ENTITY_SELECTED)) {
                selection_move_anchor_vector = entity_list[i].bound_box.center;
                entity_list[i].flags |= EDITOR_ENTITY_SELECTED;
                array_list_append(&editor_selected_list, ((Editor_Selected) {
                            .type = EDITOR_ENTITY,
                            .entity_index = i,
                            }));
                return;
            }
        }
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
            if (mouse_input_ptr->left_hold) {
                selection_move_offset = vec2f_difference(world_mouse_snapped_position, selection_move_anchor_vector);
            }

            // Handle unpresses.
            if (mouse_input_ptr->left_unpressed) {
                // For all selected elements.
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_VERTEX:
                            edges_list[editor_selected_list[i].edge_index].vertex.x += selection_move_offset.x;
                            edges_list[editor_selected_list[i].edge_index].vertex.y += selection_move_offset.y;
                            break;
                        case EDITOR_ENTITY:
                            entity_list[editor_selected_list[i].entity_index].bound_box.center.x += selection_move_offset.x;
                            entity_list[editor_selected_list[i].entity_index].bound_box.center.y += selection_move_offset.y;
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

                            if (edges_list[i].next_index != EDITOR_INVALID_INDEX && aabb_touches_point(&selection_region, edges_list[edges_list[i].next_index].vertex) && !(edges_list[i].flags & EDITOR_EDGE_SELECTED)) {
                                edges_list[i].flags |= EDITOR_EDGE_SELECTED;
                                array_list_append(&editor_selected_list, ((Editor_Selected) {
                                            .type = EDITOR_EDGE,
                                            .edge_index = i,
                                            }));
                            }
                        }
                    }

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
                // First remove EVERY reference to deleted verticies, by redistributing them among their neighbores.
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_EDGE:
                            edges_list[editor_selected_list[i].edge_index].flags &= ~EDITOR_EDGE_SELECTED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                        case EDITOR_ENTITY:
                            entity_list[editor_selected_list[i].entity_index].flags &= ~EDITOR_ENTITY_SELECTED;
                            entity_list[editor_selected_list[i].entity_index].flags |= EDITOR_ENTITY_REMOVED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                        case EDITOR_VERTEX:
                            u32 index = editor_selected_list[i].edge_index;

                            edges_list[index].flags &= ~EDITOR_EDGE_VERTEX_SELECTED;
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            
                            if (edges_list[index].previous_index != EDITOR_INVALID_INDEX) {
                                if (edges_list[index].next_index == EDITOR_INVALID_INDEX || edges_list[edges_list[index].next_index].next_index != edges_list[index].previous_index) {
                                    edges_list[edges_list[index].previous_index].next_index = edges_list[index].next_index;

                                } else {
                                    edges_list[edges_list[index].previous_index].next_index = EDITOR_INVALID_INDEX;                   
                                }
                            }
                            if (edges_list[index].next_index != EDITOR_INVALID_INDEX) {
                                if (edges_list[index].previous_index == EDITOR_INVALID_INDEX || edges_list[edges_list[index].previous_index].previous_index != edges_list[index].next_index) {
                                    edges_list[edges_list[index].next_index].previous_index = edges_list[index].previous_index;                   
                                } else {
                                    edges_list[edges_list[index].next_index].previous_index = EDITOR_INVALID_INDEX;                   
                                }
                            }

                            // Binary insertion to keep array sorted from in INCREASING order.
                            u32 length = array_list_length(&edges_deleted_indicies_list);
                            u32 start = 0;
                            while (length > 0) {
                                if (index < edges_deleted_indicies_list[start + (u32)(length / 2)]) {
                                    // Go to the left.
                                    length = (u32)(length / 2);
                                } else {
                                    // Go to the right.
                                    start += (u32)(length / 2) + 1;
                                    length = (length - 1) - (u32)(length / 2);
                                }
                            }
                            array_list_add(&edges_deleted_indicies_list, start, index);

                            break;
                        default:
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                    }
                }
                
                // Readdress edges that were removed.
                u32 i;
                for (u32 l = array_list_length(&edges_deleted_indicies_list); l > 0; l--) {
                    i = edges_deleted_indicies_list[l - 1];
                    if (i == array_list_length(&edges_list) - 1) {
                        ((Array_List_Header *)((u8 *)edges_list - sizeof(Array_List_Header)))->length--;
                    } else {
                        array_list_unordered_remove(&edges_list, i);

                        // Readdress, last edge that was moved to i index.
                        if (edges_list[i].previous_index != EDITOR_INVALID_INDEX) {
                            edges_list[edges_list[i].previous_index].next_index = i;
                        }
                        if (edges_list[i].next_index != EDITOR_INVALID_INDEX) {
                            edges_list[edges_list[i].next_index].previous_index = i;
                        }
                    }
                }
                array_list_clear(&edges_deleted_indicies_list);

                
                // Safely remove entities that were marked as removed.
                for (u32 j = 0; j < array_list_length(&entity_list); j++) {
                    if (entity_list[j].flags & EDITOR_ENTITY_REMOVED) {
                        array_list_unordered_remove(&entity_list, j);
                        j--;
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

                // @Redundant?
                // selection_move_anchor_vector = world_mouse_left_click_origin;

                if (array_list_length(&editor_selected_list) > 0) {
                    editor_state = EDITOR_STATE_CUT;
                }
            }

            if (pressed(SDLK_r)) {
                // Removing everything except entities.
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    switch(editor_selected_list[i].type) {
                        case EDITOR_ENTITY:
                            break;
                        default:
                            array_list_unordered_remove(&editor_selected_list, i);
                            i--;
                            break;
                    }
                }

                if (array_list_length(&editor_selected_list) > 0) {
                    rotate_origin_vector = world_mouse_position;
                    editor_state = EDITOR_STATE_ROTATE;
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

                edges_list[edges_list[cut_selected_edge_index].next_index].previous_index = array_list_length(&edges_list) - 1;
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
        case EDITOR_STATE_ROTATE:
            // For right now, simple rotationg is implemented around the individual entities centers and without geometry rotation.

            rotate_anchor = VEC2F_ORIGIN;
            for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                rotate_anchor = vec2f_sum(rotate_anchor, entity_list[editor_selected_list[i].entity_index].bound_box.center);
            }
            rotate_anchor = vec2f_divide_constant(rotate_anchor, array_list_length(&editor_selected_list));

            Vec2f relative_rotate_mouse_position = vec2f_normalize(vec2f_difference(world_mouse_position, rotate_anchor));
            Vec2f relative_rotate_origin_vector = vec2f_normalize(vec2f_difference(rotate_origin_vector, rotate_anchor));

            rotate_rad_offset = atan2f(vec2f_cross(relative_rotate_origin_vector, relative_rotate_mouse_position), vec2f_dot(relative_rotate_origin_vector, relative_rotate_mouse_position));
            
            rotate_rad_offset = (int)(rotate_rad_offset / (PI / 8)) * (PI / 8);


            // Handle clicks.
            if (mouse_input_ptr->left_pressed) {
                // Saving information of where user clicked.
                world_mouse_left_click_origin = world_mouse_position;
                world_mouse_snapped_left_click_origin = world_mouse_snapped_position;
                

                
                for (u32 i = 0; i < array_list_length(&editor_selected_list); i++) {
                    entity_list[editor_selected_list[i].entity_index].bound_box.rot += rotate_rad_offset;
                }

                rotate_origin_vector = world_mouse_position;

            }

            if (pressed(SDLK_ESCAPE)) {
                // Saving information of where user clicked.
                world_mouse_right_click_origin = world_mouse_position;
                world_mouse_snapped_right_click_origin = world_mouse_snapped_position;
                
                editor_state = EDITOR_STATE_SELECT;
            }

            break;
    }


}





bool editor_update() {

    if (pressed(SDLK_LEFTBRACKET)) {
        grid_scale *= 2.0f;
    }

    if (pressed(SDLK_RIGHTBRACKET)) {
        grid_scale *= 0.5f;
    }
    
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

    float grid_quad[40] = {
        -1.0f, -1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p0.x, editor_camera_p0.y, grid_scale,
         1.0f, -1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p1.x, editor_camera_p0.y, grid_scale,
        -1.0f,  1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p0.x, editor_camera_p1.y, grid_scale,
         1.0f,  1.0f, editor_camera.unit_scale, 0.2f, 0.2f, 0.2f, 1.0f, editor_camera_p1.x, editor_camera_p1.y, grid_scale,
    };

    draw_quad_data(grid_quad, 1);

    draw_end();



    // Drawing quads.     
    shader_update_projection(quad_drawer_ptr->program, &projection);

    draw_begin(quad_drawer_ptr);


    // Draw entities.
    for (u32 i = 0; i < array_list_length(&entity_list); i++) {
        switch(entity_list[i].type) {
            case PLAYER:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_PLAYER);
                break;
            case PROP_PHYSICS:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_PROP_PHYSICS, .offset_angle = entity_list[i].bound_box.rot);
                break;
            case RAY_EMITTER:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_RAY_EMITTER, .offset_angle = entity_list[i].bound_box.rot);
                break;
            case RAY_HARVESTER:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_RAY_EMITTER, .offset_angle = entity_list[i].bound_box.rot);
                break;
            case MIRROR:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_MIRROR, .offset_angle = entity_list[i].bound_box.rot);
                break;
            case GLASS:
                draw_rect(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), .color = LEVEL_COLOR_GLASS, .offset_angle = entity_list[i].bound_box.rot);
                break;
        }
    }


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




    // Draw entities aabb outlines.
    AABB entity_aabb;
    Vec2f midpoint;
    for (u32 i = 0; i < array_list_length(&entity_list); i++) {
        switch(entity_list[i].type) {
            case RAY_EMITTER:
                midpoint = vec2f_midpoint(obb_p2(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box));
                draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(obb_right(&entity_list[i].bound_box), 4.0f)), VEC4F_RED, NULL);
                break;
            case RAY_HARVESTER:
                midpoint = vec2f_midpoint(obb_p2(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box));
                draw_line(midpoint, vec2f_sum(midpoint, vec2f_multi_constant(obb_right(&entity_list[i].bound_box), 4.0f)), VEC4F_GREEN, NULL);
                break;
        }

        entity_aabb = obb_enclose_in_aabb(&entity_list[i].bound_box);

        
        if (entity_list[i].flags & EDITOR_ENTITY_SELECTED) {
            

            if (editor_state == EDITOR_STATE_ROTATE) {
                draw_rect_outline(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), VEC4F_RED, entity_list[i].bound_box.rot, NULL);
                entity_list[i].bound_box.rot += rotate_rad_offset;
                draw_rect_outline(obb_p0(&entity_list[i].bound_box), obb_p1(&entity_list[i].bound_box), VEC4F_YELLOW, entity_list[i].bound_box.rot, NULL);
                draw_cross(entity_list[i].bound_box.center, VEC4F_YELLOW, &editor_camera, NULL);
                entity_list[i].bound_box.rot -= rotate_rad_offset;
            } else {
                draw_rect_outline(entity_aabb.p0, entity_aabb.p1, VEC4F_RED, 0.0f, NULL);
                draw_cross(entity_list[i].bound_box.center, VEC4F_RED, &editor_camera, NULL);

                draw_rect_outline(vec2f_sum(entity_aabb.p0, selection_move_offset), vec2f_sum(entity_aabb.p1, selection_move_offset), VEC4F_YELLOW, 0.0f, NULL);
                draw_cross(vec2f_sum(entity_list[i].bound_box.center, selection_move_offset), VEC4F_YELLOW, &editor_camera, NULL);
            }
        } else {
            draw_rect_outline(entity_aabb.p0, entity_aabb.p1, VEC4F_WHITE, 0.0f, NULL);
            draw_cross(entity_list[i].bound_box.center, VEC4F_WHITE, &editor_camera, NULL);
        }
    }






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

        midpoint = vec2f_make(v0.x + (v1.x - v0.x) / 2, v0.y + (v1.y - v0.y) / 2);
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

    if (editor_state == EDITOR_STATE_ROTATE) {
        draw_line(rotate_anchor, world_mouse_position, VEC4F_PINK, NULL);
        draw_line(rotate_anchor, rotate_origin_vector, VEC4F_PINK, NULL);
    }


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
                    "Grid scale: %2.2f\n"
                    , window_ptr->width, window_ptr->height, (u8)editor_state, array_list_length(&edges_list), world_mouse_position.x, world_mouse_position.y, world_mouse_snapped_position.x, world_mouse_snapped_position.y, world_mouse_snapped_left_click_origin.x, world_mouse_snapped_left_click_origin.y, array_list_length(&editor_selected_list), editor_camera.unit_scale, grid_scale
                    )
            );
    );

    // Menu processing.
    if (editor_ui_mouse_menu_toggle) {
        UI_WINDOW(editor_ui_mouse_menu_origin.x, editor_ui_mouse_menu_origin.y, editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height * editor_params.ui_mouse_menu_element_count,
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Quad"))) {
                editor_add_quad(world_mouse_snapped_right_click_origin);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Player"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, PLAYER);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Prop Physics"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, PROP_PHYSICS);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Ray Emitter"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, RAY_EMITTER);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Ray Harvester"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, RAY_HARVESTER);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Mirror"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, MIRROR);
            }
            if (UI_BUTTON(vec2f_make(editor_params.ui_mouse_menu_width, editor_params.ui_mouse_menu_element_height), CSTR("Glass"))) {
                editor_add_entity(world_mouse_snapped_right_click_origin, GLASS);
            }
        );
    }

    draw_end();
}



const String EDITOR_FILE_PATH   = STR_BUFFER("res/editor/");
const String EDITOR_FILE_FORMAT = STR_BUFFER(".editor");

void editor_write(String name) {
    // Saves current editor level by the name provided.
    char file_name[EDITOR_FILE_PATH.length + name.length + EDITOR_FILE_FORMAT.length + 1];
    str_copy_to(EDITOR_FILE_PATH, file_name);
    str_copy_to(name, file_name + EDITOR_FILE_PATH.length);
    str_copy_to(EDITOR_FILE_FORMAT, file_name + EDITOR_FILE_PATH.length + name.length);
    file_name[EDITOR_FILE_PATH.length + name.length + EDITOR_FILE_FORMAT.length] = '\0';

    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        console_log("Couldn't open the editor file for writing '%s'.\n", file_name);
        return;
    }

    u64 written = 0; 
    
    written += fwrite_u32(EDITOR_FORMAT_HEADER, file) * 4;

    // Serializing each edge information.
    written += fwrite_u32(array_list_length(&edges_list), file) * 4;
    
    for (u32 i = 0; i < array_list_length(&edges_list); i++) {
        written += fwrite_float(edges_list[i].vertex.x, file) * 4;
        written += fwrite_float(edges_list[i].vertex.y, file) * 4;
        written += fwrite_u32(edges_list[i].previous_index, file) * 4;
        written += fwrite_u32(edges_list[i].next_index, file) * 4;
        written += fwrite(&edges_list[i].flipped_normal, 1, 1, file);
    }
    
    // Serializing each entity information.
    written += fwrite_u32(array_list_length(&entity_list), file) * 4;
    
    for (u32 i = 0; i < array_list_length(&entity_list); i++) {
        written += fwrite(&entity_list[i].type, 1, 1, file);
        written += fwrite_float(entity_list[i].bound_box.center.x, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.center.y, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.dimensions.x, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.dimensions.y, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.rot, file) * 4;
    }

    fclose(file);

    console_log("Written %llu bytes to editor file '%s'.\n", written, file_name);
}

void editor_read(String name) {
    char file_name[EDITOR_FILE_PATH.length + name.length + EDITOR_FILE_FORMAT.length + 1];
    str_copy_to(EDITOR_FILE_PATH, file_name);
    str_copy_to(name, file_name + EDITOR_FILE_PATH.length);
    str_copy_to(EDITOR_FILE_FORMAT, file_name + EDITOR_FILE_PATH.length + name.length);
    file_name[EDITOR_FILE_PATH.length + name.length + EDITOR_FILE_FORMAT.length] = '\0';

    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        console_log("Couldn't open the editor file for reading '%s'.\n", file_name);
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

    if (read_u32(&ptr) != EDITOR_FORMAT_HEADER) {
        console_log("Failure reading the editor file '%s', format header doesn't match.\n", file_name);
        free(buffer);
        return;
    }

    u32 edge_count = read_u32(&ptr);

    array_list_clear(&edges_list);
    for (u32 i = 0; i < edge_count; i++) {
        array_list_append(&edges_list, ((Editor_Edge) {0}));
        edges_list[i].vertex.x = read_float(&ptr);
        edges_list[i].vertex.y = read_float(&ptr);
        edges_list[i].previous_index = read_u32(&ptr);
        edges_list[i].next_index = read_u32(&ptr);
        edges_list[i].flipped_normal = read_byte(&ptr);
    }

    u32 entity_count = read_u32(&ptr);

    array_list_clear(&entity_list);
    for (u32 i = 0; i < entity_count; i++) {
        array_list_append(&entity_list, ((Editor_Entity) {0}));
        entity_list[i].type = read_byte(&ptr);
        entity_list[i].bound_box.center.x = read_float(&ptr);
        entity_list[i].bound_box.center.y = read_float(&ptr);
        entity_list[i].bound_box.dimensions.x = read_float(&ptr);
        entity_list[i].bound_box.dimensions.y = read_float(&ptr);
        entity_list[i].bound_box.rot = read_float(&ptr);
    }


    
    free(buffer);
    
    console_log("Read %llu bytes into the editor from '%s' level file.\n", ptr - buffer, file_name);
}


void editor_build(String name) {
    // Saves current editor level by the name provided.
    char file_name[LEVEL_FILE_PATH.length + name.length + LEVEL_FILE_FORMAT.length + 1];
    str_copy_to(LEVEL_FILE_PATH, file_name);
    str_copy_to(name, file_name + LEVEL_FILE_PATH.length);
    str_copy_to(LEVEL_FILE_FORMAT, file_name + LEVEL_FILE_PATH.length + name.length);
    file_name[LEVEL_FILE_PATH.length + name.length + LEVEL_FILE_FORMAT.length] = '\0';

    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        console_log("Couldn't open the level file for building '%s'.\n", file_name);
        return;
    }

    u64 written = 0; 
    
    written += fwrite_u32(LEVEL_FORMAT_HEADER, file) * 4;

    // Serializing each edge information.
    written += fwrite_u32(array_list_length(&edges_list), file) * 4;
    

    // @Important: The reason why there are two loops, is only because the first one tells the length of the polygon being serialized and the other serializes the edges themselves.
    Vec2f normal, v0, v1;
    u32 j, polygon_edge_count;
    for (u32 i = 0; i < array_list_length(&edges_list); i++) {  
        if (edges_list[i].flags & EDITOR_EDGE_BUILT) {
            continue;
        }


        j = i;
        polygon_edge_count = 0;
        while (true) {
            polygon_edge_count++;

            if (edges_list[j].next_index == EDITOR_INVALID_INDEX) {
                console_log("Couldn't finish polygon building, disconnected edge sequence encountered.\n");
                break;
            }

            j = edges_list[j].next_index;

            if (j == i) {
                break;
            }
        }

        written += fwrite_u32(polygon_edge_count, file) * 4;
        j = i;
        while(true) {
            written += fwrite_float(edges_list[j].vertex.x, file) * 4;
            written += fwrite_float(edges_list[j].vertex.y, file) * 4;

            if (edges_list[j].next_index == EDITOR_INVALID_INDEX) {
                written += fwrite_float(0.0f, file) * 4;
                written += fwrite_float(0.0f, file) * 4;
                break;
            }

            v0 = edges_list[j].vertex;
            v1 = edges_list[edges_list[j].next_index].vertex;
            normal = vec2f_normalize(vec2f_make( (-2 * edges_list[j].flipped_normal + 1) * (v1.y - v0.y), (2 * edges_list[j].flipped_normal - 1) * (v1.x - v0.x) ));

            written += fwrite_float(normal.x, file) * 4;
            written += fwrite_float(normal.y, file) * 4;
            
            edges_list[j].flags |= EDITOR_EDGE_BUILT;

            j = edges_list[j].next_index;

            if (j == i) {
                break;
            }
        }
    }

    // Buildin each entity information.
    written += fwrite_u32(array_list_length(&entity_list), file) * 4;
    
    for (u32 i = 0; i < array_list_length(&entity_list); i++) {
        written += fwrite(&entity_list[i].type, 1, 1, file);
        written += fwrite_float(entity_list[i].bound_box.center.x, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.center.y, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.dimensions.x, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.dimensions.y, file) * 4;
        written += fwrite_float(entity_list[i].bound_box.rot, file) * 4;
    }


    fclose(file);


    for (u32 i = 0; i < array_list_length(&edges_list); i++) {  
        edges_list[i].flags &= ~EDITOR_EDGE_BUILT;
    }

    console_log("Written %llu bytes to level file '%s'.\n", written, file_name);
}



void editor_add_quad(Vec2f position) {
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

void editor_add_entity(Vec2f position, Entity_Type type) {
    switch (type) {
        case PLAYER:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = PLAYER,
                        .bound_box = obb_make(position, 0.8f, 1.4f, 0.0f),
                        }));
            break;
        case PROP_PHYSICS:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = PROP_PHYSICS,
                        .bound_box = obb_make(position, 1.0f, 1.0f, 0.0f),
                        }));
            break;
        case RAY_EMITTER:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = RAY_EMITTER,
                        .bound_box = obb_make(position, 1.0f, 1.0f, 0.0f),
                        }));
            break;
        case RAY_HARVESTER:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = RAY_HARVESTER,
                        .bound_box = obb_make(position, 1.0f, 1.0f, 0.0f),
                        }));
            break;
        case MIRROR:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = MIRROR,
                        .bound_box = obb_make(position, 0.4f, 3.0f, 0.0f),
                        }));
            break;
        case GLASS:
            array_list_append(&entity_list, ((Editor_Entity) {
                        .type = GLASS,
                        .bound_box = obb_make(position, 1.0f, 4.0f, 0.0f),
                        }));
            break;
    }
}
