#ifndef VD_SHADER_
#define VD_SHADER_

#include <GLES3/gl3.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>

/**
 * @brief The shader class is essentially a pointer to program in OpenGL
 *
 */
class ShaderProgram {
   private:
    template <typename T>
    using arr_type = std::array<T, 4>;
    using buffer_type =
        std::variant<arr_type<GLint>, arr_type<GLuint>, arr_type<GLfloat>>;

    GLuint id;
    std::unordered_map<std::string, std::pair<GLint, buffer_type>> uniforms_;

    static auto compile(GLenum, const std::string&);
    static auto err_check(GLuint, int, const std::string&);

   public:
    ShaderProgram() = delete;
    ShaderProgram(const std::string&, const std::string&);
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&);
    ~ShaderProgram();

    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram& operator=(ShaderProgram&&);

    void use();

    inline GLuint get_id() const { return id; }
    inline operator GLuint() const { return id; }

    template <typename T, typename... Ts>
    ShaderProgram& set_uniform_value(const std::string& name, Ts... args) {
        use();
        auto iter = uniforms_.find(name);
        arr_type<T> vals{static_cast<T>(args)...};
        try {
            if (iter == uniforms_.end()) {
                iter = uniforms_
                           .emplace(name, std::make_pair(glGetUniformLocation(
                                                             id, name.c_str()),
                                                         vals))
                           .first;
            } else if (std::get<arr_type<T>>(iter->second.second) == vals) {
                return *this;
            } else {
                iter->second.second = vals;
            }
        } catch (std::exception& e) {
            std::cout << "Uniform " << name
                      << " has different type than that in last call.\n";
            throw e;
        }
        auto loc = iter->second.first;

        // dispatch calls to glUniform{1,2,3,4}{i,ui,f} functions.
#define num_branch(n)                                            \
    if constexpr (sizeof...(Ts) == n) {                          \
        if constexpr (std::is_same_v<T, GLint>) {                \
            glUniform##n##i(loc, static_cast<GLint>(args)...);   \
        } else if constexpr (std::is_same_v<T, GLuint>) {        \
            glUniform##n##ui(loc, static_cast<GLuint>(args)...); \
        } else if constexpr (std::is_same_v<T, GLfloat>) {       \
            glUniform##n##f(loc, static_cast<GLfloat>(args)...); \
        }                                                        \
    }

        num_branch(1) else num_branch(2) else num_branch(3) else num_branch(4)

            return *this;
    }

    ShaderProgram& bind_uniform_block(const std::string& name,
                                      GLuint uniform_bloc_binding);

    ShaderProgram& bind_texture(const std::string& name, GLuint texture_index);

    static std::string read_shader_source(const std::string& path);
};

auto ShaderProgram::err_check(GLuint id, int t, const std::string& err) {
    GLint success;
    if (t == 0) {
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    } else {
        glGetProgramiv(id, GL_LINK_STATUS, &success);
    }

    if (!success) {
        char info_log[512];
        if (t == 0) {
            glGetShaderInfoLog(id, 512, nullptr, info_log);
        } else {
            glGetProgramInfoLog(id, 512, nullptr, info_log);
        }

        std::cout << err
                  << (t == 0 ? " Shader compilation failed\n"
                             : "Program linkage failed\n")
                  << info_log << '\n';
    }

    return success;
}

auto ShaderProgram::compile(GLenum type, const std::string& src) {
    auto shader = glCreateShader(type);
    auto src_code = src.c_str();
    glShaderSource(shader, 1, &src_code, nullptr);
    glCompileShader(shader);

    if (!err_check(
            shader, 0,
            std::string{type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"})) {
        throw std::runtime_error("Shader compilation failed.");
    }

    return shader;
}

ShaderProgram::ShaderProgram(const std::string& vertex_shader_src,
                             const std::string& frag_shader_src) {
    id = glCreateProgram();
    auto vertex_shader = compile(GL_VERTEX_SHADER, vertex_shader_src);
    auto frag_shader = compile(GL_FRAGMENT_SHADER, frag_shader_src);
    glAttachShader(id, vertex_shader);
    glAttachShader(id, frag_shader);
    glLinkProgram(id);

    if (!err_check(id, 1, std::string{})) {
        throw std::runtime_error("Shader linkage failed.");
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(frag_shader);
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) : id(other.id) {
    other.id = 0;  // DeleteProgram will silently ignore the value zero.
                   // (Section 2.12.3 of OpenGL ES 3.0.6)
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) {
    if (this == &other) { return *this; }
    glDeleteProgram(id);
    id = other.id;
    other.id = 0;
    return *this;
}

ShaderProgram::~ShaderProgram() {
    glDeleteProgram(id);
}

void ShaderProgram::use() {
    glUseProgram(id);
}

ShaderProgram& ShaderProgram::bind_uniform_block(const std::string& name,
                                                 GLuint uniform_bloc_binding) {
    glUniformBlockBinding(id, glGetUniformBlockIndex(id, name.c_str()),
                          uniform_bloc_binding);
    return *this;
}

ShaderProgram& ShaderProgram::bind_texture(const std::string& name,
                                           GLuint texture_index) {
    set_uniform_value<GLint>(name, texture_index);
    return *this;
}

std::string ShaderProgram::read_shader_source(const std::string& path) {
    std::ifstream fs(path);
    if (!fs.is_open()) {
        std::cout << "Can not open shader source file: " << path << '\n';
        throw std::runtime_error("Shader file not found!");
    }
    return std::string{(std::istreambuf_iterator<char>(fs)),
                       std::istreambuf_iterator<char>()};
};

#endif  // VD_SHADER_
