#include <emscripten/html5.h>  // for H5 event handling
#include <webgl/webgl2.h>      // for all the gl* staff
#include <chrono>      // std::high_resolution_clock, std::chrono::duration
#include <functional>  // std::bind
#include <iostream>    // std::cout
#include <memory>      // std::unique_ptr
#include <random>  // std::random_device, std::mt19937, std::uniform_real_distribution
#include <vector>  // std::vector
#include "Shader.hpp"
#include "Vec.hpp"

// global constants

#define canvas "#canvas"  // canvas query selector
constexpr int canvas_width = 1920;
constexpr int canvas_height = 1080;
constexpr unsigned MAX_ARRAY_SIZE = 4096;

struct DeviceObjects {
    GLuint screen_quad_vao;
    GLuint ubo;
    GLuint fbo;
    GLuint sites_texture;
    GLuint tessellation_textures[2];
    GLuint lloyd_texture[2];
};

struct State {
    bool brute_force;
    GLuint style;
    GLuint line_width;
    unsigned frame;
    unsigned FSAA_factor;
    double timestamp;  // record timestamp of changing sites
    std::pair<std::size_t, std::size_t> update_site_range;
    std::array<std::chrono::high_resolution_clock::time_point, 10> time_points;

    bool update_sites() const {
        return update_site_range.second > update_site_range.first;
    }
    void reset_update_site_range() { update_site_range = {0, 0}; }

    double fps() {
        static std::size_t idx{};
        const auto len = time_points.size();
        const auto now = std::chrono::high_resolution_clock::now();
        auto then = std::exchange(time_points[idx], now);
        idx = (idx + 1) % len;

        return len / std::chrono::duration<double>(now - then).count();
    }
};

struct Site {
    Vec<2, float> pos;
    Vec<3, float> color;

    static constexpr std::size_t std140_padded_size = sizeof(float) * 8;
};

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
 * @brief Normalize coordinates in canvas pixels to x:(0., 1.), y:(0., 1.), and
 * flip y coordinate since the origin of mouse event coordinate is at top-left
 * corner but gl_FragCoord in fragment shader origins at bottom-left corner by
 * default.
 *
 */
inline static auto& normalize_coord(Vec<2, float>& coord) {
    coord.x() = coord.x() / static_cast<float>(canvas_width);
    coord.y() = 1.f - coord.y() / static_cast<float>(canvas_height);
    return coord;
}

static auto& get_random_sites(std::size_t site_num) {
    auto& sites = get_sites();
    sites.clear();
    sites.reserve(site_num);

    static auto random_pos = []() {
        return Vec<2, float>{get_random(), get_random()};
    };

    for (std::size_t i = 0; i < site_num; ++i) {
        sites.emplace_back(random_pos(), random_color());
    }
    return sites;
}

static auto& shader_programs() {
    static std::unordered_map<std::string, ShaderProgram> programs;
    return programs;
}
static auto& shader_programs(const std::string& name,
                             const std::string& v,
                             const std::string& f) {
    auto& programs = shader_programs();
    auto iter = programs.find(name);
    if (iter == programs.end()) {
        iter = programs.emplace(name, ShaderProgram{v, f}).first;
    } else {
        iter->second = ShaderProgram{v, f};
    }
    return iter->second;
}
static auto& shader_programs(const std::string& name) {
    return shader_programs().at(name);
}

static auto& get_device_object() {
    // Since zero in most case is the reserved value for object ID in OpenGL,
    // value initialize all the IDs prevents unintentional deletion of any
    // device object
    static DeviceObjects device_object{};
    return device_object;
}

static auto& get_state() {
    static State state{.timestamp = -1.};
    return state;
}

