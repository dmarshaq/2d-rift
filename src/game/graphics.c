#include "game/graphics.h"

#include "core/core.h"
#include "core/type.h"
#include "core/str.h"
#include "core/file.h"
#include "core/structs.h"
#include "core/mathf.h"
#include "core/log.h"


#include "SDL2/SDL_video.h"
#include <GL/glew.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"


/**
 * Graphics.
 */



bool check_gl_error() {
    s32 error = glGetError();
    if (error != 0) {
        LOG_ERROR("OpenGL error: %d.", error);
        return false;
    }
    return true;
}

int init_sdl_gl() {
    // Initialize SDL.
    if (SDL_Init(SDL_INIT_VIDEO) < 0 || SDL_Init(SDL_INIT_AUDIO) < 0) {
        LOG_ERROR("SDL could not initialize! SDL_Error: %s.", SDL_GetError());
        return 1;
    }

    // Set OpenGL attributes.
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    return 0;
}

Window_Info create_gl_window(const char *title, int x, int y, int width, int height) {
    // Create window.
    SDL_Window *window = SDL_CreateWindow(title, x, y, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        LOG_ERROR("Window could not be created! SDL_Error: %s.", SDL_GetError());
        return (Window_Info) { NULL, 0, 0 };
    }

    // Create OpenGL context.
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == NULL) {
        LOG_ERROR("OpenGL context could not be created! SDL_Error: %s.", SDL_GetError());
        return (Window_Info) { window, width, height };
    }

    if (SDL_GL_MakeCurrent(window, glContext) < 0) {
        LOG_ERROR("OpenGL current could not be created! SDL_Error: %s.", SDL_GetError());
        return (Window_Info) { window, width, height };
    }

    // Initialize GLEW.
    if (glewInit() != GLEW_OK) {
        LOG_ERROR("GLEW initialization failed!.");
        return (Window_Info) { window, width, height };
    }

    return (Window_Info) { window, width, height };
}

int init_sdl_audio() {
    // Initialize sound.
    if (Mix_Init(MIX_INIT_MP3) < 0) {
        LOG_ERROR("SDL Mixer could not initialize! SDL_Error: %s.", SDL_GetError());
        return 1;
    }

    (void)Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024);
    return 0;
}


// Shader loading key words.
String shader_version_tag      = CSTR("#version");
String vertex_shader_defines   = CSTR("#define VERTEX\n");
String fragment_shader_defines = CSTR("#define FRAGMENT\n");


s32 shader_strings_lengths[3];

// Texture params.
s32 texture_wrap_s = GL_CLAMP_TO_EDGE;
s32 texture_wrap_t = GL_CLAMP_TO_EDGE;
s32 texture_min_filter = GL_LINEAR;
s32 texture_max_filter = GL_LINEAR;

// Shader unifroms.
Matrix4f shader_uniform_pr_matrix = MATRIX4F_IDENTITY;
Matrix4f shader_uniform_ml_matrix = MATRIX4F_IDENTITY;
s32 shader_uniform_samplers[32] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };

// Drawing variables.
Vertex_Buffer verticies;
u32 *quad_indicies;
u32 texture_ids[32];

void graphics_init() {
    // Enable Blending (Rendering with alpha channels in mind).
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Make stbi flip images vertically when loading.
    stbi_set_flip_vertically_on_load(true);

    // Setting drawing variables.
    verticies = vertex_buffer_make(); // @Leak
                                      //
    quad_indicies = array_list_make(u32, MAX_QUADS_PER_BATCH * 6, &std_allocator); // @Leak
    
    // Initing quad indicies.
    Array_List_Header *header = (void *)(quad_indicies) - sizeof(Array_List_Header);
    header->length = MAX_QUADS_PER_BATCH * 6;
    for (u32 i = 0; i < MAX_QUADS_PER_BATCH * 6; i++) {
        quad_indicies[i] = i - (i / 3) * 2 + (i / 6) * 2;
    }
}

Texture texture_load(char *texture_path) {
    // Process image into texture.
    Texture texture;
    s32 nrChannels;

    u8 *data = stbi_load(texture_path, &texture.width, &texture.height, &nrChannels, 0);

    if (data == NULL) {
        LOG_ERROR("Stbi couldn't load image.");
    }

    // Loading a single image into texture example:
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_s);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_max_filter);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    return texture;
}

