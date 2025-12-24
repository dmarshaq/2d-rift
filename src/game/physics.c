#include "game/physics.h"

#include "core/mathf.h"
#include "core/core.h"

#include "game/game.h"



// Pointers to global state.
static Time_Info   *time_ptr;

void phys_init(State *state) {
    // Setting pointers to global state.
    time_ptr           = &state->t;
}




/**
 * Physics.
 */

static const Vec2f GRAVITY_ACCELERATION = (Vec2f){ 0.0f, -9.81f };
// static const Vec2f GRAVITY_ACCELERATION = (Vec2f){ 0.0f, 0.0f };
static const u8 MAX_IMPULSES = 16;
static const u8 MAX_PHYS_BOXES = 16;
static const u8 PHYS_ITERATIONS = 16;
static const float PHYS_ITERATION_STEP_TIME = (1.0f / PHYS_ITERATIONS);

/**
 * Standard units used:
 *      Mass -> kg
 *      Speed -> m/s
 *      Acceleration -> m/s^2
 *      Force -> m/s^2 * kg -> N
 *
 * Important formulas:
 *  
 *  Instanteneous acceleration and force.
 *      Vi + a = Vf
 *      Vi + (F / m) = Vf
 *  
 *  Acceleration and force over period of time / continuous.
 *      Vi + a * dt = Vf
 *      Vi + (F / m) * dt = Vf
 */

/**
 * Applies instanteneous force to rigid body.
 */
void phys_apply_force(Body_2D *body, Vec2f force) {
    body->velocity = vec2f_sum(body->velocity, vec2f_multi_constant(force, body->inv_mass));
}

/**
 * Applies instanteneous acceleration to rigid body.
 */
void phys_apply_acceleration(Body_2D *body, Vec2f acceleration) {
    body->velocity = vec2f_sum(body->velocity, acceleration);
}

void phys_apply_angular_acceleration(Body_2D *body, float acceleration) {
    body->angular_velocity += acceleration;
}



void phys_add_impulse(Vec2f force, u32 milliseconds, Body_2D *body) {
    TODO("Refactor impulses.");
    // Impulse impulse = (Impulse) { 
    //     .delta_force = vec2f_divide_constant(force, (float)milliseconds), 
    //         .milliseconds = milliseconds, 
    //         .target = body 
    // };
    // array_list_append(&state->impulses, impulse);
}

void phys_apply_impulses() {
    TODO("Refactor impulses.");
    // for (u32 i = 0; i < array_list_length(&state->impulses); i++) {
    //     u32 force_time_multi = state->t.delta_time_milliseconds;

    //     if (state->impulses[i].milliseconds < state->t.delta_time_milliseconds) {
    //         force_time_multi = state->impulses[i].milliseconds;
    //         state->impulses[i].milliseconds = 0;
    //     }
    //     else {
    //         state->impulses[i].milliseconds -= state->t.delta_time_milliseconds;
    //     }

    //     state->impulses[i].target->velocity = vec2f_sum(state->impulses[i].target->velocity, vec2f_multi_constant(vec2f_divide_constant(state->impulses[i].delta_force, state->impulses[i].target->mass), (float)force_time_multi));

    //     if (state->impulses[i].milliseconds == 0) {
    //         array_list_unordered_remove(&state->impulses, i);
    //         i--;
    //     }
    // }
}

/**
 * Internal function.
 * @Important: "axis1" should always correspond to the axis alligned with "obb1".
 * Returns min separation between "obb1" and "obb2" projected points on "axis1".
 */
float phys_sat_min_depth_on_normal(OBB *obb1, Vec2f axis1, OBB *obb2) {
    // Getting minA and maxA out of 2 points.
    float minA = vec2f_dot(axis1, obb_p0(obb1));
    float maxA = vec2f_dot(axis1, obb_p1(obb1));

    if (minA > maxA) {
        minA = maxA;
        maxA = vec2f_dot(axis1, obb_p0(obb1));
    }

    // Getting minB and maxB out of 4 points. @Optimization: Maybe find a way to calculate dot products only once and reuse them to find min, max.
    float minB = fminf(fminf(vec2f_dot(axis1, obb_p0(obb2)), vec2f_dot(axis1, obb_p1(obb2))), fminf(vec2f_dot(axis1, obb_p2(obb2)), vec2f_dot(axis1, obb_p3(obb2))));
    float maxB = fmaxf(fmaxf(vec2f_dot(axis1, obb_p0(obb2)), vec2f_dot(axis1, obb_p1(obb2))), fmaxf(vec2f_dot(axis1, obb_p2(obb2)), vec2f_dot(axis1, obb_p3(obb2))));
    
    float dist1 = maxB - minA; 
    float dist2 = maxA - minB; 

    return fabsf(fminf(dist1, dist2)) * sig(dist1 * dist2);
}

