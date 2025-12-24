#include "game/entities.h"

#include "core/core.h"

#include <stdlib.h>
#include <string.h>

static Entity *entities;
static s64 entities_count_;

Entity *entities_init() {
    entities = calloc(MAX_ENTITIES, sizeof(Entity));
    entities_count_ = 0;
    return entities;
}

Entity *entities_spawn(Entity_Type type) {
    entities_count_++;

    if (entities_count_ > MAX_ENTITIES) {
        printf_err("Couldn't spawn entity, MAX_ENTITIES limit of %lld has been reached.\n", MAX_ENTITIES);
        entities_count_ = MAX_ENTITIES;
        return NULL;
    }

    entities[entities_count_ - 1].type = type;

    // Default initialization of entities.
    switch (type) {
        case NONE:
            break;
        case PROP_PHYSICS:
            TODO("PROP_PHYSICS default initialization.");
            break;
        case PROP_STATIC:
            break;
        case PLAYER:
            TODO("PLAYER default initialization.");
            break;
        default:
            printf_warning("Couldn't default initialize entity, unknown type.\n");
            break;
    }


    return entities + entities_count_ - 1;
}

void entities_remove(Entity *entity) {
    entities_count_--;
    memcpy(entity, entities + entities_count_, sizeof(Entity));
}

s64 entities_count() {
    return entities_count_;
}


