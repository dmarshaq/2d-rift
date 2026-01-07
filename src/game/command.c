#include "game/command.h"

#include "meta_generated.h"

#include "game/console.h"

#include "core/str.h"
#include "core/typeinfo.h"
#include "core/structs.h"
#include "core/arena.h"



static Command *command_list;

static Arena command_args_arena;


#define ARENA_CAPACITY 4096


void command_init() {
    command_list = array_list_make(Command, 8, &std_allocator);
    command_args_arena = arena_make(ARENA_CAPACITY);

    register_all_commands();
}

void command_register(Type_Info *type, void (*ptr)(Any *, u32)) {
    // Checking if type is a FUNCTION.
    if (type->type != FUNCTION) {
        printf_err("Couldn't register command, '%.*s' type is not a FUNCTION.\n", UNPACK(type->name));
        return;
    }

    array_list_append(&command_list, ((Command) {
                .type = type,
                .func = ptr,
                }));
}

/**
 * @Speed: When list of commands grows it will be benificial to store them in a hash table.
 */
void command_run(String command) {
    String remainder = command;

    remainder = str_eat_spaces(remainder);
    if (remainder.length == 0) {
        console_log("Command name is not specified.\n");
        return;
    }

    
    String command_name = str_get_until_space(remainder);
    remainder.data += command_name.length;
    remainder.length -= command_name.length;


    for (s64 i = 0; i < array_list_length(&command_list); i++) {
        if (str_equals(command_name, command_list[i].type->name)) {
            // @Temporary: Since in C there are no *default* params, variables 'min_args' and 'max_args' will be just set to the total count of the arguments.
            u32 min_args = command_list[i].type->t_function.arguments_length;
            u32 max_args = command_list[i].type->t_function.arguments_length;
            

            // Scanning for arguments.
            Any parsed_args[max_args + 1]; // This gurantees that parsed args has space for at least one element, which is always used for the return value.
            u32 args_count = 0;
            String arg;

            while (true) {
                remainder = str_eat_spaces(remainder);
                if (remainder.length == 0) {
                    break;
                }

                args_count++;
                if (args_count > max_args) {
                    if (max_args == 1)
                        console_log("%.*s: Expected no more than '%d' argument.\n", UNPACK(command_name), max_args);
                    else
                        console_log("%.*s: Expected no more than '%d' arguments.\n", UNPACK(command_name), max_args);
                    return;
                }

                // Getting arg str and moving remainder to the index after.
                if (remainder.data[0] == '"') {
                    remainder = str_eat_chars(remainder, 1);

                    s64 end = str_find_char_left(remainder, '"');
                    if (end < 0) {
                        console_log("%.*s: Argument [%d]: Expected closing \".\n", UNPACK(command_name), args_count - 1);
                        return;
                    }

                    arg = str_substring(remainder, 0, end);
                    remainder = str_eat_chars(remainder, arg.length + 1);
                } else {
                    arg = str_get_until_space(remainder);
                    remainder = str_eat_chars(remainder, arg.length);
                }

                // Parsing arg string.
                Type_Info *expected_argument_type = get_base_of_typedef(command_list[i].type->t_function.arguments[args_count - 1].type);
                void *data = arena_alloc(&command_args_arena, expected_argument_type->size);

                switch(expected_argument_type->type) {
                    case INTEGER:
                        if (!str_is_int(arg)) {
                            console_log("%.*s: Argument [%d]: '%.*s' type mismatch, expected 'Integer'.\n", UNPACK(command_name), args_count - 1, UNPACK(arg));
                            return;
                        }

                        s64 value = str_parse_int(arg);
                        mem_copy_int(data, &value, expected_argument_type->size, 8, 0);

                        break;
                    case FLOAT:
                        if (!str_is_float(arg)) {
                            console_log("%.*s: Argument [%d]: '%.*s' type mismatch, expected 'Float'.\n", UNPACK(command_name), args_count - 1, UNPACK(arg));
                            return;
                        }

                        *(float *)(data) = str_parse_float(arg);

                        break;
                    case ENUM:
                        // Sending enum value based on integer supplied
                        if (str_is_int(arg)) {
                            s64 value = str_parse_int(arg);
                            mem_copy_int(data, &value, expected_argument_type->size, 8, 0);
                            goto enum_arg_parsing_break;
                        }

                        // Sending enum value based on actual enum member name supplied.
                        for (u32 i = 0; i < expected_argument_type->t_enum.members_length; i++) {
                            if (str_equals(expected_argument_type->t_enum.members[i].name, arg)) {
                                mem_copy_int(data, &expected_argument_type->t_enum.members[i].value, expected_argument_type->size, 8, 0);
                                goto enum_arg_parsing_break;
                            }
                        }

                        console_log("%.*s: Argument [%d]: '%.*s' type mismatch, expected enum value of type '%.*s'.\n", UNPACK(command_name), args_count - 1, UNPACK(arg), UNPACK(expected_argument_type->name));
                        return;

enum_arg_parsing_break:
                        break;
                    case STRING:
                        *(String *)(data) = arg;
                        break;
                    default:
                        TODO("Other arguments types handling.");
                        break;
                }


                parsed_args[args_count - 1] = ((Any) {
                        .type = expected_argument_type,
                        .data = data,
                        });

            }

            if (args_count < min_args) {
                if (min_args == 1)
                    console_log("%.*s: Expected at least '%d' argument.\n", UNPACK(command_name), min_args);
                else
                    console_log("%.*s: Expected at least '%d' arguments.\n", UNPACK(command_name), min_args);
                return;
            }

            // @Incomplete: Adding default args if not all args were filled.
            for (u32 j = args_count; j < max_args; j++) {
                TODO("Adding default args.");
            }

            
            // Making sure that allocated size in arena is big enough for the return value.
            Type_Info *expected_return_type = get_base_of_typedef(command_list[i].type->t_function.return_type);
            if (arena_size(&command_args_arena) < expected_return_type->size) {
                arena_alloc(&command_args_arena, expected_return_type->size - arena_size(&command_args_arena));
            }

            // Calling wrapper function.
            command_list[i].func(parsed_args, max_args);
    

            // Getting return value if its not void.
            if (expected_return_type->type != VOID) {
                // Return value is always placed into 0th index of parsed_args buffer.
                // Printing it to the console.
                Any r_value = parsed_args[0];

                String buffer = STR(128, (char[128]){});

                buffer = format_any(r_value, buffer);

                console_log("%.*s\n", UNPACK(buffer));
            }



            arena_clear(&command_args_arena);
            return;
        }
    }

    console_log("%.*s: Is not a command.\n", UNPACK(command_name));
}


void command_print(Command *command) {
    console_log("\n\nName: %-10.*s    Args count: %2d\n", UNPACK(command->type->name), command->type->t_function.arguments_length);
    
    console_log("----------------------------------------------------------------\n");
    for (u32 i = 0; i < command->type->t_function.arguments_length; i++) {
        console_log("    Argument [%d]:   ", i);
        print_type_info(command->type->t_function.arguments[i].type);
    }
}