/**
 * Returns true of "obb1" and "obb2" touch.
 * Usefull for triggers.
 */
bool phys_sat_check_collision_obb(OBB *obb1, OBB *obb2) {
    return phys_sat_min_depth_on_normal(obb1, obb_right(obb1), obb2) > 0.0f &&
        phys_sat_min_depth_on_normal(obb1, obb_up(obb1), obb2) > 0.0f &&
        phys_sat_min_depth_on_normal(obb2, obb_right(obb2), obb1) > 0.0f &&
        phys_sat_min_depth_on_normal(obb2, obb_up(obb2), obb1) > 0.0f;
}

void phys_sat_find_min_depth_normal(OBB *obb1, OBB *obb2, float *depth, Vec2f *normal) {
    Vec2f normals[4] = {
        obb_right(obb1),
        obb_up(obb1),
        obb_right(obb2),
        obb_up(obb2)
    };
    float depths[4] = {
        phys_sat_min_depth_on_normal(obb1, normals[0], obb2),
        phys_sat_min_depth_on_normal(obb1, normals[1], obb2),
        phys_sat_min_depth_on_normal(obb2, normals[2], obb1),
        phys_sat_min_depth_on_normal(obb2, normals[3], obb1)
    };

    for (u32 i = 1; i < 4; i++) {
        if (depths[i] < depths[0]) {
            depths[0] = depths[i];
            normals[0] = normals[i];
        }
    }


    // Flip normal if it's not looking in direction of collosion.
    if (vec2f_dot(normals[0], vec2f_normalize(vec2f_difference(obb2->center, obb1->center))) < 0.0f) {
        normals[0] = vec2f_negate(normals[0]);
    }

    *depth = depths[0];
    *normal = normals[0];
}

/**
 * Sets both obbs apart based on depth and normal.
 * Usefull for two colliding dynamic objects.
 */
void phys_resolve_dynamic_obb_collision(OBB *obb1, OBB *obb2, float depth, Vec2f normal) {

    Vec2f displacement = vec2f_multi_constant(normal, depth / 2);

    obb1->center = vec2f_sum(obb1->center, vec2f_negate(displacement));
    obb2->center = vec2f_sum(obb2->center, displacement);
}

void phys_resolve_dynamic_body_collision_basic(Body_2D *body1, Body_2D *body2, Vec2f normal) {
    Vec2f relative_velocity = vec2f_difference(body2->velocity, body1->velocity);

    float e = fminf(body1->restitution, body2->restitution);

    float j = -(1.0f + e) * vec2f_dot(relative_velocity, normal);
    j /= (1.0f * body1->inv_mass + 1.0f * body2->inv_mass);

    phys_apply_force(body1, vec2f_multi_constant(normal, -j));
    phys_apply_force(body2, vec2f_multi_constant(normal, j));
}

