#include <stdio.h>

#include "core/type.h"
#include "core/typeinfo.h"
#include "core/str.h"
#include "core/arena.h"
#include "core/file.h"
#include "core/structs.h"

#include "meta/lexer.h"
#include "meta/meta.h"



const char* debug_meta_str = "\033[34m[META]\033[0m";
#define meta_log(format, ...)                 (void)fprintf(stderr, "%s " format, debug_meta_str, ##__VA_ARGS__);







// Type table / introspcetion vars.
static const s64 POINTER_SIZE = 8;

static const String TYPE_PTR_POSTFIX = STR_BUFFER("_ptr");

static const String TYPEDEF_STR    = STR_BUFFER("typedef");
static const String STRUCT_STR     = STR_BUFFER("struct");
static const String UNION_STR      = STR_BUFFER("union");
static const String ENUM_STR       = STR_BUFFER("enum");


static Arena arena_strings; //@Leak.
static Arena arena_type_info; // @Leak.
static Arena arena_type_info_function_argument; // @Leak.
static Arena arena_type_info_struct_member; // @Leak.
static Arena arena_type_info_enum_member; // @Leak.

static Type_Info **type_table; // @Leak.



// Maybe move current state of meta processing to a separate struct that might contain all of the info like current_file_name, etc.
static char *current_file_name = NULL;




// RegisterCommand vars.
static Type_Info **registered_functions; // @Leak.
static String *registered_functions_headers; // @Leak.


// @Important: We take responsability, of preserving all important strings even after contents of the file are disposed, it means all important typenames are copied into 'arena_strings'
#define SAVE_STRING(str) (str).data = str_copy_to((str), arena_alloc(&arena_strings, (str).length))



String type_table_get_typename(Type_Info *type) {
    /**
     * For right now meta programm outputs hash_table that consists of actual types that main programm will see, it is a disadvantage since somehow this means
     * the programm should be able to translate pointers to the main programm as well.
     * The better solution would be to not be consistent with main programm type info data and while parsing have separate meta type info's, that contain
     * more relevant to process data.
     * But for now it is not a case, so the solution is to force through the pointers until addresses much and get the key from that element in the hash table.
     */
    for (u32 j = 0; j < hash_table_capacity(&type_table); j++) {
        Hash_Table_Slot *_slot = _hash_table_get_slot((void **)&type_table, j);
        if (_slot->state == SLOT_OCCUPIED && type_table[j] == type) {
            return _slot->key;
        }
    }
    return (String) {0};
}


bool type_table_defined(String typename) {
    void *reference = hash_table_get(&type_table, UNPACK(typename));

    if (reference == NULL) {
        return false;
    }

    Type_Info *type = *(Type_Info **)(reference);

    if (type->type == UNKNOWN) {
        return false;
    }

    return true;
}


bool type_table_exists(String typename) {
    return hash_table_get(&type_table, UNPACK(typename)) != NULL;
}



/**
 * Type_Info of 'base_typename' should exist.
 * Returns enum ready typename of the pointer.
 */
String type_table_add_pointer(String base_typename, s64 asterisk_count) {

    // Making typename be enum ready, substituting '*' with '_ptr'.
    // Buffer will contain both actual typename and enum ready typename:
    // For example: "char***" and "char_ptr_ptr_ptr"     *sigh* . . . 
    // Yes this is stupid but it works well.
    char *buffer = arena_alloc(&arena_strings,  (base_typename.length + asterisk_count)+ (base_typename.length + asterisk_count * TYPE_PTR_POSTFIX.length));

    char *ptr_typename      = str_copy_to(base_typename, buffer);
    char *ptr_enum_typename = str_copy_to(base_typename, buffer + base_typename.length + asterisk_count);

    String typename         = STR(base_typename.length, ptr_typename);
    String enum_typename    = STR(base_typename.length, ptr_enum_typename);

    for (s64 i = 0; i < asterisk_count; i++) {
        Type_Info *base_type = *(Type_Info **)(hash_table_get(&type_table, UNPACK(enum_typename)));
        ptr_typename[base_typename.length + i] = '*';
        str_copy_to(TYPE_PTR_POSTFIX, ptr_enum_typename + base_typename.length + i * TYPE_PTR_POSTFIX.length);
        
        typename      = STR(base_typename.length + (i + 1), ptr_typename);
        enum_typename = STR(base_typename.length + (i + 1) * TYPE_PTR_POSTFIX.length, ptr_enum_typename);



        printf(" [DEB] PTR ADDING typename: '%.*s', with enum_typename: '%.*s'\n", UNPACK(typename), UNPACK(enum_typename));


        // Type check, if already exists, since pointer types are unique because the base type is unique, it's not the reason to error.
        if (type_table_exists(enum_typename)) {
            continue;
        }

        // Putting pointer.
        Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
        *item = ((Type_Info) { POINTER, typename, POINTER_SIZE, POINTER_SIZE, .t_pointer = { base_type } });
        hash_table_put(&type_table, item, UNPACK(enum_typename));
    }


    return enum_typename;
    // Filling in actual typename.
    // Example: " char*** "
    // str_copy_to(base_typename, buffer);
    // for (s64 i = 0; i < asterisk_count; i++) {
    //     buffer[base_typename.length + i] = '*';
    // }







    // String typename = STR(base_typename.length + asterisk_count * TYPE_PTR_POSTFIX.length, buffer);


    // return typename;
}

/**
 * Type_Info of 'typename' should exist.
 */
