#include "game/console.h"

#include "meta_generated.h"

#include "game/game.h"
#include "game/draw.h"
#include "game/graphics.h"
#include "game/command.h"
#include "game/vars.h"

#include "core/structs.h"
#include "core/arena.h"
#include "core/str.h"
#include "core/mathf.h"
#include "core/file.h"
#include "core/typeinfo.h"

#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <stdio.h>



static Font_Baked font_input;
static Font_Baked font_output;

static Matrix4f projection;


@Introspect;
typedef struct console {
    s64 speed;
    float open_percent;
    float full_open_percent;
    s64 text_pad;
} Console;

static Console console;



static const s64 HISTORY_BUFFER_SIZE = 8192;
static const s64 HISTORY_MAX_MESSAGES = 256;

#define HISTORY_MAX_BUFFERS   2
#define INPUT_BUFFER_SIZE     100

typedef enum history_message_type : u8 {
    MESSAGE_USER        = 0,
    MESSAGE_LOG         = 1,
    MESSAGE_ERROR       = 2,
} History_Message_Type;

static const Vec4f HISTORY_MESSAGE_COLORS[] = {
    ((Vec4f) { 0.5f, 0.8f, 0.3f, 1.0f }),
    ((Vec4f) { 0.8f, 0.8f, 0.8f, 1.0f }),
    ((Vec4f) { 0.8f, 0.4f, 0.4f, 1.0f }),
};

typedef struct history_message {
    History_Message_Type type;
    String str;
} History_Message;

static History_Message *history;
static s64 history_index;
static float history_font_top_pad;
static float history_block_width;

static s64 display_line_offset; // It is amount of line that should be skipped before rendering first (most bottom line) in the console.
                                // So in case of scrolling console up this value would correspond to the amount of lines scrolled up.




/**
 * This structs sole purpose is to point to the specific user input relatively to the user input history array.
 * String cannot be used since they hold absolute address of the data not the realtive one to the beginning of the array.
 */
typedef struct user_input_handle {
    s64 length;
    s64 index;
} User_Input_Handle;

static User_Input_Handle *user_input_history;
static s64 user_input_peeked_message_index;


static char input[INPUT_BUFFER_SIZE];
static float input_height;
static float input_font_top_pad;
static float input_block_width;

static s64 input_length;
static s64 input_cursor_index;

static T_Interpolator input_cursor_blink_timer;
static bool input_cursor_visible;
static bool input_cursor_moved;
static float input_cursor_activity;

static float c_y0;
static float c_y0_target;

static float c_x0;
static float c_x1;



static char *history_active_buffer;
static s64 history_active_buffer_index;
static s64 history_buffer_write_index;
static char *history_buffers[HISTORY_MAX_BUFFERS];

static char *user_input_history_buffer;


typedef enum console_openness {
    CLOSED,
    OPEN,
    FULLY_OPEN,
} Console_Openness;

static Console_Openness console_state;




bool console_active() {
    return console_state != CLOSED;
}






float console_max_height(Window_Info *window){
    return window->height * console.full_open_percent; 
}

void console_start_input_if_not(Text_Input *text_i) {
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}

void console_stop_input(Text_Input *text_i) {
    SDL_StopTextInput();
}

void history_peek_up_user_message() {
    // Case: index is -1.
    if (user_input_peeked_message_index == -1) {
        user_input_peeked_message_index = (s64)array_list_length(&user_input_history) - 1; // @Important: s64 cast is needed since array list length return u32.
        
        if (user_input_peeked_message_index == -1) return; // This is a rare case when the 'user_input_peeked_message_index' is -1 because length of user input history is 0, in this case we don't want to move the cursor so we just early return.

        goto move_cursor_return;
    }
    

    // Default case: index is not -1.
    user_input_peeked_message_index--;
    user_input_peeked_message_index = maxi(0, user_input_peeked_message_index); // Making sure user_input_peeked_message_index is clamped to 0.

move_cursor_return:
    input_cursor_index = user_input_history[user_input_peeked_message_index].length - 1; // Cutting off '\n', because input field shouldn't have any newlines.  
}

void history_peek_down_user_message() {
    // Case: index is -1
    if (user_input_peeked_message_index == -1) return;

    // Default case: index is not -1.
    user_input_peeked_message_index++;

    // If to avoid out of bounds problem and jump to the -1 index (input field message).
    if (user_input_peeked_message_index == (s64)array_list_length(&user_input_history)) {
        user_input_peeked_message_index = -1;
        input_cursor_index = input_length;
        return;
    }

    input_cursor_index = user_input_history[user_input_peeked_message_index].length - 1; // Cutting off '\n', because input field shouldn't have any newlines.
}


