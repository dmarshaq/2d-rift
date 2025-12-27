#ifndef PHYSICS_H
#define PHYSICS_H

#include "core/mathf.h"
#include "core/core.h"


typedef struct state State;

void phys_init(State *state);

#define calculate_obb_inertia(mass, width, height)                                          ((1.0f / 12.0f) * mass * (height * height + width * width))

typedef struct body_2d {
    Vec2f velocity;
    float angular_velocity;

    float mass;
    float inv_mass;
    float inertia;
    float inv_inertia;
    Vec2f mass_center;
    float restitution;
    float static_friction;
    float dynamic_friction;
} Body_2D;

static Body_2D phys_body_obb_make(OBB *obb, float mass, float restitution, float static_friction, float dynamic_friction) {
    return (Body_2D) { 
            VEC2F_ORIGIN, 
            0.0f, 
            mass, 
            (mass == 0.0f ? 0.0f : 1.0f / mass), 
            calculate_obb_inertia(mass, obb->dimensions.x, obb->dimensions.y), 
            (mass == 0.0f ? 0.0f : 1.0f / calculate_obb_inertia(mass, obb->dimensions.x, obb->dimensions.y)), 
            obb->center, 
            restitution, 
            static_friction, 
            dynamic_friction 
            };
}




typedef struct impulse {
    Vec2f delta_force;
    u32 milliseconds;
    Body_2D *target;
} Impulse;

/**
 * Some of these flags in theory can be moved to rigid body 2d to abstact shape from body when resolving collisions.
 */
typedef struct phys_box {
    OBB bound_box;
    Body_2D body;
    
    bool dynamic;
    bool rotatable;
    bool destructible;
    bool gravitable;
    bool grounded;
} Phys_Box;


static Phys_Box phys_box_make(Vec2f position, float width, float height, float rotation, float mass, float restitution, float static_friction, float dynamic_friction, bool dynamic, bool rotatable, bool destructible, bool gravitable) {
    Phys_Box phys_box;
    phys_box.bound_box = obb_make(position, width, height, rotation);
    phys_box.body = phys_body_obb_make(&phys_box.bound_box, mass, restitution, static_friction, dynamic_friction);
    phys_box.dynamic        = dynamic;
    phys_box.rotatable      = rotatable;
    phys_box.destructible   = destructible;
    phys_box.gravitable     = gravitable;

    return phys_box;
}

/**
 * Applies instanteneous force to rigid body.
 */
void phys_apply_force(Body_2D *body, Vec2f force);

/**
 * Applies instanteneous acceleration to rigid body.
 */
void phys_apply_acceleration(Body_2D *body, Vec2f acceleration);

void phys_apply_angular_acceleration(Body_2D *body, float acceleration);

/**
 * Takes in array of memory where phys_boxes are stored, count corresponds to count of phys boxes while stride corresponds to the offset in bytes that pointer should be moved to get next phys box.
 * @Important: Passed pointer should always point to the first actual phys box element.
 */
void phys_update(Phys_Box *phys_boxes, s64 count, s64 stride);


#endif
