#include <emscripten/html5.h>  // for H5 event handling
#include <webgl/webgl2.h>      // for all the gl* staff
#include <fstream>             // std::ifstream
#include <functional>          // std::bind
#include <iostream>            // std::cout
#include <memory>              // std::unique_ptr
#include <random>  // std::random_device, std::mt19937, std::uniform_real_distribution
#include <vector>  // std::vector
#include "Shader.hpp"
#include "Vec.hpp"

#define canvas "#canvas"  // canvas query selector
constexpr int canvas_width = 1920;
constexpr int canvas_height = 1080;

// vertex array buffer
static GLuint vao;
// uniform buffer id
static GLuint ubo;
// location of uniform var site_array_size
static GLint site_array_size_loc;
static GLint style_loc;
static GLint line_width_loc;
static GLint line_color_loc;

static GLuint style;
static int line_width = 1;

struct Site {
    Vec<2, float> pos;
    Vec<3, float> color;

    static constexpr std::size_t std140_padded_size = sizeof(float) * 8;
};

// constexpr std::size_t SITE_NUM = 1024;

#define exec_and_check(func, ...)                             \
    do {                                                      \
        if (func(__VA_ARGS__) != EMSCRIPTEN_RESULT_SUCCESS) { \
            std::cout << "Failed to invoke" #func << '\n';    \
            return 1;                                         \
        }                                                     \
    } while (false)

static float get_random() {
    static std::mt19937 gen{std::random_device{}()};
    static std::uniform_real_distribution<float> real_dist(0., 1.);

    return real_dist(gen);
}

inline static auto random_color() {
    return Vec<3, float>{get_random(), get_random(), get_random()};
};

inline static auto& get_sites() {
    static std::vector<Site> sites;
    return sites;
}

/**
 * @brief Normalize coordinates in canvas pixels to x:(-ar/2, ar/2), y:(-.5,
 * .5), and flip y coordinate.
 *
 */
inline static auto& normalize_coord(Vec<2, float>& coord) {
    constexpr float ar =
        static_cast<float>(canvas_width) / static_cast<float>(canvas_height);
    coord.x() = (coord.x() / static_cast<float>(canvas_width) - .5f) * ar;
    coord.y() = .5f - coord.y() / static_cast<float>(canvas_height);
    return coord;
}

static auto& get_random_sites(std::size_t site_num) {
    auto& sites = get_sites();
    sites.clear();
    sites.reserve(site_num);

    constexpr float ar =
        static_cast<float>(canvas_width) / static_cast<float>(canvas_height);
    static auto random_pos = []() {
        return Vec<2, float>{(get_random() - .5f) * ar, get_random() - .5f};
    };

    for (std::size_t i = 0; i < site_num; ++i) {
        sites.emplace_back(random_pos(), random_color());
    }
    return sites;
}

static auto& get_drawing_shader(const std::string& v = "",
                                const std::string& f = "") {
    static ShaderProgram program(v, f);
    return program;
}

