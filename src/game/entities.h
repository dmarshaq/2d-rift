#ifndef ENTITIES_H
#define ENTITIES_H

#include "core/type.h"
#include "core/mathf.h"

#include "game/physics.h"

/**
 * Entities are using Array of Structure approach because of how various might accessing be and how different enetities operations per field might look like.
 * But other structures like physics or particles are better suited to use a Structure of Arrays approach due to their calculation and memory heavy nature.
 * So the ideal solution is to use a hybrid approach, and not overcomplicate the game by aggressivly sticking to a signle way of doing things.
 */

static const s64 MAX_ENTITIES = 64;




/**
 * Boilerplate for adding new entities.
 * Add new entities here.
 */
typedef struct prop_physics {
    Vec4f color;
} Prop_Physics;

typedef struct prop_static {
    Vec4f color;
} Prop_Static;

typedef struct player {
    Vec4f color;
} Player;



typedef enum entity_type : u8 {
    NONE          = 0x0,
    PROP_PHYSICS,
    PROP_STATIC,
    PLAYER,
} Entity_Type;

/**
 * Entity can be basically treated as a giant polymorphic structure of various game objects.
 * It migth now be the most optimized way, but it gets the job done if used correctly, and is simple,
 * because everything is an Entity.
 * @Important: If entity type is NONE it means it is not used in any away, basically, it doesn't exist and can be replaced any time soon with just spawned entity.
 */
typedef struct entity {
    Entity_Type type;
    Phys_Box phys_box;
    union {
        Prop_Physics    prop_physics;
        Prop_Static     prop_static;
        Player          player;
    };
} Entity;


/**
 * Returns the address of the array of MAX_ENTITIES lenght.
 * @Important: It also internally uses this pointer for the rest of the program's execution to manage entities.
 */
Entity *entities_init();

/**
 * Spawns a new entity of specified type.
 * Returns pointer to the spawned entity.
 * @Important: Doesn't initialize entity, if it failed to spawn returns NULL.
 */
Entity *entities_spawn(Entity_Type type);

/**
 * Removes an entity.
 * @Important: Entity pointer must be a valid entity that was properly spawned.
 * Entity is removed unorderdly from the array, so data of jus removed entity is replaced by the last entity in the array.
 */
void entities_remove(Entity *entity);

/**
 * Returns the count of entities.
 * @Important: entities_count() - 1, will get the index of the last entity in the array.
 */
s64 entities_count();














#endif
