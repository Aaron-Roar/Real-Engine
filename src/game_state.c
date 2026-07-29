#include "game_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "entity_components.h"
#include "physics.h"
#include "yyjson.h"

typedef struct StateDocument {
    yyjson_doc *document;
    yyjson_val *entities;
} StateDocument;

static bool state_number(yyjson_val *object, const char *key, double *value) {
    yyjson_val *item = yyjson_obj_get(object, key);

    if(!yyjson_is_num(item) || value == NULL) {
        return false;
    }
    *value = yyjson_get_num(item);
    return true;
}

static bool state_boolean(yyjson_val *object, const char *key, bool *value) {
    yyjson_val *item = yyjson_obj_get(object, key);

    if(!yyjson_is_bool(item) || value == NULL) {
        return false;
    }
    *value = yyjson_get_bool(item);
    return true;
}

static bool state_vec2(yyjson_val *object, Vec2D *value) {
    double x;
    double y;

    if(!yyjson_is_obj(object) || value == NULL
            || !state_number(object, "x", &x)
            || !state_number(object, "y", &y)) {
        return false;
    }
    *value = (Vec2D){(float)x, (float)y};
    return true;
}

static EngineResult state_resolve_name(yyjson_val *value, Entity *entity) {
    EntityResult result;

    if(!yyjson_is_str(value) || entity == NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    result = entity_find_by_name(yyjson_get_str(value));
    if(result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_STATE_REFERENCE_NOT_FOUND);
    }
    *entity = result.result.value;
    return error_result_value(true);
}

static CMask state_flag_mask(const char *flag) {
    if(strcmp(flag, "static") == 0) return STATIC;
    if(strcmp(flag, "dynamic") == 0) return DYNAMIC;
    if(strcmp(flag, "collision") == 0) return COLLISION;
    if(strcmp(flag, "targetable") == 0) return TARGETABLE;
    if(strcmp(flag, "particle") == 0) return PARTICLE;
    if(strcmp(flag, "hold") == 0) return HOLD;
    return NONE;
}

