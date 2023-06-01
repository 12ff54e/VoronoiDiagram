#ifndef VD_SHADER_
#define VD_SHADER_

#include <GLES3/gl3.h>
#include <iostream>
#include <string>
#include <unordered_map>

/**
 * @brief The shader class is essentially a pointer to program in OpenGL
 *
 */
class ShaderProgram {
   private:
    GLuint id;
    std::unordered_map<std::string, GLint> uniforms_;

    static auto compile(GLenum, const std::string&);
    static auto err_check(GLuint, int, const std::string&);

   public:
    ShaderProgram() = delete;
    ShaderProgram(const std::string&, const std::string&);
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = default;
    ~ShaderProgram();

    void use();

    inline GLuint get_id() const { return id; }
    inline operator GLuint() const { return id; }

    template <typename T, typename... Ts>
    void set_uniform_value(const std::string& name, Ts... args) {
        auto iter = uniforms_.find(name);
        if (iter == uniforms_.end()) {
            iter =
                uniforms_.emplace(name, glGetUniformLocation(id, name.c_str()))
                    .first;
        }
        auto loc = iter->second;

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
    }
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

ShaderProgram::~ShaderProgram() {
    glDeleteProgram(id);
}

void ShaderProgram::use() {
    glUseProgram(id);
}

#endif  // VD_SHADER_