// main loop
static void draw() {
    // set background color to grey
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // generate random sites
    auto& current_sites = get_sites();

    // pad every site to two vec4
    std::size_t padded_data_size =
        current_sites.size() * Site::std140_padded_size / sizeof(float);
    auto ptr = std::make_unique<float[]>(padded_data_size);

    for (std::size_t i = 0; i < current_sites.size(); ++i) {
        ptr[8 * i] = current_sites[i].pos.x();
        ptr[8 * i + 1] = current_sites[i].pos.y();

        ptr[8 * i + 4] = current_sites[i].color.x();
        ptr[8 * i + 5] = current_sites[i].color.y();
        ptr[8 * i + 6] = current_sites[i].color.z();
    }

    auto& shader = get_drawing_shader();
    shader.set_uniform_value<GLuint>("site_array_size", current_sites.size());
    shader.set_uniform_value<GLuint>("style", style);
    shader.set_uniform_value<GLfloat>(
        "line_width",
        static_cast<float>(line_width) / static_cast<float>(canvas_height));
    shader.set_uniform_value<GLfloat>("line_color", 0, 0, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    static_cast<GLsizeiptr>(sizeof(float) * padded_data_size),
                    ptr.get());

    // Draw the full-screen quad, to let OpenGL invoke fragment shader
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

extern "C" {
void draw_some_sites(std::size_t, bool, bool, bool, int);
void draw_some_sites(std::size_t num = get_sites().size(),
                     bool draw_site = true,
                     bool draw_frame = false,
                     bool keep_sites = false,
                     int line_width_ = -1) {
    if (!keep_sites) { get_random_sites(num); }
    style = unsigned(draw_site) + (unsigned(draw_frame) << 1) +
            (draw_frame ? 0u : 4u);
    if (line_width_ > 0) { line_width = line_width_; }
    draw();
}
}

int main() {
    EmscriptenWebGLContextAttributes webgl_context_attr;
    emscripten_webgl_init_context_attributes(&webgl_context_attr);
    webgl_context_attr.majorVersion = 2;
    webgl_context_attr.minorVersion = 0;
    webgl_context_attr.antialias = EM_TRUE;
    webgl_context_attr.preserveDrawingBuffer = EM_TRUE;
    exec_and_check(emscripten_set_canvas_element_size, canvas, canvas_width,
                   canvas_height);

    auto webgl_context =
        emscripten_webgl_create_context(canvas, &webgl_context_attr);

    exec_and_check(emscripten_webgl_make_context_current, webgl_context);
    std::cout << glGetString(GL_VERSION) << '\n';

    // Create shader program

    auto read_shader_source = [](const std::string& path) {
        std::fstream fs(path);
        if (!fs.is_open()) {
            std::cout << "Can not open shader source file: " << path << '\n';
            throw std::runtime_error("Shader file not found!");
        }
        return std::string{(std::istreambuf_iterator<char>(fs)),
                           std::istreambuf_iterator<char>()};
    };

    auto& drawing_shader =
        get_drawing_shader(read_shader_source("shader/simple_2D.vert"),
                           read_shader_source("shader/drawing.frag"));
    std::cout << "Shader compilation success.\n";
    drawing_shader.use();

    // pass canvas dimension to shader as uniform
    drawing_shader.set_uniform_value<GLfloat>("canvas_size", canvas_width,
                                              canvas_height);
#define GET_LOC(var) var##_loc = glGetUniformLocation(drawing_shader, #var)
    GET_LOC(site_array_size);
    GET_LOC(style);
    GET_LOC(line_width);
    GET_LOC(line_color);

    constexpr unsigned MAX_ARRAY_SIZE = 4096;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, MAX_ARRAY_SIZE * Site::std140_padded_size,
                 nullptr, GL_DYNAMIC_DRAW);

    // bind ubo to shader program
    GLuint blockIndex = glGetUniformBlockIndex(drawing_shader, "user_data");
    GLuint bindingPoint = 0;
    glUniformBlockBinding(drawing_shader, blockIndex, bindingPoint);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);

    // Prepare data for the full-screen quad
    {
        // x, y, r, g, b for point at upper-left, upper-right,
        // lower-right, lower-left.
        GLfloat positions_color[] = {
            -1.f, 1.f,  .9f, .7f, .4f, 1.f, 1.f,  .8f, .7f, 1.f,
            -1.f, -1.f, .5f, 1.f, .2f, 1.f, -1.f, .9f, .7f, .4f,
        };
        constexpr unsigned pos_size = 2;
        constexpr unsigned color_size = 3;
        constexpr unsigned vertex_size = pos_size + color_size;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(positions_color), positions_color,
                     GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, pos_size, GL_FLOAT, GL_FALSE,
                              sizeof(GLfloat) * vertex_size, nullptr);
        glVertexAttribPointer(
            1, color_size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * vertex_size,
            reinterpret_cast<void*>(sizeof(GLfloat) * pos_size));
    }

    auto handle_key = [](int, const EmscriptenKeyboardEvent* key_event, void*) {
        if (key_event->code == std::string{"KeyF"}) {
            get_random_sites(get_sites().size());
            draw();
            return EM_TRUE;
        }
        return EM_FALSE;
    };
    auto handle_mouse_click = [](int, const EmscriptenMouseEvent* mouse_event,
                                 void*) {
        Vec<2, float> pos(mouse_event->targetX, mouse_event->targetY);
        get_sites().emplace_back(normalize_coord(pos), random_color());
        draw();
        return EM_TRUE;
    };
    emscripten_set_keydown_callback("body", nullptr, false, handle_key);
    emscripten_set_click_callback(canvas, nullptr, false, handle_mouse_click);

    std::cout << "Press F to get new random sites.\n";

    return 0;
}