int type_table_add_typedef(String typename, String typedef_typename) {
    Type_Info *typedef_of = *(Type_Info **)(hash_table_get(&type_table, UNPACK(typename)));

    // Type check, if already defined.
    if (type_table_defined(typedef_typename)) {
        printf_err("Type Table: Couldn't add '%.*s' typedef, it already exists.", typedef_typename);
        return 1;
    }

    // Putting typedef.
    Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { TYPEDEF, typedef_typename, 0, 0, .t_typedef = { typedef_of } });
    hash_table_put(&type_table, item, UNPACK(typedef_typename) );

    return 0;
}

void type_table_add_unknown(String typename) {
    Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { UNKNOWN, typename, 0, 0 });

    hash_table_put(&type_table, item, UNPACK(typename));
}


Type_Info* type_table_add_struct(String typename) {
    Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { STRUCT, typename, 0, 0, .t_struct = { 0, arena_type_info_struct_member.ptr } });
    hash_table_put(&type_table, item, UNPACK(typename));
    return item;
}

void type_table_add_struct_member(Type_Info *struct_type, String member_typename, String member_name) {
    Type_Info *member_type = *(Type_Info **)(hash_table_get(&type_table, UNPACK(member_typename)));

    Type_Info_Struct_Member *member = arena_alloc(&arena_type_info_struct_member, sizeof(Type_Info_Struct_Member));

    *member = ((Type_Info_Struct_Member) { member_type, member_name, 0 });

    struct_type->t_struct.members_length++;
}




Type_Info* type_table_add_enum(String typename) {
    Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { ENUM, typename, 4, 4, .t_enum = { true, 0, arena_type_info_enum_member.ptr } });
    hash_table_put(&type_table, item, UNPACK(typename));
    return item;
}

void type_table_add_enum_member(Type_Info *enum_type, String member_name, u64 value) {
    
    Type_Info_Enum_Member *member = arena_alloc(&arena_type_info_enum_member, sizeof(Type_Info_Enum_Member));

    *member = ((Type_Info_Enum_Member) { member_name, value });

    enum_type->t_enum.members_length++;
}




Type_Info* type_table_add_function(Type_Info *return_type, String typename) {
    // @Perfomance: Maybe move this on a higher level to reduce total amount of the strings saved, and simply save one string per file processed.
    String definition_file = CSTR(current_file_name);

    // Remove 'src/' from the beginning.
    definition_file = str_eat_chars(definition_file, 4);
    
    // @Temporary, maybe move such cutting functionality to the 'str.h'.
    // Remove '_m' from the path name, and save string.
    s64 index_of__m = str_find(definition_file, STR_BUFFER("_m"));

    if (index_of__m != -1) {
        char *file_path = arena_alloc(&arena_strings, definition_file.length - 2);
        memcpy(file_path, definition_file.data, index_of__m);
        memcpy(file_path + index_of__m, definition_file.data + index_of__m + 2, definition_file.length - 2 - index_of__m);
        definition_file.data = file_path;
        definition_file.length = definition_file.length - 2;
    }
    else {
        SAVE_STRING(definition_file);
    }


    Type_Info *item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { FUNCTION, typename, 0, 0, .t_function = { return_type, 0, arena_type_info_function_argument.ptr, definition_file } });
    hash_table_put(&type_table, item, UNPACK(typename));
    return item;
}

void type_table_add_function_argument(Type_Info *function_type, String arg_typename, String arg_name) {
    Type_Info *arg_type = *(Type_Info **)(hash_table_get(&type_table, UNPACK(arg_typename)));

    Type_Info_Function_Argument *arg = arena_alloc(&arena_type_info_function_argument, sizeof(Type_Info_Function_Argument));

    *arg = ((Type_Info_Function_Argument) { arg_type, arg_name });

    function_type->t_function.arguments_length++;
}





/**
 * @Recursion.
 */
int type_table_calculate_size_of(Type_Info *type) {
    if (type->type == UNKNOWN) {
        printf_err("Couldn't calculate size of the type, type is UNKNOWN.\n");
        return -1;
    }

    if (type->type == TYPEDEF) {
        if (type_table_calculate_size_of(type->t_typedef.typedef_of) != 0) 
            return -1;

        type->size = type->t_typedef.typedef_of->size;
        type->align = type->t_typedef.typedef_of->align;
        return 0;
    }


    if (type->type == STRUCT) {
        Type_Info_Struct_Member *members = type->t_struct.members;

        u32 offset = 0;
        u32 max_align = 0;
        for (u32 i = 0; i < type->t_struct.members_length; i++) {
            if (type_table_calculate_size_of(members[i].type) != 0) 
                return -1;
            
            // Edge case: 0 aligned member.
            if (members[i].type->align == 0)
                continue;

            // Setting offset to be next alligned offset;
            offset = (offset + members[i].type->align - 1) / members[i].type->align * members[i].type->align;
            members[i].offset = offset;
            offset += members[i].type->size;

            // Comparing with max align.
            if (max_align < members[i].type->align) {
                max_align = members[i].type->align;
            }
        }

        type->size = offset;
        type->align = max_align;
        return 0;
    }

    return 0;
}


/**
 * Recursivly calculate sizes.
 * Ignore size calculations for non 0 sizes.
 */
int type_table_calculate_sizes() {
    for (u32 i = 0; i < hash_table_capacity(&type_table); i++) {
        Hash_Table_Slot *slot = _hash_table_get_slot((void **)&type_table, i);
        Type_Info *item;
        if (slot->state == SLOT_OCCUPIED) {
            item = type_table[i];

            if (item->size == 0 && item->type != UNKNOWN) {
                if (type_table_calculate_size_of(item) != 0)
                    return -1;
            }
        }
    }

    return 0;
}










void registered_functions_init() {
    registered_functions = array_list_make(Type_Info *, 8, &std_allocator);
    registered_functions_headers = array_list_make(String, 8, &std_allocator);
}


