#ifndef VD_CONE_
#define VD_CONE_

#include <GLES3/gl3.h>

#include <array>

#include "Counter.hpp"
#include "DrawObject.hpp"
#include "GeometricTransform.hpp"
#include "Vec.hpp"

class Cone : public Counter<Cone>, public DrawObject<Cone> {
   public:
    using val_type = float;

    Cone(const Vec<3, val_type>& tip,
         const Vec<3, val_type>& color,
         val_type rx,
         val_type ry,
         val_type h);
    Cone(Cone&&);
    ~Cone();

    // implementations

    static void init_impl();
    static void set_vertex_format_impl();
    static void draw_impl();

   private:
    static constexpr std::size_t N = 96;
    static constexpr GLsizeiptr vertex_cap = (3 * (N + 2)) * sizeof(val_type);
    static std::array<val_type, 3 * (N + 2)> vec;

    // Vec<3, val_type> tip_;
    // Vec<3, val_type> color_;
    // val_type rx_, ry_, h_;
};

#endif  // VD_CONE_