void phys_resolve_dynamic_body_collision(Body_2D *body1, Body_2D *body2, Vec2f normal, Vec2f *contacts, u32 contacts_count) {
    // Calculated variables, used in both loops.
    Vec2f impulses[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    Vec2f r1_array[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    Vec2f r2_array[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    
    // Calculation variables.
    float e = fminf(body1->restitution, body2->restitution);

    Vec2f angular_lin_velocity1, angular_lin_velocity2, relative_velocity;
    float r1_perp_dot_n, r2_perp_dot_n, contact_velocity_mag, j;

    // Calculating impulses and vectors to contact points, for each contact point.
    for (u32 i = 0; i < contacts_count; i++) {
        r1_array[i] = vec2f_difference(contacts[i], body1->mass_center);
        r2_array[i] = vec2f_difference(contacts[i], body2->mass_center);

        angular_lin_velocity1 = vec2f_multi_constant(vec2f_make(-r1_array[i].y, r1_array[i].x), body1->angular_velocity);
        angular_lin_velocity2 = vec2f_multi_constant(vec2f_make(-r2_array[i].y, r2_array[i].x), body2->angular_velocity);

        relative_velocity = vec2f_difference(vec2f_sum(body2->velocity, angular_lin_velocity2), vec2f_sum(body1->velocity, angular_lin_velocity1));
        
        contact_velocity_mag = vec2f_dot(relative_velocity, normal);

        if (contact_velocity_mag > 0.0f) {
            continue;
        }

        r1_perp_dot_n = vec2f_cross(r1_array[i], normal);
        r2_perp_dot_n = vec2f_cross(r2_array[i], normal);

        j = -(1.0f + e) * contact_velocity_mag;
        j /= body1->inv_mass + body2->inv_mass + (r1_perp_dot_n * r1_perp_dot_n) * body1->inv_inertia + (r2_perp_dot_n * r2_perp_dot_n) * body2->inv_inertia;
        j /= (float)contacts_count;

        impulses[i] = vec2f_multi_constant(normal, j);
    }

    // For each contact point applying impulse and angular acceleration.
    for (u32 i = 0; i < contacts_count; i++) {
        phys_apply_force(body1, vec2f_negate(impulses[i]));
        phys_apply_angular_acceleration(body1, -vec2f_cross(r1_array[i], impulses[i]) * body1->inv_inertia);

        phys_apply_force(body2, impulses[i]);
        phys_apply_angular_acceleration(body2, vec2f_cross(r2_array[i], impulses[i]) * body2->inv_inertia);
    }
}

typedef struct phys_collision_calc_vars {
    // Calculated variables, used in both loops.
    Vec2f impulse;
    Vec2f r1;
    Vec2f r2;
    
    // Calculation variables.
    float e;
    float sf;
    float df;

    Vec2f angular_lin_velocity1;
    Vec2f angular_lin_velocity2;
    Vec2f relative_velocity;
    float r1_perp_dot_n; 
    float r2_perp_dot_n;
    float contact_velocity_mag;
    float j[2];
    float jt;

    // Result variables.
    Vec2f res_velocity1;
    Vec2f res_velocity2;
    float res_angular_acceleration1;
    float res_angular_acceleration2;

} Phys_Collision_Calc_Vars;

static Phys_Collision_Calc_Vars p_calc_vars;

void phys_resolve_phys_box_collision_with_rotation_friction(Phys_Box *box1, Phys_Box *box2, Vec2f normal, Vec2f *contacts, u32 contacts_count) {
    // Setting up variables that can be calculated without contact points.
    p_calc_vars.e  = fminf(box1->body.restitution, box2->body.restitution);
    p_calc_vars.sf = (box1->body.static_friction + box2->body.static_friction) / 2;
    p_calc_vars.df = (box1->body.dynamic_friction + box2->body.dynamic_friction) / 2;

    // Init resulat vars to default values.
    p_calc_vars.res_velocity1 = VEC2F_ORIGIN;
    p_calc_vars.res_velocity2 = VEC2F_ORIGIN;
    p_calc_vars.res_angular_acceleration1 = 0.0f;
    p_calc_vars.res_angular_acceleration2 = 0.0f;


    p_calc_vars.r1 = VEC2F_ORIGIN;
    p_calc_vars.r2 = VEC2F_ORIGIN;
    p_calc_vars.impulse = VEC2F_ORIGIN;

    p_calc_vars.j[0] = 0.0f;
    p_calc_vars.j[1] = 0.0f;

    // Calculating impulses and vectors to contact points, for each contact point.
    for (u32 i = 0; i < contacts_count; i++) {
        p_calc_vars.r1 = vec2f_difference(contacts[i], box1->body.mass_center);
        p_calc_vars.r2 = vec2f_difference(contacts[i], box2->body.mass_center);

        p_calc_vars.angular_lin_velocity1 = vec2f_multi_constant(vec2f_make(-p_calc_vars.r1.y, p_calc_vars.r1.x), box1->body.angular_velocity);
        p_calc_vars.angular_lin_velocity2 = vec2f_multi_constant(vec2f_make(-p_calc_vars.r2.y, p_calc_vars.r2.x), box2->body.angular_velocity);

        p_calc_vars.relative_velocity = vec2f_difference(vec2f_sum(box2->body.velocity, p_calc_vars.angular_lin_velocity2), vec2f_sum(box1->body.velocity, p_calc_vars.angular_lin_velocity1));

        p_calc_vars.contact_velocity_mag = vec2f_dot(p_calc_vars.relative_velocity, normal);


        if (p_calc_vars.contact_velocity_mag > 0.0f) {
            continue;
        }

        p_calc_vars.r1_perp_dot_n = vec2f_cross(p_calc_vars.r1, normal);
        p_calc_vars.r2_perp_dot_n = vec2f_cross(p_calc_vars.r2, normal);

        p_calc_vars.j[i] = -(1.0f + p_calc_vars.e) * p_calc_vars.contact_velocity_mag;
        p_calc_vars.j[i] /= box1->body.inv_mass + box2->body.inv_mass + (p_calc_vars.r1_perp_dot_n * p_calc_vars.r1_perp_dot_n) * box1->body.inv_inertia + (p_calc_vars.r2_perp_dot_n * p_calc_vars.r2_perp_dot_n) * box2->body.inv_inertia;
        p_calc_vars.j[i] /= (float)contacts_count;



        p_calc_vars.impulse = vec2f_multi_constant(normal, p_calc_vars.j[i]);

        p_calc_vars.res_velocity1 = vec2f_sum(p_calc_vars.res_velocity1, vec2f_multi_constant(vec2f_negate(p_calc_vars.impulse), box1->body.inv_mass));
        p_calc_vars.res_angular_acceleration1 += -vec2f_cross(p_calc_vars.r1, p_calc_vars.impulse) * box1->body.inv_inertia;

        p_calc_vars.res_velocity2 = vec2f_sum(p_calc_vars.res_velocity2, vec2f_multi_constant(p_calc_vars.impulse, box2->body.inv_mass));
        p_calc_vars.res_angular_acceleration2 += vec2f_cross(p_calc_vars.r2, p_calc_vars.impulse) * box2->body.inv_inertia;
    }

    box1->body.velocity = vec2f_sum(box1->body.velocity, p_calc_vars.res_velocity1);
    if (box1->rotatable)
        box1->body.angular_velocity += p_calc_vars.res_angular_acceleration1;

    box2->body.velocity = vec2f_sum(box2->body.velocity, p_calc_vars.res_velocity2);
    if (box2->rotatable)
        box2->body.angular_velocity += p_calc_vars.res_angular_acceleration2;



    p_calc_vars.res_velocity1 = VEC2F_ORIGIN;
    p_calc_vars.res_velocity2 = VEC2F_ORIGIN;
    p_calc_vars.res_angular_acceleration1 = 0.0f;
    p_calc_vars.res_angular_acceleration2 = 0.0f;


    p_calc_vars.r1 = VEC2F_ORIGIN;
    p_calc_vars.r2 = VEC2F_ORIGIN;
    p_calc_vars.impulse = VEC2F_ORIGIN;


    // Friction.
    for (u32 i = 0; i < contacts_count; i++) {
        p_calc_vars.r1 = vec2f_difference(contacts[i], box1->body.mass_center);
        p_calc_vars.r2 = vec2f_difference(contacts[i], box2->body.mass_center);

        p_calc_vars.angular_lin_velocity1 = vec2f_multi_constant(vec2f_make(-p_calc_vars.r1.y, p_calc_vars.r1.x), box1->body.angular_velocity);
        p_calc_vars.angular_lin_velocity2 = vec2f_multi_constant(vec2f_make(-p_calc_vars.r2.y, p_calc_vars.r2.x), box2->body.angular_velocity);

        p_calc_vars.relative_velocity = vec2f_difference(vec2f_sum(box2->body.velocity, p_calc_vars.angular_lin_velocity2), vec2f_sum(box1->body.velocity, p_calc_vars.angular_lin_velocity1));

        Vec2f tanget = vec2f_difference(p_calc_vars.relative_velocity, vec2f_multi_constant(normal, vec2f_dot(p_calc_vars.relative_velocity, normal)));

        
        if (fequal(tanget.x, 0.0f) && fequal(tanget.y, 0.0f)) {
            continue;
        }

        tanget = vec2f_normalize(tanget);

        p_calc_vars.r1_perp_dot_n = vec2f_cross(p_calc_vars.r1, tanget);
        p_calc_vars.r2_perp_dot_n = vec2f_cross(p_calc_vars.r2, tanget);

        p_calc_vars.jt = -vec2f_dot(p_calc_vars.relative_velocity, tanget);
        p_calc_vars.jt /= box1->body.inv_mass + box2->body.inv_mass + (p_calc_vars.r1_perp_dot_n * p_calc_vars.r1_perp_dot_n) * box1->body.inv_inertia + (p_calc_vars.r2_perp_dot_n * p_calc_vars.r2_perp_dot_n) * box2->body.inv_inertia;
        p_calc_vars.jt /= (float)contacts_count;

        // Collumbs law.
        if (fabsf(p_calc_vars.jt) <= p_calc_vars.j[i] * p_calc_vars.sf) {
            // friction_impulses[i] = vec2f_multi_constant(tanget, p_calc_vars.jt);
            p_calc_vars.impulse = vec2f_multi_constant(tanget, p_calc_vars.jt);
        }
        else {
            // friction_impulses[i] = vec2f_multi_constant(tanget, -j_array[i] * p_calc_vars.df);
            p_calc_vars.impulse = vec2f_multi_constant(tanget, -p_calc_vars.j[i] * p_calc_vars.df);
        }

        p_calc_vars.res_velocity1 = vec2f_sum(p_calc_vars.res_velocity1, vec2f_multi_constant(vec2f_negate(p_calc_vars.impulse), box1->body.inv_mass));
        p_calc_vars.res_angular_acceleration1 += -vec2f_cross(p_calc_vars.r1, p_calc_vars.impulse) * box1->body.inv_inertia;

        p_calc_vars.res_velocity2 = vec2f_sum(p_calc_vars.res_velocity2, vec2f_multi_constant(p_calc_vars.impulse, box2->body.inv_mass));
        p_calc_vars.res_angular_acceleration2 += vec2f_cross(p_calc_vars.r2, p_calc_vars.impulse) * box2->body.inv_inertia;
    }

    box1->body.velocity = vec2f_sum(box1->body.velocity, p_calc_vars.res_velocity1);
    if (box1->rotatable)
        box1->body.angular_velocity += p_calc_vars.res_angular_acceleration1;

    box2->body.velocity = vec2f_sum(box2->body.velocity, p_calc_vars.res_velocity2);
    if (box2->rotatable)
        box2->body.angular_velocity += p_calc_vars.res_angular_acceleration2;
}

void phys_resolve_dynamic_body_collision_with_friction(Body_2D *body1, Body_2D *body2, Vec2f normal, Vec2f *contacts, u32 contacts_count) {
    // Calculated variables, used in both loops.
    Vec2f impulses[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    Vec2f r1_array[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    Vec2f r2_array[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    float j_array[2] = { 0.0f, 0.0f };
    Vec2f friction_impulses[2] = { VEC2F_ORIGIN, VEC2F_ORIGIN };
    
    // Calculation variables.
    float e = fminf(body1->restitution, body2->restitution);
    float sf = (body1->static_friction + body2->static_friction) / 2;
    float df = (body1->dynamic_friction + body2->dynamic_friction) / 2;

    Vec2f angular_lin_velocity1, angular_lin_velocity2, relative_velocity;
    float r1_perp_dot_n, r2_perp_dot_n, contact_velocity_mag, j, jt;

    // Calculating impulses and vectors to contact points, for each contact point.
    for (u32 i = 0; i < contacts_count; i++) {
        r1_array[i] = vec2f_difference(contacts[i], body1->mass_center);
        r2_array[i] = vec2f_difference(contacts[i], body2->mass_center);

        angular_lin_velocity1 = vec2f_multi_constant(vec2f_make(-r1_array[i].y, r1_array[i].x), body1->angular_velocity);
        angular_lin_velocity2 = vec2f_multi_constant(vec2f_make(-r2_array[i].y, r2_array[i].x), body2->angular_velocity);

        relative_velocity = vec2f_difference(vec2f_sum(body2->velocity, angular_lin_velocity2), vec2f_sum(body1->velocity, angular_lin_velocity1));

        contact_velocity_mag = vec2f_dot(relative_velocity, normal);


        if (contact_velocity_mag > 0.0f) {
            continue;
        }

        r1_perp_dot_n = vec2f_cross(r1_array[i], normal);
        r2_perp_dot_n = vec2f_cross(r2_array[i], normal);

        j = -(1.0f + e) * contact_velocity_mag;
        j /= body1->inv_mass + body2->inv_mass + (r1_perp_dot_n * r1_perp_dot_n) * body1->inv_inertia + (r2_perp_dot_n * r2_perp_dot_n) * body2->inv_inertia;
        j /= (float)contacts_count;


        j_array[i] = j;

        impulses[i] = vec2f_multi_constant(normal, j);
    }

    // For each contact point applying impulse and angular acceleration.
    for (u32 i = 0; i < contacts_count; i++) {
        phys_apply_force(body1, vec2f_negate(impulses[i]));
        phys_apply_angular_acceleration(body1, -vec2f_cross(r1_array[i], impulses[i]) * body1->inv_inertia);

        phys_apply_force(body2, impulses[i]);
        phys_apply_angular_acceleration(body2, vec2f_cross(r2_array[i], impulses[i]) * body2->inv_inertia);
    }

    // Friction.
    for (u32 i = 0; i < contacts_count; i++) {
        r1_array[i] = vec2f_difference(contacts[i], body1->mass_center);
        r2_array[i] = vec2f_difference(contacts[i], body2->mass_center);

        angular_lin_velocity1 = vec2f_multi_constant(vec2f_make(-r1_array[i].y, r1_array[i].x), body1->angular_velocity);
        angular_lin_velocity2 = vec2f_multi_constant(vec2f_make(-r2_array[i].y, r2_array[i].x), body2->angular_velocity);

        relative_velocity = vec2f_difference(vec2f_sum(body2->velocity, angular_lin_velocity2), vec2f_sum(body1->velocity, angular_lin_velocity1));


        Vec2f tanget = vec2f_difference(relative_velocity, vec2f_multi_constant(normal, vec2f_dot(relative_velocity, normal)));

        
        if (fequal(tanget.x, 0.0f) && fequal(tanget.y, 0.0f)) {
            continue;
        }

        tanget = vec2f_normalize(tanget);

        r1_perp_dot_n = vec2f_cross(r1_array[i], tanget);
        r2_perp_dot_n = vec2f_cross(r2_array[i], tanget);

        jt = -vec2f_dot(relative_velocity, tanget);
        jt /= body1->inv_mass + body2->inv_mass + (r1_perp_dot_n * r1_perp_dot_n) * body1->inv_inertia + (r2_perp_dot_n * r2_perp_dot_n) * body2->inv_inertia;
        jt /= (float)contacts_count;
    

        // Collumbs law.
        if (fabsf(jt) <= j * sf) {
            friction_impulses[i] = vec2f_multi_constant(tanget, jt);
        }
        else {
            friction_impulses[i] = vec2f_multi_constant(tanget, -j_array[i] * df);
        }
    }

    // For each contact point applying friction impulse and angular acceleration.
    for (u32 i = 0; i < contacts_count; i++) {
        phys_apply_force(body1, vec2f_negate(friction_impulses[i]));
        phys_apply_angular_acceleration(body1, -vec2f_cross(r1_array[i], friction_impulses[i]) * body1->inv_inertia);

        phys_apply_force(body2, friction_impulses[i]);
        phys_apply_angular_acceleration(body2, vec2f_cross(r2_array[i], friction_impulses[i]) * body2->inv_inertia);
    }
}

/**
 * Sets dynamic obb apart based on depth and normal.
 * Usefull for colliding dynamic object with immovable or static object.
 */
void phys_resolve_static_obb_collision(OBB *obb, float depth, Vec2f normal) {
    obb->center = vec2f_sum(obb->center, vec2f_multi_constant(normal, depth));
}

/**
 * Finds all contact points, maximum of 2, and stores them in "points" array.
 * @Important: "points" should not be NULL and should be of size two.
 * Returns count of points found.
 */
u32 phys_find_contanct_points_obb(OBB* obb1, OBB* obb2, Vec2f *points) {
    Vec2f verticies[8] = {
        obb_p0(obb1),
        obb_p2(obb1),
        obb_p1(obb1),
        obb_p3(obb1),
        obb_p0(obb2),
        obb_p2(obb2),
        obb_p1(obb2),
        obb_p3(obb2),
    };
    
    Vec2f a, b, p;
    float min_dist1 = FLT_MAX;
    float dist = 0.0f;
    u32 count = 0;

    for (u32 o = 0; o < 2; o++) {
        for (u32 i = 0; i < 4; i++) {
            p = verticies[i + o * 4];
            for (u32 j = 0; j < 4; j++) {
                a = verticies[j + 4 - o * 4];
                b = verticies[((j + 1) % 4) + 4 - o * 4];

                dist = point_segment_min_distance(p, a, b);

                if (fequal(dist, min_dist1)) {
                    if (!(fequal(p.x, p.y) && fequal(points[0].x, points[0].y))) {
                        points[1] = p;
                        count = 2;
                    }
                }
                else if (dist < min_dist1) {
                    min_dist1 = dist;
                    points[0] = p;
                    count = 1;
                }
            }
        }
    }

    return count;
}


void phys_update(Phys_Box *phys_boxes, s64 count, s64 stride) {
    float depth;
    Vec2f normal;
    Phys_Box *box1;
    Phys_Box *box2;

    for (u32 it = 0; it < PHYS_ITERATIONS; it++) {

        for (u32 i = 0; i < count; i++) {
            // box1 = phys_boxes[i];
            box1 = (Phys_Box *)((char *)(phys_boxes) + i * stride);


            if (!box1->dynamic) {
                continue;
            }
            
            box1->grounded = false;

            // Applying gravity.
            if (box1->gravitable) {
                box1->body.velocity = vec2f_sum(box1->body.velocity, vec2f_multi_constant(GRAVITY_ACCELERATION, time_ptr->delta_time * PHYS_ITERATION_STEP_TIME ));
            }

            // Applying velocities.
            box1->bound_box.center = vec2f_sum(box1->bound_box.center, vec2f_multi_constant(box1->body.velocity, time_ptr->delta_time * PHYS_ITERATION_STEP_TIME));
            box1->bound_box.rot += box1->body.angular_velocity * time_ptr->delta_time * PHYS_ITERATION_STEP_TIME;
            box1->body.mass_center = box1->bound_box.center;

            // @Incomplete: Add proper debugging support (physics visualization).
            // // Debug drawing.
            // draw_line(box1->bound_box.center, vec2f_sum(box1->bound_box.center, vec2f_multi_constant(box1->body.velocity, t->delta_time * PHYS_ITERATION_STEP_TIME)), VEC4F_RED, &state->debug_vert_buffer);
            // 
            // // Debug drawing.
            // draw_quad_outline(obb_p0(&box1->bound_box), obb_p1(&box1->bound_box), VEC4F_RED, box1->bound_box.rot, &state->debug_vert_buffer);

        }

        Vec2f contacts[2];
        u32 contacts_count;
        // Collision.
        for (u32 i = 0; i < count; i++) {
            box1 = (Phys_Box *)((char *)(phys_boxes) + i * stride);
            for (u32 j = i + 1; j < count; j++) {
                box2 = (Phys_Box *)((char *)(phys_boxes) + j * stride);
                if (phys_sat_check_collision_obb(&box1->bound_box, &box2->bound_box)) { // @Speed: Need separate broad phase.
                    
                    // Fidning depth and normal of collision.
                    phys_sat_find_min_depth_normal(&box1->bound_box, &box2->bound_box, &depth, &normal);

                    // Calculating dot product to check if any objects are grounded.
                    float grounded_dot = vec2f_dot(vec2f_normalize(GRAVITY_ACCELERATION), normal);

                    if (box1->dynamic && !box2->dynamic) {
                        phys_resolve_static_obb_collision(&box1->bound_box, depth, vec2f_negate(normal));

                        if (grounded_dot > 0.7f)
                            box1->grounded = true;
                    }
                    else if (box2->dynamic && !box1->dynamic) {
                        phys_resolve_static_obb_collision(&box2->bound_box, depth, normal);

                        if (grounded_dot < -0.7f)
                            box2->grounded = true;
                    }
                    else {
                        phys_resolve_dynamic_obb_collision(&box1->bound_box, &box2->bound_box, depth, normal);

                        if (grounded_dot > 0.7f)
                            box1->grounded = true;
                        else if (grounded_dot < -0.7f)
                            box2->grounded = true;
                    }
                    box1->body.mass_center = box1->bound_box.center;
                    box2->body.mass_center = box2->bound_box.center;

                    // Detailed physics collision response resolution happens here.
                    contacts_count = phys_find_contanct_points_obb(&box1->bound_box, &box2->bound_box, contacts);
                    phys_resolve_phys_box_collision_with_rotation_friction(box1, box2, normal, contacts, contacts_count);
                }
            }
        }
    }
}