static auto& initial_device_objects() {
    auto& device_object = get_device_object();

    // texture storing site information
    glGenTextures(1, &device_object.sites_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI,
                 static_cast<GLsizei>(MAX_ARRAY_SIZE), 2, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_INT, nullptr);

    // ping-pong textures for JFA
    glGenTextures(2, device_object.tessellation_textures);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.tessellation_textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, 4 * canvas_width,
                 4 * canvas_height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, device_object.tessellation_textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, 4 * canvas_width,
                 4 * canvas_height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,
                 nullptr);

    // ping-pong texture for Lloyd
    glGenTextures(2, device_object.lloyd_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.lloyd_texture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI,
                 static_cast<GLsizei>(MAX_ARRAY_SIZE), 65, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_INT, nullptr);
    glBindTexture(GL_TEXTURE_2D, device_object.lloyd_texture[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI,
                 static_cast<GLsizei>(MAX_ARRAY_SIZE), 65, 0, GL_RGBA_INTEGER,
                 GL_UNSIGNED_INT, nullptr);

    // frame buffer
    glGenFramebuffers(1, &device_object.fbo);

    // Prepare data for the full-screen quad

    // x, y, r, g, b for point at upper-left, upper-right,
    // lower-right, lower-left.
    GLfloat positions_color[] = {
        -1.f, 1.f,  .9f, .7f, .4f, 1.f, 1.f,  .8f, .7f, 1.f,
        -1.f, -1.f, .5f, 1.f, .2f, 1.f, -1.f, .9f, .7f, .4f,
    };
    constexpr unsigned pos_size = 2;
    constexpr unsigned color_size = 3;
    constexpr unsigned vertex_size = pos_size + color_size;

    glGenVertexArrays(1, &device_object.screen_quad_vao);
    glBindVertexArray(device_object.screen_quad_vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions_color), positions_color,
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, pos_size, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * vertex_size, nullptr);
    glVertexAttribPointer(1, color_size, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * vertex_size,
                          reinterpret_cast<void*>(sizeof(GLfloat) * pos_size));

    return device_object;
}

/**
 * @brief Compute Voronoi diagram using JFA
 *
 * @return texture id
 */
static GLuint voronoi_tesselation() {
    auto& device_object = get_device_object();
    glBindFramebuffer(GL_FRAMEBUFFER, device_object.fbo);

    auto& state = get_state();
    int width = static_cast<int>(state.FSAA_factor) * canvas_width;
    int height = static_cast<int>(state.FSAA_factor) * canvas_height;

    auto& shader = shader_programs("JFA");
    shader.use();
    shader.set_uniform_value<GLuint>("site_num", get_sites().size())
        .set_uniform_value<GLuint>("style", state.style)
        .set_uniform_value<GLuint>("FSAA_factor", state.FSAA_factor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);

    glViewport(0, 0, width, height);

    // the extra step is for initialize texture with sites
    auto step_num = static_cast<std::size_t>(
                        std::ceil(std::log2(std::max(width, height)))) +
                    1;

    Vec<2, GLint> step_size{width, height};
    for (std::size_t i = 0; i < step_num; ++i) {
        shader.set_uniform_value<GLint>("step_size", step_size.x(),
                                        step_size.y());

        // input texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,
                      device_object.tessellation_textures[i % 2]);
        // output texture
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            device_object.tessellation_textures[1 - i % 2], 0);

        // Draw the full-screen quad, to let OpenGL invoke fragment shader
        glBindVertexArray(device_object.screen_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        step_size = (step_size + Vec<2, GLint>{1, 1}) / 2;
    }

    return device_object.tessellation_textures[step_num % 2];
}

static void lloyd(GLuint board_texture) {
    auto& device_object = get_device_object();
    glBindFramebuffer(GL_FRAMEBUFFER, device_object.fbo);

    const GLuint site_num = get_sites().size();
    GLuint patches_per_line =
        static_cast<GLuint>(std::ceil(16 / std::sqrt(site_num)));
    const GLuint patches_per_quad = patches_per_line * patches_per_line;

    auto& state = get_state();

    auto& shader = shader_programs("Lloyd");
    shader.set_uniform_value<GLuint>("site_num", site_num)
        .set_uniform_value<GLuint>("patches_per_line", patches_per_line)
        .set_uniform_value<GLuint>("patches_per_quad", patches_per_quad)
        .set_uniform_value<GLuint>("FSAA_factor", state.FSAA_factor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, board_texture);

    glViewport(0, 0, static_cast<GLsizei>(site_num),
               static_cast<GLsizei>(4 * patches_per_quad + 1));

    auto step_num =
        static_cast<GLuint>(std::ceil(std::log2(4 * patches_per_quad))) + 1;
    for (GLuint step = 0; step < step_num; ++step) {
        shader.set_uniform_value<GLuint>("reduction_step", step);

        // input texture
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, device_object.lloyd_texture[step % 2]);
        // output texture
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               device_object.lloyd_texture[1 - step % 2], 0);

        // Draw the full-screen quad, to let OpenGL invoke fragment shader
        glBindVertexArray(device_object.screen_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if constexpr (false) {
        auto pixels = std::make_unique<GLuint[]>(4);
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                     pixels.get());
        std::cout << "Average moving distance: "
                  << static_cast<GLfloat>(pixels[0]) / GLfloat{10} << "px\n";
    }

    // copy new site position into site_info
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 1,
                        static_cast<GLsizei>(site_num), 1);
}

