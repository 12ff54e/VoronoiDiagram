#ifndef VD_DRAW_OBJ
#define VD_DRAW_OBJ

#include <GLES3/gl3.h>
#include <iterator>
#include <list>
#include <unordered_map>

/**
 * @brief This template is the framework of objects that can be draw using
 * OpenGL. Classes inherit this template via CRTP should implement these
 * methods:
 * - static void init_impl()
 *      To creates buffer objects and binds them.
 * - static void set_vertex_format_impl()
 *      To tell gl the meaning of vertex data in buffer.
 * - static void draw_impl()
 *      To call gl draw command.
 */
template <typename T>
class DrawObject {
   protected:
    static inline GLuint VAO;
    static inline GLuint VBO;
    static inline GLuint EBO;
    static inline bool ready = false;
    static inline GLsizeiptr array_buffer_index;
    static inline GLsizeiptr array_buffer_cap;
    static inline GLsizeiptr element_buffer_index;
    static inline GLsizeiptr element_buffer_cap;

    /**
     * @brief Create buffer object and transfer vertex data to GL
     *
     */
    static void init() {
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        T::init_impl();

        set_vertex_format();
        ready = true;
    }
    static void set_vertex_format() {
        glBindVertexArray(VAO);
        T::set_vertex_format_impl();
    }
    static void grow_buffer(GLenum target, GLuint& buffer, GLsizeiptr to_size) {
        GLenum usage;
        GLint64 origin_size;

        glBindBuffer(target, buffer);
        glGetBufferParameteriv(target, GL_BUFFER_USAGE,
                               reinterpret_cast<GLint*>(&usage));
        glGetBufferParameteri64v(target, GL_BUFFER_SIZE, &origin_size);

        GLuint buffer_new;
        glGenBuffers(1, &buffer_new);
        glBindBuffer(target, buffer_new);
        glBufferData(target, to_size, nullptr, usage);

        glBindBuffer(GL_COPY_READ_BUFFER, buffer);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, target, 0, 0,
                            static_cast<GLsizeiptr>(origin_size));

        glDeleteBuffers(1, &buffer);
        buffer = buffer_new;

        glBindBuffer(target, buffer);
        set_vertex_format();
    }

   public:
    DrawObject() {
        if (!ready) { init(); }
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
    }
    /**
     * @brief Copy constructor is deleted since it's not very useful
     */
    DrawObject(const DrawObject&) = delete;
    DrawObject(DrawObject&&) = default;

    static void draw() {
        glBindVertexArray(VAO);
        T::draw_impl();
    }
};

#endif  // VD_DRAW_OBJ