void type_table_init() {
    arena_strings                       = arena_make(4*KB);
    arena_type_info                     = arena_make(4*KB);
    arena_type_info_function_argument   = arena_make(2*KB);
    arena_type_info_struct_member       = arena_make(2*KB);
    arena_type_info_enum_member         = arena_make(2*KB);

    type_table = hash_table_make(Type_Info *, 32, &std_allocator);

    Type_Info *item;

    // Most basic C types
    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("int"), 4, 4, .t_integer = { 32, true } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("int") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("char"), 1, 1, .t_integer = { 8, false } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("char") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { FLOAT, CSTR("float"), 4, 4, .t_float = { 32 } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("float") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { BOOL, CSTR("bool"), 1, 1 });
    hash_table_put(&type_table, item, UNPACK_LITERAL("bool") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { VOID, CSTR("void"), 0, 0 });
    hash_table_put(&type_table, item, UNPACK_LITERAL("void") );

    // Custom int typedefs that are commonly used, making them as actual int meta types for simplicity.
    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("s8"), 1, 1, .t_integer = { 8, true } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("s8") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("u8"), 1, 1, .t_integer = { 8, false } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("u8") );


    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("s16"), 2, 2, .t_integer = { 16, true } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("s16") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("u16"), 2, 2, .t_integer = { 16, false } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("u16") );


    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("s32"), 4, 4, .t_integer = { 32, true } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("s32") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("u32"), 4, 4, .t_integer = { 32, false } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("u32") );


    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("s64"), 8, 8, .t_integer = { 64, true } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("s64") );

    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { INTEGER, CSTR("u64"), 8, 8, .t_integer = { 64, false } });
    hash_table_put(&type_table, item, UNPACK_LITERAL("u64") );


    item = arena_alloc(&arena_type_info, sizeof(Type_Info));
    *item = ((Type_Info) { STRING, CSTR("String"), 16, 8 });
    hash_table_put(&type_table, item, UNPACK_LITERAL("String") );
}











/**
 * This is just an abstraction of checking token with expected type.
 */
int parser_expect_token_type(Token *token, Token_Type expected) {
    if (token->type != expected) {
        printf_err("%s:%lld Expected ", current_file_name, token->line_num);
        token_type_print(expected);
        printf("but got ");
        token_type_print(token->type);
        printf(".\n");
        return 1;
    }

    return 0;
}

/**
 * This is just an abstraction of getting and checking next token.
 */
int parser_get_and_expect_token(Lexer *lexer, Token *output, Token_Type expected) {
    *output = lexer_next_token(lexer);

    return parser_expect_token_type(output, expected);
}

/**
 * It might be possible to write a more 'elegant' way of saving lexer state, but for right now it is just easier to copy lexer struct.
 */
Token parser_peek_next(Lexer lexer) {
    return lexer_next_token(&lexer);
}

/**
 * This is just wrapper function to abstract away same parsing mechanism for pointers.
 */
String parser_parse_pointer_asterisks(Lexer *lexer, Token *token) {
    String base_typename = token->str;
    s64 asterisk_count = 0;

    while (true) {
        *token = parser_peek_next(*lexer);
        if (token->type != TOKEN_ASTERISK) {
            break;
        }

        lexer_next_token(lexer);
        asterisk_count++;
#ifdef LOG
        printf("Identified a pointer.\n");
#endif
    }
    base_typename = type_table_add_pointer(base_typename, asterisk_count);

    return base_typename;
}



/**
 * Following code until END can be project and task specific.
 * Add new meta_note_process_*(...) here.
 *
 * Template:

int meta_note_process_MyNote(Lexer lexer) {

    return 0;
}

 */




/**
 * This is a comment of what @Introspect EXPECTS, because I am not writing an entire C parser for this to work :D
 *
 * Introspect will search for following patterns anything else will output an error.
 * 
 * [typedef] [SYMBOL typename] [optional '*'] [SYMBOL alias typename] [';']
 *  Examples:
 *      typedef int     i32;
 *      typedef i32**   Custom_i32_ptr;
 *
 * [typedef] [struct, enum, union] [SYMBOL typename] ['{...}'] [SYMBOL alias typename] [';']
 *  Examples:
 *      typedef struct console {
 *          ...
 *      } Console;
 *
 * [SYMBOL typename] [optional '*'] [SYMBOL function name] ['(...)'] ['{', ';']
 *  Examples:
 *      void *make_buffer(...);
 *      int exec_function() {
 *
 */