// main loop
static void draw() {
    auto& device_object = get_device_object();
    auto& current_sites = get_sites();
    auto& state = get_state();

    // update texture storing site data
    if (state.update_sites()) {
        const auto count =
            state.update_site_range.second - state.update_site_range.first;
        auto pixels = std::make_unique<GLuint[]>(2 * 4 * count);

        for (std::size_t i = 0; i < count; ++i) {
            auto site_idx = i + state.update_site_range.first;
            pixels[4 * i + 2] = static_cast<GLuint>(
                std::round(current_sites[site_idx].pos.x() * canvas_width));
            pixels[4 * i + 3] = static_cast<GLuint>(
                std::round(current_sites[site_idx].pos.y() * canvas_height));
            pixels[count * 4 + 4 * i] = static_cast<GLuint>(
                current_sites[site_idx].color.x() * (0xffffu));
            pixels[count * 4 + 4 * i + 1] = static_cast<GLuint>(
                current_sites[site_idx].color.y() * (0xffffu));
            pixels[count * 4 + 4 * i + 2] = static_cast<GLuint>(
                current_sites[site_idx].color.z() * (0xffffu));
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        static_cast<GLsizei>(state.update_site_range.first), 0,
                        static_cast<GLsizei>(count), 2, GL_RGBA_INTEGER,
                        GL_UNSIGNED_INT, pixels.get());

        state.reset_update_site_range();
    }

    if (state.brute_force) {
        // set background color to grey
        glClearColor(0.3f, 0.3f, 0.3f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader_programs("brute_force_shader")
            .set_uniform_value<GLuint>("site_num", current_sites.size())
            .set_uniform_value<GLuint>("style", state.style)
            .set_uniform_value<GLfloat>("line_width",
                                        static_cast<float>(state.line_width) /
                                            static_cast<float>(canvas_height))
            .set_uniform_value<GLfloat>("line_color", 0, 0, 0);
    } else {
        auto board = voronoi_tesselation();
        lloyd(board);

        shader_programs("draw_texture")
            .set_uniform_value<GLuint>("site_num", current_sites.size())
            .set_uniform_value<GLuint>("style", state.style)
            .set_uniform_value<GLuint>("FSAA_factor", state.FSAA_factor);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, device_object.sites_texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, board);
        // switch render target back to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // restore viewport to canvas size
    glViewport(0, 0, canvas_width, canvas_height);

    // Draw the full-screen quad, to let OpenGL invoke fragment shader
    glBindVertexArray(device_object.screen_quad_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    EM_ASM(document.getElementById("fps").textContent = `${$0.toFixed(2)} fps`,
           state.fps());
    ++state.frame;
}

extern "C" {
/**
 * @brief Expose to web for changing state of the drawing
 *
 */
EMSCRIPTEN_KEEPALIVE void alter_state(bool,
                                      bool,
                                      bool,
                                      bool,
                                      bool,
                                      unsigned,
                                      unsigned,
                                      std::size_t,
                                      GLuint,
                                      double);
void alter_state(bool brute_force,
                 bool draw_site,
                 bool draw_frame,
                 bool periodic_x,
                 bool periodic_y,
                 unsigned FSAA,
                 unsigned metric,
                 std::size_t site_num,
                 GLuint line_width,
                 double timestamp) {
    auto& state = get_state();
    GLuint style = unsigned(draw_site) + (unsigned(draw_frame) << 1) +
                   (draw_frame ? 0u : 4u) + (unsigned(periodic_x) << 3) +
                   (unsigned(periodic_y) << 4) + (metric << 5);
    if (state.style != style || state.line_width != line_width) {
        state.style = style;
        state.line_width = line_width;
    }
    // if site num slider is dragged
    if (site_num != get_sites().size() && timestamp > state.timestamp) {
        get_random_sites(site_num);
        state.update_site_range = {0, site_num};
        state.timestamp = timestamp;
    }
    state.brute_force = brute_force;
    state.FSAA_factor = FSAA;
}
}

int main() {
    // canvas and context setup
    {
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
    }

    initial_device_objects();

    // Create shader program
    {
        shader_programs(
            "brute_force_shader",
            ShaderProgram::read_shader_source("shader/simple_2D.vert"),
            ShaderProgram::read_shader_source("shader/drawing.frag"))
            .set_uniform_value<GLfloat>("canvas_size", canvas_width,
                                        canvas_height)
            .bind_texture("site_info", 0);
        shader_programs(
            "draw_texture",
            ShaderProgram::read_shader_source("shader/simple_2D.vert"),
            ShaderProgram::read_shader_source("shader/draw_texture.frag"))
            .set_uniform_value<GLfloat>("canvas_size", canvas_width,
                                        canvas_height)
            .bind_texture("site_info", 0)
            .bind_texture("board", 1);
        shader_programs(
            "JFA", ShaderProgram::read_shader_source("shader/simple_2D.vert"),
            ShaderProgram::read_shader_source("shader/JFA.frag"))
            .set_uniform_value<GLfloat>("canvas_size", canvas_width,
                                        canvas_height)
            .bind_texture("site_info", 0)
            .bind_texture("board", 1);

        shader_programs(
            "Lloyd", ShaderProgram::read_shader_source("shader/simple_2D.vert"),
            ShaderProgram::read_shader_source("shader/Lloyd.frag"))
            .set_uniform_value<GLfloat>("canvas_size", canvas_width,
                                        canvas_height)
            .bind_texture("site_info", 0)
            .bind_texture("board", 1)
            .bind_texture("prev_sites", 2);

        GLuint sampler;
        glGenSamplers(1, &sampler);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindSampler(0, sampler);
        glBindSampler(1, sampler);
        glBindSampler(2, sampler);

        std::cout << "Shader compilation success.\n";
    }

    // handle mouse/keyboard input
    {
        using namespace std::chrono;
        static auto t = high_resolution_clock::now();

        auto handle_key = [](int, const EmscriptenKeyboardEvent* key_event,
                             void* ptr) {
            auto now = high_resolution_clock::now();
            auto& last_call_time = *static_cast<decltype(now)*>(ptr);

            if (now - last_call_time <
                std::chrono::duration<double, std::ratio<1, 24>>{1}) {
                return EM_FALSE;
            }
            if (key_event->code == std::string{"KeyF"}) {
                auto site_num = get_sites().size();
                get_random_sites(site_num);
                auto& state = get_state();
                state.update_site_range = {0, site_num};
                state.timestamp = emscripten_performance_now();
                last_call_time = now;
                return EM_TRUE;
            }
            return EM_FALSE;
        };
        auto handle_mouse_click =
            [](int, const EmscriptenMouseEvent* mouse_event, void*) {
                Vec<2, float> pos(mouse_event->targetX, mouse_event->targetY);
                auto& sites = get_sites();
                sites.emplace_back(normalize_coord(pos), random_color());
                auto& state = get_state();
                state.update_site_range = {sites.size() - 1, sites.size()};
                state.timestamp = emscripten_performance_now();
                return EM_TRUE;
            };
        emscripten_set_keydown_callback("body", &t, false, handle_key);
        emscripten_set_click_callback(canvas, nullptr, false,
                                      handle_mouse_click);

        std::cout << "Press F to get new random sites.\n";
    }

    emscripten_set_main_loop(draw, 0, EM_FALSE);
    return 0;
}

// TODO: Adjustable relaxation speed
// TODO: Hardware aware texture size
// TODO: Wrap to other 2D manifold e.g. torus, sphere, etc.
