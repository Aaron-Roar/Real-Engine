#include <math.h>
#include "math2d.h"
#include <float.h>

#include <float.h>

AABB math_aabb_create(Shape world_shape) {
    AABB aabb = {
        .min_x =  FLT_MAX,
        .max_x = -FLT_MAX,
        .min_y =  FLT_MAX,
        .max_y = -FLT_MAX
    };

    if(world_shape.amount_of_vertices <= 0) {
        return (AABB){0};
    }

    for(int i = 0; i < world_shape.amount_of_vertices; i++) {
        Vec2D vertex = world_shape.vertices[i];

        if(vertex.x < aabb.min_x) {
            aabb.min_x = vertex.x;
        }

        if(vertex.x > aabb.max_x) {
            aabb.max_x = vertex.x;
        }

        if(vertex.y < aabb.min_y) {
            aabb.min_y = vertex.y;
        }

        if(vertex.y > aabb.max_y) {
            aabb.max_y = vertex.y;
        }
    }

    return aabb;
}
float math_aabb_width(AABB aabb) {
    return aabb.max_x - aabb.min_x;
}

float math_aabb_height(AABB aabb) {
    return aabb.max_y - aabb.min_y;
}

Vec2D math_aabb_center(AABB aabb) {
    return (Vec2D){
        .x = (aabb.min_x + aabb.max_x) * 0.5f,
        .y = (aabb.min_y + aabb.max_y) * 0.5f
    };
}
bool math_aabb_overlap(AABB a, AABB b) {
    return
        a.min_x <= b.max_x &&
        a.max_x >= b.min_x &&
        a.min_y <= b.max_y &&
        a.max_y >= b.min_y;
}

float math_list_maximum(Vec1DList list) {
    float max_value = list.vectors[0];
    for(int i = 0; i < list.amount_of_vectors; i += 1) {
        if( list.vectors[i] > max_value)
            max_value = list.vectors[i];
    }
    return max_value;
}

float math_list_minimum(Vec1DList list) {
    float min_value = list.vectors[0];
    for(int i = 0; i < list.amount_of_vectors; i += 1) {
        if( list.vectors[i] < min_value)
            min_value = list.vectors[i];
    }
    return min_value;
}

Vec2DList math_normals_create(Shape shape) {
    Vec2DList normals = {0};
    if (shape.amount_of_vertices <= 1 || shape.amount_of_vertices > MAX_VECTORS) {
        //Error
        return normals;
    }

    normals.amount_of_vectors = shape.amount_of_vertices;

    for(int i = 0; i < shape.amount_of_vertices; i++) {
        int j = (i + 1)%shape.amount_of_vertices;  //makes the last vertex wrap to the first one

        normals.vectors[i].y = -(shape.vertices[j].x - shape.vertices[i].x);
        normals.vectors[i].x = shape.vertices[j].y - shape.vertices[i].y;
    }

    return normals;
}

Vec2D math_vector_normalize(Vec2D vector) {
    return (Vec2D){
        .x = vector.x/sqrt(vector.x*vector.x + vector.y*vector.y),
        .y = vector.y/sqrt(vector.x*vector.x + vector.y*vector.y),
    };
}

Vec2DList math_vectors_normalize(Vec2DList vectors) {
    Vec2DList normalized_vecs = {0};
    normalized_vecs.amount_of_vectors = vectors.amount_of_vectors;
    for(int i = 0; i < vectors.amount_of_vectors; i += 1) {
        normalized_vecs.vectors[i] = math_vector_normalize(vectors.vectors[i]);
    }
    return normalized_vecs;
}

float math_dot_product(Vec2D vector_1, Vec2D vector_2) {
    return vector_1.x*vector_2.x + vector_1.y*vector_2.y;
}

Shape math_square_create(float width, float height) {
    Shape shape = {
        .amount_of_vertices = 4,
        .vertices = {
            {.x = width/2, .y = height/2},
            {.x = width/2, .y = -height/2},
            {.x = -width/2, .y = -height/2},
            {.x = -width/2, .y = height/2},
        }
    };
    return shape;
}

Shape math_circle_create(float radius, uint8_t verticies) {
    Shape shape = {0};
    if(verticies < MIN_VERTICIES) {
        shape.amount_of_vertices = MIN_VERTICIES;
    }
    else if(verticies > MAX_VERTICIES) {
        shape.amount_of_vertices = MAX_VERTICIES;
    }
    else {
        shape.amount_of_vertices = verticies;
    }

    float angle_increment = ((float)2*PI_F)/((float)shape.amount_of_vertices);
    for(int i = 0; i < shape.amount_of_vertices; i++) {
        shape.vertices[i].x = cosf(angle_increment*i)*radius;
        shape.vertices[i].y = sinf(angle_increment*i)*radius;
    }
    return shape;
}

Projection math_project_shape_on_axis(Shape shape, Axis axis) {
    Vec1DList list = {0};
    list.amount_of_vectors = shape.amount_of_vertices;
    for(int i = 0; i < shape.amount_of_vertices; i += 1) {
        list.vectors[i] = math_dot_product(shape.vertices[i], axis);
    }
    return (Projection){
        .max = math_list_maximum(list),
        .min = math_list_minimum(list)
    };
}



float math_projection_overlap(Projection projection_1, Projection projection_2) {
    return fminf(projection_1.max, projection_2.max) - fmaxf(projection_1.min, projection_2.min);
}

bool math_shape_overlap_on_axes(Shape shape_1, Shape shape_2, Vec2DList axes) {
    for (int i = 0; i < axes.amount_of_vectors; i += 1) {
        Axis axis = axes.vectors[i];

        Projection p1 = math_project_shape_on_axis(shape_1, axis);
        Projection p2 = math_project_shape_on_axis(shape_2, axis);

        float overlap = math_projection_overlap(p1, p2);

        if (overlap <= 0.0f) {
            return false;
        }
    }

    return true;
}

