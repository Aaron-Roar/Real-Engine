#include "editor_rotation_control.h"

#include <math.h>

Position editor_rotation_control_position_get(Position center,
        Orientation orientation,
        float arm_length) {
    return (Position){
        center.x + sinf(orientation) * arm_length,
        center.y - cosf(orientation) * arm_length
    };
}

bool editor_rotation_control_hit_check(Position pointer,
        Position control,
        float radius) {
    float x = pointer.x - control.x;
    float y = pointer.y - control.y;
    return x * x + y * y <= radius * radius;
}
