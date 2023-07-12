#include "include/Cone.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::array<Cone::val_type, 3 * (Cone::N + 2)> Cone::vec;

void Cone::init_impl() {
    constexpr double dt = 2. * M_PI / N;
    for (unsigned i = 0; i <= N; ++i) {
        vec[3 * i + 3] = static_cast<val_type>(std::cos(i * dt));
        vec[3 * i + 4] = static_cast<val_type>(std::sin(i * dt));
        vec[3 * i + 5] = static_cast<val_type>(1);
    }

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    array_buffer_index = vertex_cap;
    array_buffer_cap = vertex_cap + 160 * sizeof(val_type);
    glBufferData(GL_ARRAY_BUFFER, array_buffer_cap, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_cap, vec.data());
}

void Cone::set_vertex_format_impl() {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 19 * sizeof(float),
                          reinterpret_cast<void*>(vertex_cap));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(1);

    for (std::size_t i = 0; i < 4; ++i) {
        glVertexAttribPointer(
            i + 2, 4, GL_FLOAT, GL_FALSE, 19 * sizeof(float),
            reinterpret_cast<void*>(vertex_cap + (3 + i * 4) * sizeof(float)));
        glVertexAttribDivisor(i + 2, 1);
        glEnableVertexAttribArray(2 + i);
    }
}

void Cone::draw_impl() {
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, vec.size() / 3,
                          static_cast<GLsizei>(get_count()));
}

Cone::Cone(const Vec<3, val_type>& tip,
           const Vec<3, val_type>& color,
           val_type rx,
           val_type ry,
           val_type h)
//: tip_(tip), color_(color), rx_(rx), ry_(ry), h_(h)
{
    auto transform =
        composite(translationTransform(tip), scalingTransform(rx, ry, h));
    val_type data[19];
    for (std::size_t i = 0; i < 3; ++i) { data[i] = color[i]; }
    for (std::size_t col = 0; col < 4; ++col) {
        for (std::size_t row = 0; row < 4; ++row) {
            data[3 + col * 4 + row] = transform(row, col);
        }
    }

    while (array_buffer_index + static_cast<GLsizeiptr>(sizeof(data)) >
           array_buffer_cap) {
        // double the size of array buffer size
        array_buffer_cap = 2 * array_buffer_cap - vertex_cap;
        grow_buffer(GL_ARRAY_BUFFER, VBO, array_buffer_cap);
    }

    glBufferSubData(GL_ARRAY_BUFFER, array_buffer_index, sizeof(data), data);
    array_buffer_index += sizeof(data);
}

Cone::Cone(Cone&&) = default;

Cone::~Cone() {
    array_buffer_index -= 19 * sizeof(val_type);
}