bool math_shape_overlap(Shape shape_1, Shape shape_2) {
    Vec2DList shape1_normals = math_vectors_normalize(math_normals_create(shape_1));
    Vec2DList shape2_normals = math_vectors_normalize(math_normals_create(shape_2));

    if (!math_shape_overlap_on_axes(shape_1, shape_2, shape1_normals)) {
        return false;
    }

    if (!math_shape_overlap_on_axes(shape_1, shape_2, shape2_normals)) {
        return false;
    }

    return true;
}

float math_cross_2d(Vec2D a, Vec2D b) {
    return a.x * b.y - a.y * b.x;
}

Vec2D math_angular_velocity_cross_vec(float omega, Vec2D r) {
    return (Vec2D){
        .x = -omega * r.y,
        .y =  omega * r.x
    };
}

Vec2D math_project_onto_axis(Vec2D v, Axis axis) {
    float amount = math_dot_product(v, axis);

    return (Vec2D){
        .x = axis.x * amount,
        .y = axis.y * amount
    };
}

float math_vector_magnitude(Vec2D vector) {
    return sqrtf(vector.x * vector.x + vector.y * vector.y);
}

float math_axis_magnitude(Axis axis) {
    return sqrtf(axis.x * axis.x + axis.y * axis.y);
}

Vec2D math_vector_rotate(Vec2D vector, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);

    return (Vec2D){
        .x = vector.x * c - vector.y * s,
        .y = vector.x * s + vector.y * c
    };
}

Vec2D math_polygon_centroid(Shape shape) {
    double area_sum = 0.0;
    double cx_sum = 0.0;
    double cy_sum = 0.0;

    for (int i = 0; i < shape.amount_of_vertices; i++) {
        int j = (i + 1) % shape.amount_of_vertices;

        double xi = shape.vertices[i].x;
        double yi = shape.vertices[i].y;
        double xj = shape.vertices[j].x;
        double yj = shape.vertices[j].y;

        double cross = xi * yj - xj * yi;

        area_sum += cross;
        cx_sum += (xi + xj) * cross;
        cy_sum += (yi + yj) * cross;
    }

    double area = area_sum * 0.5;

    if (fabs(area) < 1e-8) {
        // Degenerate polygon fallback: average vertices
        Vec2D avg = {0};

        for (int i = 0; i < shape.amount_of_vertices; i++) {
            avg.x += shape.vertices[i].x;
            avg.y += shape.vertices[i].y;
        }

        avg.x /= shape.amount_of_vertices;
        avg.y /= shape.amount_of_vertices;

        return avg;
    }

    Vec2D centroid = {
        .x = cx_sum / (6.0 * area),
        .y = cy_sum / (6.0 * area),
    };

    return centroid;
}

Vec1D math_circle_radius(Shape circle, Vec2D centroid) {
  return sqrt(
      (circle.vertices[0].x - centroid.x)*(circle.vertices[0].x - centroid.x)
      + (circle.vertices[0].y - centroid.y)*(circle.vertices[0].y - centroid.y)
      );
}

Vec2D math_vector_subtract(Vec2D vector_a, Vec2D vector_b) {
  return (Vec2D){.x = (vector_a.x - vector_b.x), .y = (vector_a.y - vector_b.y)};
}

Vec1D math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2) {
  return (radius_1 + radius_2) - sqrt(
      (centroid_1.x - centroid_2.x)*(centroid_1.x - centroid_2.x)
      +(centroid_1.y - centroid_2.y)*(centroid_1.y - centroid_2.y)
      );
}

Shape math_shape_x_scale(Shape shape, float scale) {
    Shape scaled_shape = shape;

    for (uint16_t i = 0; i < scaled_shape.amount_of_vertices; i++) {
        scaled_shape.vertices[i].x *= scale;
    }

    return scaled_shape;
}
Shape math_shape_y_scale(Shape shape, float scale) {
    Shape scaled_shape = shape;

    for (uint16_t i = 0; i < scaled_shape.amount_of_vertices; i++) {
        scaled_shape.vertices[i].y *= scale;
    }

    return scaled_shape;
}
Shape math_shape_scale(Shape shape, float scale) {
    Shape scaled_shape = shape;

    for (uint16_t i = 0; i < scaled_shape.amount_of_vertices; i++) {
        scaled_shape.vertices[i].x *= scale;
        scaled_shape.vertices[i].y *= scale;
    }

    return scaled_shape;
}

Shape math_vertex_add(Shape shape) {
    uint16_t amount_of_vertices = shape.amount_of_vertices + 1;
    if(amount_of_vertices >= MAX_VERTICIES) {
        return shape;
    }
    if(amount_of_vertices < 3) {
        return shape;
    }

    float radius = math_circle_radius(shape, math_polygon_centroid(shape));
    if(amount_of_vertices == 3) {
        return math_circle_create(radius, 3);
    }
    if(amount_of_vertices == 4) {
        return math_square_create(radius, radius);
    }
    return math_circle_create(radius, amount_of_vertices);
}

Shape math_vertex_delete(Shape shape) {
    uint16_t amount_of_vertices = shape.amount_of_vertices - 1;
    if(amount_of_vertices >= MAX_VERTICIES) {
        return shape;
    }
    if(amount_of_vertices < 3) {
        return shape;
    }

    float radius = math_circle_radius(shape, math_polygon_centroid(shape));
    if(amount_of_vertices == 3) {
        return math_circle_create(radius, 3);
    }
    if(amount_of_vertices == 4) {
        return math_square_create(radius, radius);
    }
    return math_circle_create(radius, amount_of_vertices);
}