void history_return_peeked_message() {
    if (user_input_peeked_message_index != -1) {
        User_Input_Handle handle = user_input_history[user_input_peeked_message_index];
        
        input_length = handle.length - 1; // handle.length - 1 because last char is '\n' so we skip it.
        memcpy(input, &user_input_history_buffer[handle.index], input_length);

        user_input_peeked_message_index = -1;
    }
}





/**
 * Console printing.
 */
void console_add(char *buffer, s64 length, History_Message_Type type) {
    // Copy actual string data.
    if (history_buffer_write_index + length > HISTORY_BUFFER_SIZE) {
        if (length > HISTORY_BUFFER_SIZE) {
            printf_err("Message written to the console buffer is too long, not enough memory space to store it.\n");
            return;
        }

        // Swapping buffers.
        // @Important: Right now it doesn't clear swapped buffer and doesn't track any pointers that migth have been left in the buffer, but it is assumed that buffers are simply big, so that by the time it tracks back to the used buffer all string pointers that pointed to the that memory would be gone.
        history_active_buffer_index = (history_active_buffer_index + 1) % HISTORY_MAX_BUFFERS;
        history_buffer_write_index = 0;
    }

    char *ptr = history_buffers[history_active_buffer_index] + history_buffer_write_index;
    history_buffer_write_index += length;
    memcpy(ptr, buffer, length);

    // Saving user input into user input history.
    if (type == MESSAGE_USER) {
        array_list_append(&user_input_history, ((User_Input_Handle){ .length = length, .index = array_list_length(&user_input_history_buffer) }));
        array_list_append_multiple(&user_input_history_buffer, buffer, length);
    }


    // Following code will determine to either create a new String if previous was ended with '\n' or append to previous string if not. It is done to properly display messages if previous one wasn't ended with '\n'.
    bool concat_with_last_message = false;
    History_Message *last_message = NULL;
    u32 history_length = looped_array_length(&history);
    if (history_length > 0) {
        last_message = history + _looped_array_map_index((void *)history, history_length - 1);
        concat_with_last_message = last_message->str.data[last_message->str.length - 1] != '\n';
    }


    if (concat_with_last_message) {
        last_message->str.length += length;
    } else {
        History_Message msg = (History_Message){
                    .type = type,
                    .str = STR(length, ptr),
                    };
        looped_array_append(&history, msg);
    }
}

void cprintf_va(History_Message_Type type, char *format, va_list args) {
    // Make a copy of args
    va_list args_copy;
    va_copy(args_copy, args);
    s64 length = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (length < 0) {
        printf_err("Failed to find formatted string length for console output.\n");
        return;
    }

    char buffer[length + 1];
    vsnprintf(buffer, length + 1, format, args);

    console_add(buffer, length, type);
}

void cprintf(History_Message_Type type, char *format, ...) {
    va_list args;
    va_start(args, format);
    cprintf_va(type, format, args);
    va_end(args);
}

void console_log(char *format, ...) {
    va_list args;
    va_start(args, format);
    cprintf_va(MESSAGE_LOG, format, args);
    va_end(args);
}


void console_error(char *format, ...) {
    va_list args;
    va_start(args, format);
    cprintf_va(MESSAGE_ERROR, format, args);
    va_end(args);
}






// Pointers to global state.
static Quad_Drawer *quad_drawer_ptr;
static Events_Info *events_ptr;
static Window_Info *window_ptr;
static Time_Info   *time_ptr;

void console_init(State *state) {
    // Tweak vars default values.
    console.speed               = 100;
    console.open_percent        = 0.4f;
    console.full_open_percent   = 0.8f;
    console.text_pad            = 10;
    
    vars_tree_add(TYPE_OF(console), (u8 *)&console, CSTR("console"));

    // Setting pointers to global state.
    quad_drawer_ptr    = &state->quad_drawer;
    window_ptr         = &state->window;
    events_ptr         = &state->events;
    time_ptr           = &state->t;



    // ------ Internal vars.

    // Get resources.
    quad_drawer_ptr = &state->quad_drawer;

    // Load needed font... Hard coded...
    u8* font_data = read_file_into_buffer("res/font/Consolas-Regular.ttf", NULL, &std_allocator);

    font_input = font_bake(font_data, 18.0f);
    font_output = font_bake(font_data, 16.0f);

    free(font_data);

    // @Important: For metrics we assume that fonts are monospaced!
    // Set input metrics.
    input_font_top_pad = font_input.line_height * 0.4f;
    input_height = font_input.line_height + input_font_top_pad;
    input_block_width = font_input.chars[(s32)' ' - font_input.first_char_code].xadvance;

    // Set history height.
    history_font_top_pad = font_output.line_height * 0.2f;
    history_block_width = font_output.chars[(s32)' ' - font_input.first_char_code].xadvance;

    // Important not styling, logic vars.
    history = looped_array_make(History_Message, HISTORY_MAX_MESSAGES, &std_allocator);
    display_line_offset = 0;


    user_input_history = array_list_make(User_Input_Handle, 8, &std_allocator);
    user_input_peeked_message_index = -1;

    // Input.
    input_length = 0; 
    input_cursor_index = 0;



    // Cursor.
    input_cursor_blink_timer = ti_make(800);
    input_cursor_visible = true;
    input_cursor_moved = false;
    input_cursor_activity = 0.0f;

    // Console positions and state.
    c_y0 = state->window.height;
    console_state = CLOSED;


    // Initing all history buffers. The idea is to swap buffer once it has been filled for the next one.
    history_active_buffer_index = 0;
    history_active_buffer = allocator_alloc(&std_allocator, HISTORY_BUFFER_SIZE * HISTORY_MAX_BUFFERS);
    for (s64 i = 0; i < HISTORY_MAX_BUFFERS; i++) {
        history_buffers[i] = history_active_buffer + i * HISTORY_BUFFER_SIZE;
    }
    history_buffer_write_index = 0;

    user_input_history_buffer = array_list_make(char, HISTORY_BUFFER_SIZE, &std_allocator);
}

