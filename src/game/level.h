#ifndef LEVEL_H
#define LEVEL_H

#include "core/str.h"

#include "game/physics.h"


typedef struct state State;

// 0x6c65766c stands for 'levl' in ascii.
#define LEVEL_FORMAT_HEADER 0x6c65766c

extern const String LEVEL_FILE_PATH;
extern const String LEVEL_FILE_FORMAT;

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

typedef struct trigger {
    String name;
    OBB bound_box;
} Trigger;



typedef enum entity_type : u8 {
    NONE          = 0x0,
    PROP_PHYSICS,
    PROP_STATIC,
    PLAYER,
    TRIGGER,
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
        Trigger         trigger;
    };
} Entity;


typedef enum level_flags : u8 {
    LEVEL_LOADED = 0x01,
} Level_Flags;


typedef struct level {
    String name;

    Level_Flags flags;

    s64 entities_count;
    Entity *entities;

    s64 phys_polygons_count;
    Phys_Polygon *phys_polygons;
} Level;


/**
 * Inits level manager, should be called one time before any other function is called.
 */
void level_manager_init(State *state);

/**
 * Loads level into the memory.
 */
@Introspect;
@RegisterCommand;
void level_load(String name);

/**
 * Following function updates currently loaded level.
 */
void level_update();

/**
 * Following function draws currently loaded level.
 */
void level_draw();

/**
 * Adds specified entity to the array of entities.
 * Returns index of added element in the array.
 * Returns -1 if there is not enough space.
 */
s64 level_add_entity(Entity entity);

/**
 * Unorderly removes specified entity by its index from the array of entities.
 * Returns the removed entity.
 * Returns entity NONE if nothing was removed, or removed entity is of type NONE.
 */
Entity level_remove_entity(s64 index);





#endif