int meta_note_process_Introspect(Lexer lexer) {
    Token next;

    
    // Lexing until SYMBOL token.
    while (true) {
        next = lexer_next_token(&lexer);

        if (next.type == TOKEN_ZERO) return 0;

        if (next.type == TOKEN_SYMBOL) break;
    }


    // Variable for typename.
    String typename = {0};

    if (str_equals(next.str, TYPEDEF_STR)) {
        

        // Checking if it is a struct, enum or union, if not just doing direct alias.
        if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;
        
        if (str_equals(next.str, STRUCT_STR)) {

            // Parsing [struct] [SYMBOL typename] ['{...}'].
            if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

            typename = next.str;

            // Type checking if defined.
            if (type_table_defined(typename)) {
                printf_err("%s:%lld @Introspect: Struct typename is already defined as: '%.*s'\n", current_file_name, next.line_num, UNPACK(typename));
                return 1;
            }

            SAVE_STRING(typename);
            Type_Info *struct_type = type_table_add_struct(typename);
#ifdef LOG 
            printf("Identified struct typename as: '%.*s'\n", UNPACK(typename)); 
#endif
            



            
            // Expecting '{'
            if (parser_get_and_expect_token(&lexer, &next, TOKEN_CURLY_OPEN) != 0) return 1;

            while(true) {
                next = lexer_next_token(&lexer);
                
                if (next.type == TOKEN_CURLY_CLOSE) {
                    break;
                }
                
                // Parsing field.
                // Expecting only:
                //  [SYMBOL field_typename] [optional '*'] [SYMBOL field_name] [';']
                
                String field_typename = {0};
                String field_name = {0};

                if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) return 1;

                field_typename = next.str;

                if (!type_table_exists(field_typename)) {
                    SAVE_STRING(field_typename);
                    type_table_add_unknown(field_typename);
                }


                // This function automatically writes string's data to arena_strings.
                // No reason to SAVE_STRING here.
                field_typename = parser_parse_pointer_asterisks(&lexer, &next);

#ifdef LOG 
                printf("    - Identified field typename as: '%.*s'\n", UNPACK(field_typename)); 
#endif
                



                if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

                field_name = next.str;

                SAVE_STRING(field_name);
#ifdef LOG 
                printf("    - Identified field: '%.*s'\n", UNPACK(field_name)); 
#endif

                
                // Adding field type and name to struct.
                type_table_add_struct_member(struct_type, field_typename, field_name);


                // Semicolon check.
                if (parser_get_and_expect_token(&lexer, &next, TOKEN_SEMICOLON) != 0) return 1;
            }

        } 
        else if (str_equals(next.str, UNION_STR)) {
            // Parsing [union] [SYMBOL typename] ['{...}'].
            TODO("UNION introspection.");

        }
        else if (str_equals(next.str, ENUM_STR)) {
            // Parsing [enum] [SYMBOL typename] [optional ':', SYMBOL int typename] ['{...}'].

            // Getting typename
            if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

            typename = next.str;

            // Type checking if defined.
            if (type_table_defined(typename)) {
                printf_err("%s:%lld @Introspect: Enum typename is already defined as: '%.*s'\n", current_file_name, next.line_num, UNPACK(typename));
                return 1;
            }

            SAVE_STRING(typename);
            Type_Info *enum_type = type_table_add_enum(typename);
#ifdef LOG 
            printf("Identified enum typename as: '%.*s'\n", UNPACK(typename)); 
#endif
            
            // [optional ':', SYMBOL integer typename]
            // Colon check.
            next = parser_peek_next(lexer);
            if (next.type == TOKEN_COLON) {
                lexer_next_token(&lexer);

                next = lexer_next_token(&lexer);
                
                Type_Info *integer_type = *(Type_Info **)hash_table_get(&type_table, UNPACK(next.str)); 
                if (integer_type == NULL) {
                    printf_err("%s:%lld @Introspect: Unknown underlying enum type: '%.*s'\n", current_file_name, next.line_num, UNPACK(next.str));
                    return 1;
                }


                if (integer_type->type != INTEGER) {
                    printf_err("%s:%lld @Introspect: Underlying enum type is not integer: '%.*s'\n", current_file_name, next.line_num, UNPACK(next.str));
                    return 1;
                }
                
                // Making enum to have underlying integer type specified.
                enum_type->size = integer_type->size;
                enum_type->align = integer_type->align;
                enum_type->t_enum.is_signed = integer_type->t_integer.is_signed;
            }

            



            // Expecting '{'
            if (parser_get_and_expect_token(&lexer, &next, TOKEN_CURLY_OPEN) != 0) return 1;

            u64 enum_value_counter = 0;
            while(true) {
                next = lexer_next_token(&lexer);
                
                if (next.type == TOKEN_CURLY_CLOSE) {
                    break;
                }
                
                // Parsing member.
                // Expecting only:
                //  [SYMBOL member_name] [optional '=', int value] [',']
                
                String member_name = {0};

                if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) return 1;

                member_name = next.str;
                SAVE_STRING(member_name);

#ifdef LOG 
                printf("    - Identified member name as: '%.*s'\n", UNPACK(member_name)); 
#endif


                // [optional '=', int value]
                next = parser_peek_next(lexer);
                if (next.type == TOKEN_ASSIGN) {
                    lexer_next_token(&lexer);
                    
                    String value_str = lexer_next_token(&lexer).str;
                    while (parser_peek_next(lexer).type != TOKEN_COMMA) {
                        value_str.length += lexer_next_token(&lexer).str.length;
                    }
                    
                
                    
                    if (!str_is_int(value_str)) {
                        printf_err("%s:%lld @Introspect: Expected enum member value to be integer or is unsupported integer format: '%.*s'\n", current_file_name, next.line_num, UNPACK(value_str));
                        return 1;
                    }

                    enum_value_counter = (u64)str_parse_int(value_str);
                }


                // Adding field type and name to struct.
                type_table_add_enum_member(enum_type, member_name, enum_value_counter);

#ifdef LOG 
                printf("    - Identified member value as: '0x%llx'\n", enum_value_counter); 
#endif

                enum_value_counter++;




                // Comma check.
                if (parser_get_and_expect_token(&lexer, &next, TOKEN_COMMA) != 0) return 1;
            }
            
        }
        else {
            // Parsing [SYMBOL typename] [optional '*'].
            if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) return 1;

            typename = next.str;

            // Type checking if exists, if not recording it as unknown.
            if (!type_table_exists(typename)) {
                SAVE_STRING(typename);
                type_table_add_unknown(typename);
            }



            // Parsing [optional '*'].
            // We are rewriting typename, since it will be ultimetly the identifier that is aliased by a typedef.
            typename = parser_parse_pointer_asterisks(&lexer, &next);

#ifdef LOG 
            printf("Identified typename as: '%.*s'\n", UNPACK(typename)); 