void console_update() {
    // Reset state.
    input_cursor_moved = false;

    c_x0 = 0.1f * window_ptr->width;
    c_x1 = 0.9f * window_ptr->width;

    // Checking for input to change console's state.
    if (pressed(SDLK_F11)) {
        if (hold(SDLK_LSHIFT)) {
            if (console_state == FULLY_OPEN) {
                console_state = CLOSED;
                console_stop_input(&events_ptr->text_input);
            }
            else {
                console_state = FULLY_OPEN;
                console_start_input_if_not(&events_ptr->text_input);
            }
        } else {
            if (console_state == OPEN) {
                console_state = CLOSED;
                console_stop_input(&events_ptr->text_input);
            }
            else {
                console_state = OPEN;
                console_start_input_if_not(&events_ptr->text_input);
            }
        }
    }

    // Lerping console if it's y0 doesn't match it's state (target y0).
    switch(console_state) {
        case CLOSED:
            c_y0_target = window_ptr->height;
            break;
        case OPEN:
            c_y0_target = window_ptr->height * (1.0f - console.open_percent);
            break;
        case FULLY_OPEN:
            c_y0_target = window_ptr->height * (1.0f - console.full_open_percent);
            break;
        default:
            printf_err("Unknown console state.\n");
            break;
    }

    c_y0 = lerp(c_y0, c_y0_target, 0.01f * time_ptr->delta_time_milliseconds);
    

    // Checking for input if active.
    if (SDL_IsTextInputActive()) {
        
        if (repeat(SDLK_UP, time_ptr->delta_time_milliseconds)) {
            if (hold(SDLK_LSHIFT))
                display_line_offset++;
            else
                history_peek_up_user_message();
        }

        if (repeat(SDLK_DOWN, time_ptr->delta_time_milliseconds)) {
            if (hold(SDLK_LSHIFT))
                display_line_offset--;
            else
                history_peek_down_user_message();
        }

        if (display_line_offset < 0) 
            display_line_offset = 0;

        if (repeat(SDLK_LEFT, time_ptr->delta_time_milliseconds)) {
            // Stop history command walk when cursor moving.
            history_return_peeked_message();
            
            if (input_cursor_index > 0)
                input_cursor_index--;

            input_cursor_moved = true;
        }

        if (repeat(SDLK_RIGHT, time_ptr->delta_time_milliseconds)) {
            // Stop history command walk when cursor moving.
            history_return_peeked_message();
            
            if (input_cursor_index < input_length)
                input_cursor_index++;

            input_cursor_moved = true;
        }


        if (events_ptr->text_input.text_inputted) {
            // Stop history command walk when text editing.
            history_return_peeked_message();

            s64 written = insert_input_text(input, INPUT_BUFFER_SIZE, input_length, input_cursor_index,&events_ptr->text_input);

            input_length += written;
            input_cursor_index += written;

            input_cursor_moved = true;
        }



        if (repeat(SDLK_BACKSPACE, time_ptr->delta_time_milliseconds)) {
            // Stop history command walk when text editing.
            history_return_peeked_message();

            if (input_cursor_index > 0) {
                // Shifting string if deleted in the middle of the text.
                if (input_cursor_index < input_length) {

                    // Shift the string to the left by one.
                    for (s64 i = input_cursor_index - 1; i < input_length; i++) {
                        input[i] = input[i + 1];
                    }
                }

                input_length--;
                input_cursor_index--;
            }

            input_cursor_moved = true;
        }




        if (pressed(SDLK_RETURN)) {
            // Stop history command walk when text editing.
            history_return_peeked_message();

            // Add a new line symbol and flush input if return key was pressed.
            cprintf(MESSAGE_USER, "%.*s\n", input_length, input);
            command_run(STR(input_length, input));

            // Clearing text input buffer.
            input[0] = '\0';
            input_length = 0;
            input_cursor_index = 0;

            // Moving display of the console to the very bottom.
            display_line_offset = 0;
        }
        
        // @Obosolete?
        //  input_cursor_index = events_ptr->text_input.write_index;
    }

    // Cursor styling.
    ti_update(&input_cursor_blink_timer, time_ptr->delta_time_milliseconds);
    
    if (input_cursor_moved) {
        ti_reset(&input_cursor_blink_timer);
        input_cursor_visible = true;
        input_cursor_activity += 0.03f;
    } else {
        input_cursor_activity -= 0.15f * time_ptr->delta_time;
    }
    input_cursor_activity = clamp(input_cursor_activity, 0.0f, 0.25f);


    if (ti_is_complete(&input_cursor_blink_timer)) {
        ti_reset(&input_cursor_blink_timer);
        input_cursor_visible = !input_cursor_visible;
    }

}