void texture_unload(Texture *texture) {
    glDeleteTextures(1, &texture->id);

    texture->id = 0;
    texture->width = 0;
    texture->height = 0;
}

UV_Region uv_slice(u32 rows, u32 cols, u32 index) {
    UV_Region uv = {
        .uv0 = vec2f_make((1.0f / (float)cols) * (float)(index % cols), (1.0f / (float)rows) * (float)((rows - 1) - (u32)(index / rows)))
    };
    uv.uv1 = vec2f_make(uv.uv0.x + 1.0f / (float)cols, uv.uv0.y + 1.0f / (float)rows);
    return uv;
}


const char *shader_uniform_pr_matrix_name = "pr_matrix";
const char *shader_uniform_ml_matrix_name = "ml_matrix";
const char *shader_uniform_samplers_name = "u_textures";

/**
 * @Temporary: Later, setting uniforms either will be done more automatically, or simplified to be done by user manually. 
 * But right now it is not neccassary to care about too much, since only one shader is used anyway.
 */
void shader_init_uniforms(Shader *program) {
    // Get uniform's locations based on unifrom's name.
    s32 quad_shader_pr_matrix_loc = glGetUniformLocation(program->id, shader_uniform_pr_matrix_name);
    if (quad_shader_pr_matrix_loc == -1) {
        LOG_WARNING("Couldn't get location of %s uniform, in shader with id: %d.", shader_uniform_pr_matrix_name, program->id);
    }

    s32 quad_shader_ml_matrix_loc = glGetUniformLocation(program->id, shader_uniform_ml_matrix_name);
    if (quad_shader_ml_matrix_loc == -1) {
        LOG_WARNING("Couldn't get location of %s uniform, in shader with id: %d.", shader_uniform_ml_matrix_name, program->id);
    }

    s32 quad_shader_samplers_loc = glGetUniformLocation(program->id, shader_uniform_samplers_name);
    if (quad_shader_samplers_loc == -1) {
        LOG_WARNING("Couldn't get location of %s uniform, in shader with id: %d.", shader_uniform_samplers_name, program->id);
    }
    
    // Set uniforms.
    glUseProgram(program->id);
    glUniformMatrix4fv(quad_shader_pr_matrix_loc, 1, GL_TRUE, shader_uniform_pr_matrix.array);
    glUniformMatrix4fv(quad_shader_ml_matrix_loc, 1, GL_TRUE, shader_uniform_ml_matrix.array);
    glUniform1iv(quad_shader_samplers_loc, 32, shader_uniform_samplers);
    glUseProgram(0);
}

bool check_program(u32 id, char *shader_path) {
    s32 is_linked = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &is_linked); 
    if (is_linked == GL_FALSE) {
        LOG_ERROR("Program of %s, failed to link.", shader_path);
       
        s32 info_log_length;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &info_log_length);
        char *buffer = malloc(info_log_length);
        
        s32 buffer_size;
        glGetProgramInfoLog(id, info_log_length, &buffer_size, buffer);
        (void)fprintf(stderr, "%s\n", buffer);
        
        free(buffer);

        return false;
    } 
    return true;
}

bool check_shader(u32 id, char *shader_path) {
    s32 is_compiled = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &is_compiled); 
    if (is_compiled == GL_FALSE) {
        LOG_ERROR("Shader of %s, failed to compile.", shader_path);
        
        s32 info_log_length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_log_length);
        char *buffer = malloc(info_log_length);
        
        s32 buffer_size;
        glGetShaderInfoLog(id, info_log_length, &buffer_size, buffer);
        (void)fprintf(stderr, "%s\n", buffer);
        
        free(buffer);

        return false;
    } 
    return true;
}

s32 components_of(GLenum type) {
    switch (type) {
        case GL_FLOAT: return 1;
        case GL_FLOAT_VEC2: return 2;
        case GL_FLOAT_VEC3: return 3;
        case GL_FLOAT_VEC4: return 4;
        case GL_FLOAT_MAT4: return 16;
        default: return 0;
    }
}