#endif
        }

typedef_alias_end:
        // Variable for alias typename.
        String alias_typename = {0};

        if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

        alias_typename = next.str;

        // Checking if alias == actual typename.
        if (str_equals(typename, alias_typename)) {
#ifdef LOG 
            printf("Identified typedef to be equal to typename, typedef meta type is ignored.\n"); 
#endif
            alias_typename = typename;
        } else {
            SAVE_STRING(alias_typename);
            if (type_table_add_typedef(typename, alias_typename) != 0) return 1;
#ifdef LOG 
            printf("Identified typedef alias as name: '%.*s'\n", UNPACK(alias_typename)); 
#endif
        }




        // Semicolon check.
        if (parser_get_and_expect_token(&lexer, &next, TOKEN_SEMICOLON) != 0) return 1;
        

        return 0;
    }



    else {
        // Parsing [SYMBOL return typename] [optional '*'] [SYMBOL typename] 
        String return_typename = {0};

        if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) return 1;

        return_typename = next.str;

        // Type checking if exists, if not recording it as unknown.
        if (!type_table_exists(return_typename)) {
            SAVE_STRING(return_typename);
            type_table_add_unknown(return_typename);
        }

        // Parsing [optional '*'].
        // We are rewriting typename, since it will be ultimetly the identifier that is aliased by a typedef.
        return_typename = parser_parse_pointer_asterisks(&lexer, &next);

#ifdef LOG 
        printf("    - Identified function return typename: '%.*s'\n", UNPACK(return_typename)); 
#endif


        if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

        typename = next.str;
        if (type_table_defined(typename)) {
            printf_err("%s:%lld @Introspect: Function typename is already defined as: '%.*s'\n", current_file_name, next.line_num, UNPACK(typename));
            return 1;
        }
        
        SAVE_STRING(typename);
        
        Type_Info *return_type = *(Type_Info **)(hash_table_get(&type_table, UNPACK(return_typename)));
        Type_Info *function_type = type_table_add_function(return_type, typename);
        
        
#ifdef LOG 
        printf("    - Identified function typename: '%.*s'\n", UNPACK(typename)); 
#endif


        // Parsing ['(...)'] ['{', ';']

        // Expecting '('
        if (parser_get_and_expect_token(&lexer, &next, TOKEN_PARAN_OPEN) != 0) return 1;

        while(true) {
            next = lexer_next_token(&lexer);

            if (next.type == TOKEN_PARAN_CLOSE) {
                break;
            }

            // Parsing field.
            // Expecting only:
            //  [SYMBOL arg_typename] [optional '*'] [SYMBOL arg_name]

            String arg_typename = {0};
            String arg_name = {0};

            if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) return 1;

            arg_typename = next.str;

            if (!type_table_exists(arg_typename)) {
                SAVE_STRING(arg_typename);
                type_table_add_unknown(arg_typename);
            }

            // This function automatically writes string's data to arena_strings.
            // No reason to SAVE_STRING here.
            arg_typename = parser_parse_pointer_asterisks(&lexer, &next);

#ifdef LOG 
            printf("    - Identified arg typename as: '%.*s'\n", UNPACK(arg_typename)); 
#endif



            if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) return 1;

            arg_name = next.str;
            SAVE_STRING(arg_name);
#ifdef LOG 
            printf("    - Identified argument: '%.*s'\n", UNPACK(arg_name)); 
#endif


            // Adding arg type and name to function.
            type_table_add_function_argument(function_type, arg_typename, arg_name);


            // Checking if [','] is present, if it is, eat it.
            next = parser_peek_next(lexer);
            if (next.type == TOKEN_COMMA) {
                next = lexer_next_token(&lexer);

            }
        }

        // Semicolon or '{' check.
        next = lexer_next_token(&lexer);

        if (next.type != TOKEN_SEMICOLON && next.type != TOKEN_CURLY_OPEN) {
            printf_err("%s:%lld @Introspect: Expected ';' or '{' at the end of function type definition.\n", current_file_name, next.line_num);
            return 1;
        }

        return 0;
    }
}



int meta_note_process_RegisterCommand(Lexer lexer, FILE *output_h) {
    Token next;
    
    // Lexing until SYMBOL token.
    while (true) {
        next = lexer_next_token(&lexer);

        if (next.type == TOKEN_ZERO) return 0;

        if (next.type == TOKEN_SYMBOL) break;
    }

    // Parsing [SYMBOL return typename] [optional '*'] [SYMBOL typename] 
    if (parser_expect_token_type(&next, TOKEN_SYMBOL) != 0) {
        return 1;
    }

    // Parsing [optional '*'].
    parser_parse_pointer_asterisks(&lexer, &next);

    // Getting to the typename of the function.
    if (parser_get_and_expect_token(&lexer, &next, TOKEN_SYMBOL) != 0) {
        return 1;
    }

    // Checking if type exists.
    if (!type_table_defined(next.str)) {
        printf_err("%s:lld @RegisterCommand: Expected function '%.*s' to be introspected.\n", current_file_name, next.line_num, UNPACK(next.str));
        return 1;
    }

    String typename = next.str;

    Type_Info *type = *(Type_Info **)hash_table_get(&type_table, UNPACK(typename));


    // Extra check to see if the type is truly a function.
    if (type->type != FUNCTION) {
        printf_err("%s:lld @RegisterCommand: Expected '%.*s' to be a function.\n", current_file_name, next.line_num, UNPACK(typename));
        return 1;
    }

    // Appending function.
    array_list_append(&registered_functions, type);
    

    // Appending it's definition_file if its unique and if it is .h.
    if (str_find(type->t_function.definition_file, STR_BUFFER(".h")) != -1) {
        u32 i = -1; // @Carefull: Very dangerous overflowing is here.
        do  {
            i++;

            // If no elements left.
            if (i == array_list_length(&registered_functions_headers)) {
                array_list_append(&registered_functions_headers, type->t_function.definition_file);
                break;
            }

        } while (!str_equals(registered_functions_headers[i], type->t_function.definition_file));

    }


    return 0;
}