void console_draw() {
    projection = screen_calculate_projection(window_ptr->width, window_ptr->height);
    shader_update_projection(quad_drawer_ptr->program, &projection);
    

    draw_begin(quad_drawer_ptr);

    // Output.
    draw_rect(vec2f_make(c_x0, c_y0 + input_height), vec2f_make(c_x1, c_y0 + console_max_height(window_ptr)), .color = vec4f_make(0.10f, 0.12f, 0.24f, 0.98f));

    // Input.
    draw_rect(vec2f_make(c_x0, c_y0), vec2f_make(c_x1, c_y0 + input_height), .color = vec4f_make(0.18f, 0.18f, 0.35f, 0.98f));
    
    // Draw text of the history.
    Vec2f history_draw_origin = vec2f_make(c_x0 + console.text_pad, c_y0 + input_height);
    s64 lines_drawen = 0;
    History_Message *msg;
    s64 msg_line_count;

    u32 history_length = looped_array_length(&history);
    for (s64 i = 1; i <= history_length; i++) {
        msg = history + _looped_array_map_index((void *)history, history_length - i); // Calling to internal function cause it is just faster to get pointer this way.
        msg_line_count = str_count_chars(msg->str, '\n'); // Later when wrapping of text will be made in the console, the counting of lines will change.

        lines_drawen += msg_line_count;
        if (lines_drawen <= display_line_offset) {
            continue;
        }

        // Cutting off lines that are in the same message but are supposed to be offseted.
        String msg_str = msg->str;
        s64 msg_cut_lines = msg_line_count - mini(msg_line_count, lines_drawen - display_line_offset);
        for (s64 j = -1; j < msg_cut_lines; j++) { // @Important: -1 is here because this for loop is given extra turn to remove the very last '\n' at the end of every message.
            msg_str = str_substring(msg_str, 0, str_find_char_right(msg_str, '\n'));
        }


        history_draw_origin.y += (msg_line_count - msg_cut_lines) * font_output.line_height; 
        // history_draw_origin.y += history_font_top_pad;

        draw_text(msg_str, history_draw_origin, &font_output, .color = HISTORY_MESSAGE_COLORS[msg->type]);
    }
    
    display_line_offset = mini(display_line_offset, lines_drawen);
    
    // Draw input cursor.
    if (input_cursor_visible) {
        Vec4f color = vec4f_lerp(vec4f_make(0.58f, 0.58f, 0.85f, 0.90f), VEC4F_YELLOW, input_cursor_activity);
        
        // Choosing cursor width.
        float width = input_block_width;
        if (input_cursor_index < input_length) {
            width = 2;
        }

        draw_rect(vec2f_make(c_x0 + console.text_pad + input_cursor_index * input_block_width, c_y0 + font_input.line_gap), vec2f_make(c_x0 + console.text_pad + input_cursor_index * input_block_width + width, c_y0 + input_height - input_font_top_pad * 0.5f), .color = color);
    }


    // Draw input text.
    if (user_input_peeked_message_index != -1) {
        User_Input_Handle handle = user_input_history[user_input_peeked_message_index];
        draw_text(STR(handle.length, &user_input_history_buffer[handle.index]), vec2f_make(c_x0 + console.text_pad, c_y0 + input_height - input_font_top_pad), &font_input, .color = VEC4F_YELLOW);
    } else {
        draw_text(STR(input_length, input), vec2f_make(c_x0 + console.text_pad, c_y0 + input_height - input_font_top_pad), &font_input, .color = VEC4F_CYAN);
    }

    draw_end();
}



void console_free() {
    allocator_free(&std_allocator, history_buffers[0]);
}











// Example function that we want to register (doesn't have to be in this file).
s64 add(s32 a, s32 b) {
    return a + b;
}

void clear() {
    looped_array_clear(&history);
}