Shader shader_load(char *shader_path) {
    Shader shader;

    /**
     * @Important: To avoid error while compiling glsl file we need to ensure that "#version ...\n" line comes before anything else in the final shader string. 
     * That said, we can't just have "#define ...\n" come before version tag. 
     * Therefore we split original shader source on it's shader version part that consists of single "#version ...\n" line, and rest of the shader code that comes after.
     * And finally we insert "define ...\n" part in between these two substrings.
     */

    // Getting full shader file.
    String shader_source = read_file_into_str(shader_path, &std_allocator);
    
    // Splitting on two substrings, "shader_version" and "shader_code".
    s64 start_of_version_tag = str_find(shader_source, shader_version_tag);
    s64 end_of_version_tag   = str_find_char_left(
            str_substring(shader_source, start_of_version_tag, shader_source.length), '\n');

    String shader_version    = str_substring(shader_source, start_of_version_tag, end_of_version_tag + 1);
    String shader_code       = str_substring(shader_source, end_of_version_tag + 1, shader_source.length);
    
    // Passing all parts in the respective shader sources, where defines inserted between version and code parts.
    const char *vertex_shader_source[3] = { shader_version.data, vertex_shader_defines.data, shader_code.data };
    const char *fragment_shader_source[3] = { shader_version.data, fragment_shader_defines.data, shader_code.data };
    
    /**
     * Specifying lengths, because we don't pass null terminated strings.
     * @Important: Since defines length depends on whether we are loading vertex or fragment shader, we set it's length values separately when loading each.
     */
    shader_strings_lengths[0] = shader_version.length;
    shader_strings_lengths[2] = shader_code.length;
    

    shader_strings_lengths[1] = vertex_shader_defines.length;
    u32 vertex_shader;
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 3, vertex_shader_source, shader_strings_lengths);
    glCompileShader(vertex_shader);
    
    // Check results for errors.
    if (!check_shader(vertex_shader, shader_path)) {
        allocator_free(&std_allocator, shader_source.data);
        return (Shader) {0};
    }

    shader_strings_lengths[1] = fragment_shader_defines.length;
    u32 fragment_shader;
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 3, fragment_shader_source, shader_strings_lengths);
    glCompileShader(fragment_shader);
    
    // Check results for errors.
    if (!check_shader(fragment_shader, shader_path)) {
        allocator_free(&std_allocator, shader_source.data);
        return (Shader) {0};
    }
    


    shader.id = glCreateProgram();

    glAttachShader(shader.id, vertex_shader);
    glAttachShader(shader.id, fragment_shader);
    glLinkProgram(shader.id);
    
    // Check results for errors.
    if (!check_program(shader.id, shader_path)) {
        allocator_free(&std_allocator, shader_source.data);
        return (Shader) {0};
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    

    allocator_free(&std_allocator, shader_source.data);
    

    // Cache all attributes in shader based on shader location as index.
    shader.attributes_count = 0;
    shader.vertex_stride = 0;
    glGetProgramiv(shader.id, GL_ACTIVE_ATTRIBUTES, &shader.attributes_count);

    if (shader.attributes_count > MAX_ATTRIBUTES_PER_SHADER) {
        LOG_ERROR("Shader of %s, exceeded maximum attributes per shader limit on loading.", shader_path);
        return (Shader) {0};
    }
    
    Attribute attribute;
    for (s32 i = 0; i < shader.attributes_count; i++) {
        glGetActiveAttrib(shader.id, i, MAX_ATTRIBUTE_NAME_LENGTH, NULL, &attribute.length, &attribute.type, attribute.name);
        attribute.components = components_of(attribute.type);
        shader.vertex_stride += attribute.components;
        shader.attributes[glGetAttribLocation(shader.id, attribute.name)] = attribute;
    }

    

    return shader;
}

void shader_unload(Shader *shader) {
    glUseProgram(0);
    glDeleteProgram(shader->id);
    
    shader->id = 0;
    shader->vertex_stride = 0;
}


Camera camera_make(Vec2f center, s32 unit_scale) {
    return (Camera) {
        .center = center,
        .unit_scale = unit_scale,
    };
}

Matrix4f camera_calculate_projection(Camera *camera, float window_width, float window_height) {
    float camera_width_offset = (window_width / (float) camera->unit_scale) / 2;
    float camera_height_offset = (window_height / (float) camera->unit_scale) / 2;
    return matrix4f_orthographic(camera->center.x - camera_width_offset, camera->center.x + camera_width_offset, camera->center.y - camera_height_offset, camera->center.y + camera_height_offset, -1.0f, 1.0f);
}

Vec2f screen_to_camera(Vec2f screen_position, Camera *camera, float window_width, float window_height) {
    return vec2f_sum(vec2f_divide_constant(vec2f_difference(screen_position, vec2f_make(window_width / 2.0f, window_height / 2.0f)), camera->unit_scale), camera->center);
}