/**
 * END
 */








void meta_replace_with_space(String metanote_str) {
    for (s64 i = 0; i < metanote_str.length; i++) {
        metanote_str.data[i] = ' ';
    }
}


/**
 * Meta processing of individual file.
 * It will fwrite generated output to the output file.
 * @Important: output must be opened in "a" mode, in order for this function to properly append generated code.
 */
int meta_process_file(char *file_name, FILE *output_h, char *output_path) {
#ifdef LOG
    meta_log("Processing '%s'\n", file_name);
#endif

    current_file_name = file_name;
    
    String _content = read_file_into_str(current_file_name, &std_allocator);

    if (_content.data == NULL) {
        printf_err("Couldn't read file '%s'\n", current_file_name);
        return 1;
    }

    
    
    
    Lexer lexer;
    
    // Process _content here.
    lexer_init(&lexer, _content);

    Token next;
    while(true) {
        next = lexer_next_token(&lexer);

        if (next.type == TOKEN_ZERO) break;


        // Searching for metanotes.
        if (next.type == TOKEN_METANOTE) {

#ifdef LOG
            printf("       - Found meta note '%.*s' in file '%s' on line %lld.\n", UNPACK(next.str), current_file_name, lexer.line_num);
#endif
            
            if (next.str.length <= 1) {
                printf_err("%s:%lld Missing TOKEN_METANOTE name.\n", current_file_name, lexer.line_num);
                return 1;
            }
            
            

            // Triggering specific metanote.
            if (str_equals(next.str, CSTR("@Introspect"))) {
                if (meta_note_process_Introspect(lexer) != 0) 
                    return 1;
                goto metanote_remove_continue;
            }

            if (str_equals(next.str, CSTR("@RegisterCommand"))) {
                if (meta_note_process_RegisterCommand(lexer, output_h) != 0) 
                    return 1;
                goto metanote_remove_continue;
            }

            printf_err("%s:%lld Unknown meta note: '%.*s'.\n", current_file_name, lexer.line_num, UNPACK(next.str));
            return 1;

metanote_remove_continue:
            // Removing meta note from final source.
            meta_replace_with_space(next.str);
        }
    }
    



    // Write source to build.
    char src_file_name[strlen(output_path) + 1 + strlen(current_file_name) + 1];
    strcpy(src_file_name, output_path);
    strcat(src_file_name, "/");
    strcat(src_file_name, current_file_name);

#ifdef LOG
    meta_log("Producing build source file '%s'\n", src_file_name);
#endif
    

    if (write_str_to_file(_content, src_file_name) != 0) {
        printf_err("Couldn't create source file '%s'\n", src_file_name);
        return 1;
    }


    allocator_free(&std_allocator, _content.data);




    return 0;
}













String meta_generated_comment = CSTR(
        "/**\n"
        " * THIS FILE IS AUTO GENERATED BY META.EXE\n"
        " *\n"
        " * It will contain auto generated code that then is appended to the main compilation.\n"
        " * Do not modify any code, it will be overwritten.\n"
        " */\n"
        );


/**
 * This function overwrites file specified by file_name.
 * Then opens it in "a" mode and returns pointer to it, ready to accept auto generated code.
 * Needs to closed with fclose after use.
 */
FILE *meta_generate(char *file_name) {
    if (write_str_to_file(meta_generated_comment, file_name) != 0) {
        printf_err("Couldn't overwrite meta generated file '%s'\n", file_name);
        return NULL;
    }

    FILE *file = fopen(file_name, "ab");
    if (file == NULL) {
        printf_err("Couldn't open meta generated file for appending '%s'.\n", file_name);
        return NULL;
    }

    return file;
}








