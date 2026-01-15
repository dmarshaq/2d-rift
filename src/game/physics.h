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
    bool active;
} Phys_Box;


typedef struct phys_edge {
    Vec2f vertex;
    Vec2f normal;
} Phys_Edge;

/**
 * Polygons are defined to be used as complex static convex geometry. Meaning it is any shape that is enclosed, convex and static.
 * A great example is level geometry. 
 * For now every level is composed of such polygons to easier work with SAT based collision.
 */
typedef struct phys_polygon {
    u32 edges_count;
    Phys_Edge *edges;
} Phys_Polygon;

static Vec2f phys_polygon_center(Phys_Polygon *polygon) {
    Vec2f center = VEC2F_ORIGIN;

    for (u32 i = 0; i < polygon->edges_count; i++) {
        vec2f_sum(center, polygon->edges[i].vertex);
    }

    center = vec2f_divide_constant(center, polygon->edges_count);

    return center;
}

#define PHYS_INACTIVE_BOX ((Phys_Box) {0})


static Phys_Box phys_box_make(Vec2f position, float width, float height, float rotation, float mass, float restitution, float static_friction, float dynamic_friction, bool dynamic, bool rotatable, bool destructible, bool gravitable) {
    Phys_Box phys_box;
    phys_box.bound_box = obb_make(position, width, height, rotation);
    phys_box.body = phys_body_obb_make(&phys_box.bound_box, mass, restitution, static_friction, dynamic_friction);
    phys_box.dynamic        = dynamic;
    phys_box.rotatable      = rotatable;
    phys_box.destructible   = destructible;
    phys_box.gravitable     = gravitable;
    phys_box.active         = true;

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

/**
 * Returns true of "obb1" and "obb2" touch.
 * Usefull for triggers.
 */
bool phys_sat_check_collision_obb(OBB *obb1, OBB *obb2);

/**
 * Perfoms raycast check against a segment from a to b.
 * If raycast hits segment, returns true and changes values hit and distance.
 * If there is not intersection between raycast and segment, doesn't change values and returns false.
 * @Important: distance value works also as input to limit how far a raycast can reach, if raycast is infinite make sure to default it to FLT_MAX.
 */
bool phys_ray_cast(Vec2f origin, Vec2f direction, Vec2f a, Vec2f b, Vec2f *hit, float *distance, Vec2f *normal);

/**
 * Perfoms raycast aginst specific obb, works exactly the same as 'phys_ray_cast'.
 */
bool phys_ray_cast_obb(Vec2f origin, Vec2f direction, OBB *obb, Vec2f *hit, float *distance, Vec2f *normal);

#endif
