#ifndef ROHR_EDITOR_ROTATION_CONTROL_H
#define ROHR_EDITOR_ROTATION_CONTROL_H

#include "rohr.h"

Position editor_rotation_control_position_get(Position center,
    Orientation orientation, float arm_length);
bool editor_rotation_control_hit_check(Position pointer, Position control,
    float radius);

#endif