/**
 * @Todo: When shader loading will be more deterministic, optimize glGetUniformLocation use.
 */
void shader_update_projection(Shader *shader, Matrix4f *projection) {
    s32 quad_shader_pr_matrix_loc = glGetUniformLocation(shader->id, shader_uniform_pr_matrix_name);
    if (quad_shader_pr_matrix_loc == -1) {
        LOG_ERROR("Couldn't get location of %s uniform, in shader, when updating projection.", shader_uniform_pr_matrix_name);
    }

    // Set uniforms.
    glUseProgram(shader->id);
    glUniformMatrix4fv(quad_shader_pr_matrix_loc, 1, GL_TRUE, projection->array);
    glUseProgram(0);
}



u8 texture_ids_filled_length = 0;

float add_texture_to_slots(Texture *texture) {
    for (u8 i = 0; i < texture_ids_filled_length; i++) {
        if (texture_ids[i] == texture->id)
            return i;
    }
    if (texture_ids_filled_length < 32) {
        texture_ids[texture_ids_filled_length] = texture->id;
        texture_ids_filled_length++;
        return texture_ids_filled_length - 1;
    }
    LOG_ERROR("Overflow of 32 texture slots limit, can't add texture id: %d, to current draw call texture slots.", texture->id);
    return -1.0f;
}




// @Incomplete: Finish attributes pointers setting for different types of shaders and strides.
void drawer_init(Quad_Drawer *drawer, Shader *shader) {
    drawer->program = shader;

    // Setting Vertex Objects for render using OpenGL. Also seeting up Element Buffer Object for indices to load.
    glGenVertexArrays(1, &drawer->vao);
    glGenBuffers(1, &drawer->vbo);
    glGenBuffers(1, &drawer->ebo);

    // 1. Bind Vertex Array Object. [VAO]
    glBindVertexArray(drawer->vao);
    
    // 2. Copy verticies array in a buffer for OpenGL to use. [VBO].
    glBindBuffer(GL_ARRAY_BUFFER, drawer->vbo);
    glBufferData(GL_ARRAY_BUFFER, drawer->program->vertex_stride * VERTICIES_PER_QUAD * MAX_QUADS_PER_BATCH * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // 3. Copy indicies array in a buffer for OpenGL to use. [EBO].
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, array_list_length(&quad_indicies) * sizeof(float), quad_indicies, GL_STATIC_DRAW);
    
    // 3. Set vertex attributes pointers. [VAO, VBO, EBO]. @Old.
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)0);
    // glEnableVertexAttribArray(0);

    // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)(3 * sizeof(float)));
    // glEnableVertexAttribArray(1);
    // 
    // glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)(7 * sizeof(float)));
    // glEnableVertexAttribArray(2);
    // 
    // glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)(9 * sizeof(float)));
    // glEnableVertexAttribArray(3);
    // 
    // glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)(10 * sizeof(float)));
    // glEnableVertexAttribArray(4);

    // 3. Set vertex attributes pointers. [VAO, VBO, EBO].
    u64 offset = 0;
    for (s32 i = 0; i < shader->attributes_count; i++) {
        glVertexAttribPointer(i, shader->attributes[i].components, ATTRIBUTE_COMPONENT_TYPE, GL_FALSE, drawer->program->vertex_stride * ATTRIBUTE_COMPONENT_SIZE, (void*)(offset * ATTRIBUTE_COMPONENT_SIZE));
        glEnableVertexAttribArray(i);
        offset += shader->attributes[i].components; 
    }

    


    // 4. Unbind EBO, VBO and VAO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawer_free(Quad_Drawer *drawer) {
    glDeleteVertexArrays(1, &drawer->vao); 
    glDeleteBuffers(1, &drawer->vbo); 
    glDeleteBuffers(1, &drawer->ebo); 

    drawer->program = NULL;
    drawer->vao = 0;
    drawer->vbo = 0;
    drawer->ebo = 0;
}


