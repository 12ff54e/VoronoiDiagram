#ifndef VD_SHADER_
#define VD_SHADER_

#include <GLES3/gl3.h>
#include <iostream>
#include <string>

/**
 * @brief The shader class is essentially a pointer to program in OpenGL
 *
 */
class ShaderProgram {
   private:
    GLuint id;

    static auto compile(GLenum, const std::string&);
    static auto err_check(GLuint, int, const std::string&);

   public:
    ShaderProgram(const std::string&, const std::string&);
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = default;
    ~ShaderProgram();

    void use();

    GLuint get_id() const { return id; }
    operator GLuint() const { return id; }
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