static EngineResult state_load_shape(EntityIndex index, yyjson_val *value) {
    yyjson_val *vertex;
    size_t vertex_index;
    size_t vertex_count;
    Shape shape = {0};

    if(!yyjson_is_arr(value)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    vertex_count = yyjson_arr_size(value);
    if(vertex_count < MIN_VERTICIES || vertex_count > MAX_VERTICIES) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    yyjson_arr_foreach(value, vertex_index, vertex_count, vertex) {
        if(!state_vec2(vertex, &shape.vertices[vertex_index])) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
    }
    shape.amount_of_vertices = (uint16_t)vertex_count;
    if(ShapePool_store_at(&hit_boxes_pool, index, shape).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= HIT_BOX;
    return error_result_value(true);
}

static EngineResult state_load_components(Entity entity, yyjson_val *components) {
    EntityIndex index;
    yyjson_val *value;
    yyjson_val *flag;
    size_t flag_index;
    size_t flag_count;
    double number;
    Vec2D vector;
    EngineResult result;

    if(!entity_get_index(entity, &index) || !yyjson_is_obj(components)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }

    value = yyjson_obj_get(components, "mask");
    if(value != NULL) {
        if(!yyjson_is_uint(value) || yyjson_get_uint(value) > UINT32_MAX)
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        entity_mask[index] |= (CMask)yyjson_get_uint(value);
    }

    value = yyjson_obj_get(components, "flags");
    if(value != NULL) {
        if(!yyjson_is_arr(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
        yyjson_arr_foreach(value, flag_index, flag_count, flag) {
            CMask mask;
            if(!yyjson_is_str(flag)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            mask = state_flag_mask(yyjson_get_str(flag));
            if(mask == NONE) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            entity_mask[index] |= mask;
        }
    }

#define LOAD_VEC2(Key, PoolType, PoolVariable, Bit) \
    value = yyjson_obj_get(components, Key); \
    if(value != NULL) { \
        if(!state_vec2(value, &vector) \
                || PoolType##_store_at(&PoolVariable, index, vector).kind == ERROR_RESULT_ERROR) \
            return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        entity_mask[index] |= Bit; \
    }

    LOAD_VEC2("position", PositionPool, positions_pool, NONE)
    LOAD_VEC2("velocity", VelocityPool, velocities_pool, DYNAMIC)
    LOAD_VEC2("acceleration", AccelerationPool, accelerations_pool, DYNAMIC)
    LOAD_VEC2("force", ForcePool, forces_pool, FORCE)
#undef LOAD_VEC2

#define LOAD_SCALAR(Key, Pool, Table, Bit) \
    value = yyjson_obj_get(components, Key); \
    if(value != NULL) { \
        if(!yyjson_is_num(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        number = yyjson_get_num(value); \
        if(Pool##_store_at(&Table##_pool, index, (float)number).kind == ERROR_RESULT_ERROR) \
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED); \
        entity_mask[index] |= Bit; \
    }

    LOAD_SCALAR("mass", MassPool, mass, MASS)
    LOAD_SCALAR("orientation", OrientationPool, orientations, NONE)
    LOAD_SCALAR("angular_velocity", AngularVelocityPool, angular_velocities, DYNAMIC)
    LOAD_SCALAR("angular_acceleration", AngularAccelerationPool, angular_accelerations, DYNAMIC)
    LOAD_SCALAR("torque", TorquePool, torques, TORQUE)
    LOAD_SCALAR("friction", FrictionPool, frictions, NONE)
    LOAD_SCALAR("restitution", RestitutionPool, restitutions, NONE)
#undef LOAD_SCALAR

    value = yyjson_obj_get(components, "hit_box");
    if(value != NULL) {
        result = state_load_shape(index, value);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "target");
    if(value != NULL) {
        Entity target;
        result = state_resolve_name(value, &target);
        if(result.kind == ERROR_RESULT_ERROR) return result;
        if(TargetPool_store_at(&targets_pool, index, target).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= TARGETABLE;
    }

    value = yyjson_obj_get(components, "parent");
    if(value != NULL) {
        Entity parent;
        result = state_resolve_name(value, &parent);
        if(result.kind == ERROR_RESULT_ERROR) return result;
        result = entity_set_parent(entity, parent);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "lifetime");
    if(value != NULL) {
        double expiry_time = 0.0;
        uint64_t expiry_tick = 0;
        yyjson_val *tick;
        if(!yyjson_is_obj(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(yyjson_obj_get(value, "time") != NULL && !state_number(value, "time", &expiry_time))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        tick = yyjson_obj_get(value, "tick");
        if(tick != NULL) {
            if(!yyjson_is_uint(tick)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            expiry_tick = yyjson_get_uint(tick);
        }
        result = entity_set_life_time(entity, expiry_time, expiry_tick);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "angle_lock");
    if(value != NULL) {
        double min;
        double max;
        if(!yyjson_is_obj(value) || !state_number(value, "min", &min)
                || !state_number(value, "max", &max))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(AngleLockPool_store_at(&angle_locks_pool, index, (AngleLock){(float)min, (float)max}).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= ANGLE_LOCK;
    }

    value = yyjson_obj_get(components, "axis_lock");
    if(value != NULL) {
        AxisLock lock;
        if(!yyjson_is_obj(value)
                || !state_vec2(yyjson_obj_get(value, "axis"), &lock.axis)
                || !state_vec2(yyjson_obj_get(value, "point"), &lock.point_on_axis))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(AxisLockPool_store_at(&axis_locks_pool, index, lock).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= AXIS_LOCK;
    }

    value = yyjson_obj_get(components, "transform_lock");
    if(value != NULL) {
        TransformLock lock = {0};
        yyjson_val *driver = yyjson_obj_get(value, "driver");
        if(!yyjson_is_obj(value)
                || state_resolve_name(driver, &lock.driver).kind == ERROR_RESULT_ERROR
                || !state_vec2(yyjson_obj_get(value, "local_offset"), &lock.local_offset)
                || !state_number(value, "local_angle", &number)
                || !state_boolean(value, "lock_position", &lock.lock_position)
                || !state_boolean(value, "lock_orientation", &lock.lock_orientation)
                || !state_boolean(value, "inherit_velocity", &lock.inherit_velocity))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        lock.local_angle = (float)number;
        if(TransformLockPool_store_at(&transform_locks_pool, index, lock).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= TRANSFORM_LOCK;
    }

    value = yyjson_obj_get(components, "joint");
    if(value != NULL) {
        Joint joint = {0};
        yyjson_val *type = yyjson_obj_get(value, "type");
        if(!yyjson_is_obj(value) || !yyjson_is_str(type)
                || state_resolve_name(yyjson_obj_get(value, "a"), &joint.a).kind == ERROR_RESULT_ERROR
                || state_resolve_name(yyjson_obj_get(value, "b"), &joint.b).kind == ERROR_RESULT_ERROR
                || !state_vec2(yyjson_obj_get(value, "local_anchor_a"), &joint.local_anchor_a)
                || !state_vec2(yyjson_obj_get(value, "local_anchor_b"), &joint.local_anchor_b)
                || !state_number(value, "rest_length", &number))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        joint.rest_length = (float)number;
        if(strcmp(yyjson_get_str(type), "distance") == 0) joint.type = JOINT_DISTANCE;
        else if(strcmp(yyjson_get_str(type), "weld") == 0) joint.type = JOINT_WELD;
        else if(strcmp(yyjson_get_str(type), "pin") == 0) joint.type = JOINT_PIN;
        else return error_result_error(ERROR_ENGINE_STATE_INVALID);
#define JOINT_NUMBER(Key, Field) \
        if(!state_number(value, Key, &number)) return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        joint.Field = (float)number
        JOINT_NUMBER("stiffness", stiffness);
        JOINT_NUMBER("damping", damping);
        JOINT_NUMBER("rest_angle", rest_angle);
        JOINT_NUMBER("angular_stiffness", angular_stiffness);
        JOINT_NUMBER("angular_damping", angular_damping);
#undef JOINT_NUMBER
        if(!state_boolean(value, "lock_angle", &joint.lock_angle))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(JointPool_store_at(&joints_pool, index, joint).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= JOINT;
    }
    return error_result_value(true);
}

static void state_rollback(Entity *created, size_t created_count) {
    while(created_count > 0) {
        created_count -= 1;
        (void)entity_delete(created[created_count]);
    }
}

EngineResult game_state_load_files(const char *const *paths, size_t path_count) {
    StateDocument *documents;
    Entity *created;
    size_t document_index;
    size_t created_count = 0;
    size_t total_created = 0;
    EngineResult result = error_result_value(true);

    if(paths == NULL || path_count == 0 || path_count > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    documents = calloc(path_count, sizeof(*documents));
    created = calloc(MAX_ENTITIES, sizeof(*created));
    if(documents == NULL || created == NULL) {
        free(documents);
        free(created);
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *root;
        yyjson_val *version;
        yyjson_read_err read_error;
        if(paths[document_index] == NULL) {
            result = error_result_error(ERROR_ENGINE_STATE_INVALID);
            goto cleanup;
        }
        documents[document_index].document = yyjson_read_file(
            paths[document_index],
            YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS,
            NULL,
            &read_error
        );
        if(documents[document_index].document == NULL) {
            result = error_result_error(ERROR_ENGINE_STATE_IO_FAILED);
            goto cleanup;
        }
        root = yyjson_doc_get_root(documents[document_index].document);
        version = yyjson_obj_get(root, "version");
        documents[document_index].entities = yyjson_obj_get(root, "entities");
        if(!yyjson_is_obj(root) || !yyjson_is_uint(version)
                || yyjson_get_uint(version) != GAME_STATE_VERSION
                || !yyjson_is_arr(documents[document_index].entities)) {
            result = error_result_error(ERROR_ENGINE_STATE_INVALID);
            goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *description;
        size_t entity_index;
        size_t entity_count;
        yyjson_arr_foreach(documents[document_index].entities, entity_index, entity_count, description) {
            yyjson_val *name = yyjson_obj_get(description, "name");
            EntityResult entity_result;
            if(!yyjson_is_obj(description) || !yyjson_is_str(name)
                    || yyjson_get_len(name) == 0 || yyjson_get_len(name) >= ENTITY_NAME_MAX) {
                result = error_result_error(ERROR_ENGINE_STATE_INVALID);
                goto cleanup;
            }
            entity_result = entity_add();
            if(entity_result.kind == ERROR_RESULT_ERROR) {
                result = error_result_error(entity_result.result.error);
                goto cleanup;
            }
            created[created_count++] = entity_result.result.value;
            total_created = created_count;
            result = entity_set_name(entity_result.result.value, yyjson_get_str(name));
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *description;
        size_t entity_index;
        size_t entity_count;
        yyjson_arr_foreach(documents[document_index].entities, entity_index, entity_count, description) {
            yyjson_val *name = yyjson_obj_get(description, "name");
            yyjson_val *components = yyjson_obj_get(description, "components");
            EntityResult entity_result = entity_find_by_name(yyjson_get_str(name));
            if(entity_result.kind == ERROR_RESULT_ERROR || !yyjson_is_obj(components)) {
                result = error_result_error(ERROR_ENGINE_STATE_INVALID);
                goto cleanup;
            }
            result = state_load_components(entity_result.result.value, components);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

cleanup:
    if(result.kind == ERROR_RESULT_ERROR) state_rollback(created, total_created);
    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_doc_free(documents[document_index].document);
    }
    free(created);
    free(documents);
    return result;
}

EngineResult game_state_load_file(const char *path) {
    return game_state_load_files(&path, 1);
}

static yyjson_mut_val *state_write_vec2(yyjson_mut_doc *document, Vec2D value) {
    yyjson_mut_val *object = yyjson_mut_obj(document);
    yyjson_mut_obj_add_real(document, object, "x", value.x);
    yyjson_mut_obj_add_real(document, object, "y", value.y);
    return object;
}

static void state_write_named_reference(
        yyjson_mut_doc *document,
        yyjson_mut_val *components,
        const char *key,
        Entity entity
) {
    EntityNameResult name = entity_get_name(entity);
    if(name.kind == ERROR_RESULT_VALUE)
        yyjson_mut_obj_add_strcpy(document, components, key, name.result.value.value);
}

EngineResult game_state_save_file(const char *path) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *entity_array;
    uint32_t position;
    yyjson_write_err write_error;
    bool success;

    if(path == NULL) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    root = yyjson_mut_obj(document);
    entity_array = yyjson_mut_arr(document);
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "version", GAME_STATE_VERSION);
    yyjson_mut_obj_add_val(document, root, "entities", entity_array);

    for(position = 0; position < entity_alive_count(); position += 1) {
        EntityResult entity_result = entity_alive_at(position);
        EntityIndex index;
        EntityNameResult name;
        yyjson_mut_val *description;
        yyjson_mut_val *components;
        yyjson_mut_val *flags;
        CMask mask;
        size_t i;

        if(entity_result.kind == ERROR_RESULT_ERROR
                || !entity_get_index(entity_result.result.value, &index)) continue;
        name = entity_get_name(entity_result.result.value);
        if(name.kind == ERROR_RESULT_ERROR) continue;
        mask = entity_mask[index];
        description = yyjson_mut_obj(document);
        components = yyjson_mut_obj(document);
        flags = yyjson_mut_arr(document);
        yyjson_mut_obj_add_strcpy(document, description, "name", name.result.value.value);
        yyjson_mut_obj_add_val(document, description, "components", components);
        yyjson_mut_arr_add_val(entity_array, description);
        yyjson_mut_obj_add_uint(document, components, "mask", mask & ~ENTITY_NAME);
        if(mask & STATIC) yyjson_mut_arr_add_str(document, flags, "static");
        if(mask & DYNAMIC) yyjson_mut_arr_add_str(document, flags, "dynamic");
        if(mask & COLLISION) yyjson_mut_arr_add_str(document, flags, "collision");
        if(mask & TARGETABLE) yyjson_mut_arr_add_str(document, flags, "targetable");
        if(mask & PARTICLE) yyjson_mut_arr_add_str(document, flags, "particle");
        if(mask & HOLD) yyjson_mut_arr_add_str(document, flags, "hold");
        if(yyjson_mut_arr_size(flags) > 0) yyjson_mut_obj_add_val(document, components, "flags", flags);

        if(positions_pool.used[index]) yyjson_mut_obj_add_val(document, components, "position", state_write_vec2(document, positions[index]));
        if(velocities_pool.used[index]) yyjson_mut_obj_add_val(document, components, "velocity", state_write_vec2(document, velocities[index]));
        if(accelerations_pool.used[index]) yyjson_mut_obj_add_val(document, components, "acceleration", state_write_vec2(document, accelerations[index]));
        if(forces_pool.used[index]) yyjson_mut_obj_add_val(document, components, "force", state_write_vec2(document, forces[index]));
        if(mass_pool.used[index]) yyjson_mut_obj_add_real(document, components, "mass", mass[index]);
        if(orientations_pool.used[index]) yyjson_mut_obj_add_real(document, components, "orientation", orientations[index]);
        if(angular_velocities_pool.used[index]) yyjson_mut_obj_add_real(document, components, "angular_velocity", angular_velocities[index]);
        if(angular_accelerations_pool.used[index]) yyjson_mut_obj_add_real(document, components, "angular_acceleration", angular_accelerations[index]);
        if(torques_pool.used[index]) yyjson_mut_obj_add_real(document, components, "torque", torques[index]);
        if(frictions_pool.used[index]) yyjson_mut_obj_add_real(document, components, "friction", frictions[index]);
        if(restitutions_pool.used[index]) yyjson_mut_obj_add_real(document, components, "restitution", restitutions[index]);
        if(hit_boxes_pool.used[index]) {
            yyjson_mut_val *vertices = yyjson_mut_arr(document);
            for(i = 0; i < hit_boxes[index].amount_of_vertices; i += 1)
                yyjson_mut_arr_add_val(vertices, state_write_vec2(document, hit_boxes[index].vertices[i]));
            yyjson_mut_obj_add_val(document, components, "hit_box", vertices);
        }
        if(targets_pool.used[index]) state_write_named_reference(document, components, "target", targets[index]);
        if(parents_pool.used[index]) state_write_named_reference(document, components, "parent", parents[index]);
        if(life_times_pool.used[index]) {
            yyjson_mut_val *lifetime = yyjson_mut_obj(document);
            yyjson_mut_obj_add_real(document, lifetime, "time", life_times[index].expirey_time);
            yyjson_mut_obj_add_uint(document, lifetime, "tick", life_times[index].expirey_tick);
            yyjson_mut_obj_add_val(document, components, "lifetime", lifetime);
        }
        if(angle_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            yyjson_mut_obj_add_real(document, lock, "min", angle_locks[index].min);
            yyjson_mut_obj_add_real(document, lock, "max", angle_locks[index].max);
            yyjson_mut_obj_add_val(document, components, "angle_lock", lock);
        }
        if(axis_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            yyjson_mut_obj_add_val(document, lock, "axis", state_write_vec2(document, axis_locks[index].axis));
            yyjson_mut_obj_add_val(document, lock, "point", state_write_vec2(document, axis_locks[index].point_on_axis));
            yyjson_mut_obj_add_val(document, components, "axis_lock", lock);
        }
        if(transform_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            EntityNameResult driver = entity_get_name(transform_locks[index].driver);
            if(driver.kind == ERROR_RESULT_VALUE) {
                yyjson_mut_obj_add_strcpy(document, lock, "driver", driver.result.value.value);
                yyjson_mut_obj_add_val(document, lock, "local_offset", state_write_vec2(document, transform_locks[index].local_offset));
                yyjson_mut_obj_add_real(document, lock, "local_angle", transform_locks[index].local_angle);
                yyjson_mut_obj_add_bool(document, lock, "lock_position", transform_locks[index].lock_position);
                yyjson_mut_obj_add_bool(document, lock, "lock_orientation", transform_locks[index].lock_orientation);
                yyjson_mut_obj_add_bool(document, lock, "inherit_velocity", transform_locks[index].inherit_velocity);
                yyjson_mut_obj_add_val(document, components, "transform_lock", lock);
            }
        }
        if(joints_pool.used[index]) {
            EntityNameResult a = entity_get_name(joints[index].a);
            EntityNameResult b = entity_get_name(joints[index].b);
            if(a.kind == ERROR_RESULT_VALUE && b.kind == ERROR_RESULT_VALUE) {
                const char *type = joints[index].type == JOINT_DISTANCE ? "distance"
                    : joints[index].type == JOINT_WELD ? "weld" : "pin";
                yyjson_mut_val *joint = yyjson_mut_obj(document);
                yyjson_mut_obj_add_str(document, joint, "type", type);
                yyjson_mut_obj_add_strcpy(document, joint, "a", a.result.value.value);
                yyjson_mut_obj_add_strcpy(document, joint, "b", b.result.value.value);
                yyjson_mut_obj_add_val(document, joint, "local_anchor_a", state_write_vec2(document, joints[index].local_anchor_a));
                yyjson_mut_obj_add_val(document, joint, "local_anchor_b", state_write_vec2(document, joints[index].local_anchor_b));
                yyjson_mut_obj_add_real(document, joint, "rest_length", joints[index].rest_length);
                yyjson_mut_obj_add_real(document, joint, "stiffness", joints[index].stiffness);
                yyjson_mut_obj_add_real(document, joint, "damping", joints[index].damping);
                yyjson_mut_obj_add_bool(document, joint, "lock_angle", joints[index].lock_angle);
                yyjson_mut_obj_add_real(document, joint, "rest_angle", joints[index].rest_angle);
                yyjson_mut_obj_add_real(document, joint, "angular_stiffness", joints[index].angular_stiffness);
                yyjson_mut_obj_add_real(document, joint, "angular_damping", joints[index].angular_damping);
                yyjson_mut_obj_add_val(document, components, "joint", joint);
            }
        }
    }
    success = yyjson_mut_write_file(path, document, YYJSON_WRITE_PRETTY, NULL, &write_error);
    yyjson_mut_doc_free(document);
    return success ? error_result_value(true) : error_result_error(ERROR_ENGINE_STATE_IO_FAILED);
}