void line_drawer_init(Line_Drawer *drawer, Shader *shader) {
    drawer->program = shader;

    // Setting Vertex Objects for render using OpenGL. Also seeting up Element Buffer Object for indices to load.
    glGenVertexArrays(1, &drawer->vao);
    glGenBuffers(1, &drawer->vbo);

    // 1. Bind Vertex Array Object. [VAO]
    glBindVertexArray(drawer->vao);
    
    // 2. Copy verticies array in a buffer for OpenGL to use. [VBO].
    glBindBuffer(GL_ARRAY_BUFFER, drawer->vbo);
    // printf("max: %d\n", drawer->program->vertex_stride * VERTICIES_PER_QUAD * MAX_QUADS_PER_BATCH);
    glBufferData(GL_ARRAY_BUFFER, drawer->program->vertex_stride * VERTICIES_PER_LINE * MAX_LINES_PER_BATCH * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // 3. Set vertex attributes pointers. [VAO, VBO].
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, drawer->program->vertex_stride * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 4. Unbind EBO, VBO and VAO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void line_drawer_free(Line_Drawer *drawer) {
    glDeleteVertexArrays(1, &drawer->vao); 
    glDeleteBuffers(1, &drawer->vbo); 

    drawer->program = NULL;
    drawer->vao = 0;
    drawer->vbo = 0;
}



Vertex_Buffer vertex_buffer_make() {
    return array_list_make(float, MAX_QUADS_PER_BATCH * VERTICIES_PER_QUAD * 11, &std_allocator);
}

void vertex_buffer_free(Vertex_Buffer *buffer) {
    array_list_free(buffer);
}

void vertex_buffer_append_data(Vertex_Buffer *buffer, float *vertex_data, u32 length) {
    (void)array_list_append_multiple(buffer, vertex_data, length);
}

void vertex_buffer_draw_quads(Vertex_Buffer *buffer, Quad_Drawer *drawer) {
    u32 length = array_list_length(buffer);

    // Bind buffers, program, textures.
    glUseProgram(drawer->program->id);

    for (u8 i = 0; i < 32; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, texture_ids[i]);
    }

    glBindVertexArray(drawer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, drawer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawer->ebo);

    u32 batch_stride = MAX_QUADS_PER_BATCH * VERTICIES_PER_QUAD * drawer->program->vertex_stride;
    // Spliting all data on equal batches, and processing each batch in each draw call.
    u32 batches = length / batch_stride;
    for (u32 i = 0; i < batches; i++) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, batch_stride * sizeof(float), *buffer + (i * batch_stride));
        glDrawElements(GL_TRIANGLES, array_list_length(&quad_indicies), GL_UNSIGNED_INT, 0);
    }
    
    // Data that didn't group into full batch, rendered in last draw call.
    u32 float_attributes_left = (length - batch_stride * batches);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_attributes_left * sizeof(float), *buffer + (batches * batch_stride));
    glDrawElements(GL_TRIANGLES, float_attributes_left / drawer->program->vertex_stride / VERTICIES_PER_QUAD * INDICIES_PER_QUAD, GL_UNSIGNED_INT, 0);

    // Unbinding of buffers after use.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Unbind texture ids.
    for (u8 i = 0; i < 32; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    texture_ids_filled_length = 0;

    glUseProgram(0);
}

void vertex_buffer_draw_lines(Vertex_Buffer *buffer, Line_Drawer *drawer) {
    u32 length = array_list_length(buffer);

    // Bind buffers, program, textures.
    glUseProgram(drawer->program->id);

    glBindVertexArray(drawer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, drawer->vbo);


    u32 batch_stride = MAX_LINES_PER_BATCH * VERTICIES_PER_LINE * drawer->program->vertex_stride;
    // Spliting all data on equal batches, and processing each batch in each draw call.
    u32 batches = length / batch_stride;
    for (u32 i = 0; i < batches; i++) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, batch_stride * sizeof(float), *buffer + (i * batch_stride));
        glDrawArrays(GL_LINES, 0, MAX_LINES_PER_BATCH * VERTICIES_PER_LINE);
    }
    
    // Data that didn't group into full batch, rendered in last draw call.
    u32 float_attributes_left = (length - batch_stride * batches);
    glBufferSubData(GL_ARRAY_BUFFER, 0, float_attributes_left * sizeof(float), *buffer + (batches * batch_stride));
    glDrawArrays(GL_LINES, 0, float_attributes_left / drawer->program->vertex_stride);


    // Unbinding of buffers after use.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    glUseProgram(0);
}

void vertex_buffer_clear(Vertex_Buffer *buffer) {
    array_list_clear(buffer);
}


Quad_Drawer *active_drawer = NULL;