// Essentially meta programm is compiled into a file and immediately executed as a preprocces.
int meta_process(int count, char **files, char *output_path) {
    printf("\n\n");

    
#ifdef LOG
    meta_log("Files passed to meta.exe: ");
    for (int i = 0; i < count; i++) {
        printf("'%s' ", files[i]);
    }
    printf("\n");
#endif

    registered_functions_init();
    type_table_init();

    char *meta_generated_file_name = "src/meta_generated.h";
    char meta_generated_full_path[strlen(output_path) + 1 + strlen(meta_generated_file_name) + 1];
    strcpy(meta_generated_full_path, output_path);
    strcat(meta_generated_full_path, "/");
    strcat(meta_generated_full_path, meta_generated_file_name);

    // Preparing meta generated files.
    FILE *meta_generated_h = meta_generate(meta_generated_full_path);

    if (meta_generated_h == NULL) {
        return 1;
    }
    
    
    // Header defines
    fwrite_str(STR_BUFFER("#ifndef META_GENERATED_H\n#define META_GENERATED_H\n\n"), meta_generated_h);


    /**
     * Processing each file.
     */
    for (int i = 0; i < count; i++) {
        if (meta_process_file(files[i], meta_generated_h, output_path) != 0) {
            fclose(meta_generated_h);
            return 1;
        }
    }


    /**
     * Calculating type sizes.
     */
    if (type_table_calculate_sizes() != 0) {
        return 1;
    }
    

    /**
     * Generating Introspect Meta data.
     */

    // Include
    fwrite_str(STR_BUFFER("#include \"core/typeinfo.h\"\n"), meta_generated_h);
    fwrite_str(STR_BUFFER("#include \"game/command.h\"\n\n"), meta_generated_h);

    // Tool defines.
    fwrite_str(STR_BUFFER("#define META_TYPE(type) META_TYPE_##type\n\n"), meta_generated_h);

    fwrite_str(STR_BUFFER("#define TYPE_OF(type) (&META_TYPE_TABLE[META_TYPE(type)])\n\n"), meta_generated_h);
    // First generating Meta_Type enum.
    fwrite_str(STR_BUFFER("typedef enum meta_type {\n"), meta_generated_h);
    for (u32 i = 0; i < hash_table_capacity(&type_table); i++) {
        Hash_Table_Slot *slot = _hash_table_get_slot((void **)&type_table, i);
        if (slot->state == SLOT_OCCUPIED) {
            fprintf(meta_generated_h, "    META_TYPE(%.*s),\n", UNPACK(slot->key));
        }
    }
    fwrite_str(STR_BUFFER("} Meta_Type;\n\n"), meta_generated_h);

    // Forward declaring META_TYPE_TABLE
    fwrite_str(STR_BUFFER("static Type_Info META_TYPE_TABLE[];\n\n"), meta_generated_h);

    // Generating META_TYPE_FUNCTION_ARGS[]
    fwrite_str(STR_BUFFER("static Type_Info_Function_Argument META_TYPE_FUNCTION_ARGS[] = {\n"), meta_generated_h);
    for (u32 i = 0; i < arena_size(&arena_type_info_function_argument) / sizeof(Type_Info_Function_Argument); i++) {
        Type_Info_Function_Argument *arg = ((Type_Info_Function_Argument *)arena_type_info_function_argument.allocation) + i;
        String typename = type_table_get_typename(arg->type);
        fprintf(meta_generated_h, "    { TYPE_OF(%.*s), STR_BUFFER(\"%.*s\")", UNPACK(typename), UNPACK(arg->name));

        fwrite_str(STR_BUFFER(" },\n"), meta_generated_h);
    }
    fwrite_str(STR_BUFFER("};\n\n"), meta_generated_h);


    // Generating META_TYPE_STRUCT_MEMBERS[]
    fwrite_str(STR_BUFFER("static Type_Info_Struct_Member META_TYPE_STRUCT_MEMBERS[] = {\n"), meta_generated_h);
    for (u32 i = 0; i < arena_size(&arena_type_info_struct_member) / sizeof(Type_Info_Struct_Member); i++) {
        Type_Info_Struct_Member *member = ((Type_Info_Struct_Member *)arena_type_info_struct_member.allocation) + i;
        String typename = type_table_get_typename(member->type);
        fprintf(meta_generated_h, "    { TYPE_OF(%.*s), STR_BUFFER(\"%.*s\"), %u", UNPACK(typename), UNPACK(member->name), member->offset);

        fwrite_str(STR_BUFFER(" },\n"), meta_generated_h);
    }
    fwrite_str(STR_BUFFER("};\n\n"), meta_generated_h);


    // Generating META_TYPE_ENUM_MEMBERS[]
    fwrite_str(STR_BUFFER("static Type_Info_Enum_Member META_TYPE_ENUM_MEMBERS[] = {\n"), meta_generated_h);
    for (u32 i = 0; i < arena_size(&arena_type_info_enum_member) / sizeof(Type_Info_Enum_Member); i++) {
        Type_Info_Enum_Member *member = ((Type_Info_Enum_Member *)arena_type_info_enum_member.allocation) + i;
        fprintf(meta_generated_h, "    { STR_BUFFER(\"%.*s\"), %llu", UNPACK(member->name), member->value);

        fwrite_str(STR_BUFFER(" },\n"), meta_generated_h);
    }
    fwrite_str(STR_BUFFER("};\n\n"), meta_generated_h);


    // Now generating type table itself
    fwrite_str(STR_BUFFER("static Type_Info META_TYPE_TABLE[] = {\n"), meta_generated_h);
    for (u32 i = 0; i < hash_table_capacity(&type_table); i++) {
        Hash_Table_Slot *slot = _hash_table_get_slot((void **)&type_table, i);
        Type_Info *item;
        if (slot->state == SLOT_OCCUPIED) {
            item = type_table[i];


            fprintf(meta_generated_h, "    [META_TYPE(%.*s)] = (Type_Info) { ", UNPACK(slot->key));
            
            // Switching between type groups.
            switch(item->type) {
                case INTEGER:
                    fprintf(meta_generated_h, "INTEGER, STR_BUFFER(\"%.*s\"), %u, %u, .t_integer = { %d, %d }", UNPACK(item->name), item->size, item->align, item->t_integer.size_bits, item->t_integer.is_signed);
                    break;
                case FLOAT:
                    fprintf(meta_generated_h, "FLOAT, STR_BUFFER(\"%.*s\"), %u, %u, .t_float = { %d }", UNPACK(item->name), item->size, item->align, item->t_float.size_bits);
                    break;
                case BOOL:
                    fprintf(meta_generated_h, "BOOL, STR_BUFFER(\"%.*s\"), %u, %u", UNPACK(item->name), item->size, item->align);
                    break;
                case POINTER:
                    /**
                     * What this line assumes is that every pointer typename is 'basename' + '_ptr' at the end, so by cutting off '_ptr' we get typename of the base type.
                     */
                    String ptr_to_typename = slot->key;
                    ptr_to_typename.length -= TYPE_PTR_POSTFIX.length;
                   
                    fprintf(meta_generated_h, "POINTER, STR_BUFFER(\"%.*s\"), %u, %u, .t_pointer = { TYPE_OF(%.*s) }", UNPACK(item->name), item->size, item->align, UNPACK(ptr_to_typename));
                    break;
                case FUNCTION:
                    String return_typename = type_table_get_typename(item->t_function.return_type);

                    u64 function_arg_index = item->t_function.arguments - (Type_Info_Function_Argument *)arena_type_info_function_argument.allocation; 
                    
                    fprintf(meta_generated_h, "FUNCTION, STR_BUFFER(\"%.*s\"), %u, %u, .t_function = { TYPE_OF(%.*s), %u, &META_TYPE_FUNCTION_ARGS[%llu], STR_BUFFER(\"%.*s\") }", UNPACK(item->name), item->size, item->align, UNPACK(return_typename), item->t_function.arguments_length, function_arg_index, UNPACK(item->t_function.definition_file));
                    break;
                case VOID:
                    fprintf(meta_generated_h, "VOID, STR_BUFFER(\"%.*s\"), %u, %u", UNPACK(item->name), item->size, item->align);
                    break;
                case STRUCT:
                    u64 struct_member_index = item->t_struct.members - (Type_Info_Struct_Member *)arena_type_info_struct_member.allocation; 
                    
                    fprintf(meta_generated_h, "STRUCT, STR_BUFFER(\"%.*s\"), %u, %u, .t_struct = { %u, &META_TYPE_STRUCT_MEMBERS[%llu] }", UNPACK(item->name), item->size, item->align, item->t_struct.members_length, struct_member_index);
                    break;
                case ARRAY:
                    TODO("Array meta generation.");
                    fwrite_str(STR_BUFFER("ARRAY, .t_array = { "), meta_generated_h);
                    // ...
                    fwrite_str(STR_BUFFER(" }"), meta_generated_h);
                    break;
                case STRING:
                    fprintf(meta_generated_h, "STRING, STR_BUFFER(\"%.*s\"), %u, %u", UNPACK(item->name), item->size, item->align);
                    break;
                case ENUM:
                    u64 enum_member_index = item->t_enum.members - (Type_Info_Enum_Member *)arena_type_info_enum_member.allocation; 
                    
                    fprintf(meta_generated_h, "ENUM, STR_BUFFER(\"%.*s\"), %u, %u, .t_enum = { %d, %u, &META_TYPE_ENUM_MEMBERS[%llu] }", UNPACK(item->name), item->size, item->align, item->t_enum.is_signed, item->t_enum.members_length, enum_member_index);
                    break;
                case TYPEDEF:
                    String typedef_of_typename = type_table_get_typename(item->t_typedef.typedef_of);
                    fprintf(meta_generated_h, "TYPEDEF, STR_BUFFER(\"%.*s\"), %u, %u, .t_typedef = { TYPE_OF(%.*s) }", UNPACK(item->name), item->size, item->align, UNPACK(typedef_of_typename));
                    break;
                case UNKNOWN:
                    fprintf(meta_generated_h, "UNKNOWN, STR_BUFFER(\"%.*s\"), %u, %u", UNPACK(item->name), item->size, item->align);
                    break;
            }
           
            

            fwrite_str(STR_BUFFER(" },\n"), meta_generated_h);
        }
    }
    fwrite_str(STR_BUFFER("};\n"), meta_generated_h);





    /**
     * Generating registered functions code.
     */

    fprintf(meta_generated_h, "\n\n");

    // H files included.
    for (u32 i = 0; i < array_list_length(&registered_functions_headers); i++) {
        fprintf(meta_generated_h, "#include \"%.*s\"\n", UNPACK(registered_functions_headers[i]));
    }

    // H file output.
    for (u32 i = 0; i < array_list_length(&registered_functions); i++) {
        Type_Info *item = registered_functions[i];


        fprintf(meta_generated_h, "\nstatic void COMMAND_PREFIX(%.*s)(Any *args, u32 args_length) {\n", UNPACK(item->name));
        

        Type_Info *return_type = get_base_of_typedef(item->t_function.return_type);
        if (return_type->type != VOID) {
            fprintf(meta_generated_h, "    %.*s _rvalue = %.*s(", UNPACK(return_type->name), UNPACK(item->name));
        } 
        else {
            fprintf(meta_generated_h, "    %.*s(", UNPACK(item->name));
        }

        for (u32 j = 0; j < item->t_function.arguments_length; j++) {
            fprintf(meta_generated_h, "*(%.*s*)args[%u].data", UNPACK(item->t_function.arguments[j].type->name), j);

            if (j + 1 < item->t_function.arguments_length)
                fprintf(meta_generated_h, ", ");
        }
        fprintf(meta_generated_h, ");\n\n");

        
        if (return_type->type != VOID) {
            fprintf(meta_generated_h, "    args[0].type = TYPE_OF(%.*s);\n", UNPACK(return_type->name));
            fprintf(meta_generated_h, "    *(%.*s*)args[0].data = _rvalue;\n", UNPACK(return_type->name));
        }

        fprintf(meta_generated_h, "}\n");
    }

    fprintf(meta_generated_h, "\nstatic void register_all_commands() {\n");
    for (u32 i = 0; i < array_list_length(&registered_functions); i++) {
        Type_Info *item = registered_functions[i];

        fprintf(meta_generated_h, "    command_register(TYPE_OF(%.*s), COMMAND_PREFIX(%.*s));\n", UNPACK(item->name), UNPACK(item->name));


    }
    fprintf(meta_generated_h, "}\n\n");









    // Ending header file.
    fwrite_str(STR_BUFFER("#endif\n"), meta_generated_h);




    fclose(meta_generated_h);

    // hash_table_print((void **)&type_table);

    printf("\n\n");

    return 0;
}