void draw_begin(Quad_Drawer* drawer) {
    active_drawer = drawer;
}

void draw_end() {
    vertex_buffer_draw_quads(&verticies, active_drawer);

    // Clean up.
    active_drawer = NULL;
    array_list_clear(&verticies);
}

void draw_quad_data(float *quad_data, u32 count) {
    vertex_buffer_append_data(&verticies, quad_data, count * VERTICIES_PER_QUAD * active_drawer->program->vertex_stride);
}






Line_Drawer *active_line_drawer = NULL;

void line_draw_begin(Line_Drawer* drawer) {
    active_line_drawer = drawer;
}

void line_draw_end() {
    vertex_buffer_draw_lines(&verticies, active_line_drawer);

    // Clean up.
    active_line_drawer = NULL;
    array_list_clear(&verticies);
}

void draw_line_data(float *line_data, u32 count) {
    vertex_buffer_append_data(&verticies, line_data, count * VERTICIES_PER_LINE * active_line_drawer->program->vertex_stride);
}



void print_verticies() {
    u32 stride = 1;
    if (active_drawer != NULL) {
        stride = active_drawer->program->vertex_stride;
    }
    if (active_line_drawer != NULL) {
        stride = active_line_drawer->program->vertex_stride;
    }

    (void)printf("\n---------- VERTICIES -----------\n");
    u32 length = array_list_length(&verticies);
    if (length == 0) {
        (void)printf("[ ]\n");
    }
    else {
        (void)printf("[ ");
        for (u32 i = 0; i < length - 1; i++) {
            (void)printf("%6.1f, ", verticies[i]);
            if ((i + 1) % stride == 0)
                (void)printf("\n  ");
        }
        (void)printf("%6.1f  ]\n", verticies[length - 1]);
    }

   (void)printf("Length   : %8d\n", length);
   (void)printf("Capacity : %8d\n\n", array_list_capacity(&verticies));
    
}

void print_indicies() {
    (void)printf("\n----------- INDICIES -----------\n");
    (void)printf("[\n\n  ");
    u32 length = array_list_length(&quad_indicies);
    for (u32 i = 0; i < length; i++) {
        if ((i + 1) % 6 == 0)
            (void)printf("%4d\n\n  ", quad_indicies[i]);
        else if ((i + 1) % 3 == 0)
            (void)printf("%4d\n  ", quad_indicies[i]);
        else
            (void)printf("%4d, ", quad_indicies[i]);
    }
   (void)printf("\r]\n");
   (void)printf("Length   : %8d\n", length);
   (void)printf("Capacity : %8d\n\n", array_list_capacity(&quad_indicies));
}





Font_Baked font_bake(u8 *font_data, float font_size) {

    Font_Baked result;
    
    // Init font info.
    stbtt_fontinfo info;
    (void)stbtt_InitFont(&info, font_data, 0);

    // Initing needed variables. @Temporary: Maybe move some of this variables into font struct directly and calculate when baking bitmap.
    s32 ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

    float scale = stbtt_ScaleForPixelHeight(&info, font_size);
    result.line_height = (s32)((float)(ascent - descent + line_gap) * scale);
    result.baseline = (s32)((float)ascent * -scale);
    result.line_gap = (s32)((float)line_gap * scale);

    // Create a bitmap.
    result.bitmap.width    = 512;
    result.bitmap.height   = 512;
    u8 *bitmap = calloc(result.bitmap.width * result.bitmap.height, sizeof(u8));

    // Bake the font into the bitmap.
    result.first_char_code  = 32; // ASCII value of the first character to bake.
    result.chars_count      = 96;  // Number of characters to bake.
                         
    result.chars = malloc(result.chars_count * sizeof(stbtt_bakedchar));
    (void)stbtt_BakeFontBitmap(font_data, 0, font_size, bitmap, result.bitmap.width, result.bitmap.height, result.first_char_code, result.chars_count, result.chars);

    // Create an OpenGL texture.
    glGenTextures(1, &result.bitmap.id);
    
    glBindTexture(GL_TEXTURE_2D, result.bitmap.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_s);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_max_filter);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, result.bitmap.width, result.bitmap.height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(bitmap);

    return result;
}

void font_free(Font_Baked *font) {
    free(font->chars);
    font->line_height = 0;
    font->baseline = 0;
    font->first_char_code = 0;
    font->chars_count = 0;
    texture_unload(&font->bitmap);
}
