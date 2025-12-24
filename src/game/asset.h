#ifndef ASSET_H
#define ASSET_H


#include "core/str.h"


typedef struct asset_change {
    String full_path;           // Ex: 'res/context/some_file.txt'
    String file_name;           // Ex: 'some_file.txt'
    String file_format;         // Ex: 'txt'
} Asset_Change;


/**
 * Forcefully generate asset changes for every file in the directory specified.
 * Can be used to track all existing assets during app initialization stage.
 * Or can be used to hot reload assets in the specific folders.
 * Return 0 if successful
 */
int asset_force_changes(char *directory);

/**
 * Specified directory path would be recursivly monitored for any changes made.
 * Returns 0 if initted correctly.
 */
int asset_observer_init(char *directory);

/**
 * Collects all of the changes, that can be viewed through view_asset_changes(...) function.
 * Ideally be called only once per frame.
 * Can be used in hotreloading or be used to dynamically poll changes every frame.
 * Returns 0 if successful.
 */
int asset_observer_poll_changes();


/**
 * Returns true if there are any assets changed, otherwise returns false.
 * Outputs count of the assets changed in the count variable, and outputs pointer to the changes buffer in changes variable.
 */
bool asset_view_changes(u32 *count, const Asset_Change **changes);

/**
 * Removes asset change from the changes list.
 * Note: This means that count of changes will decrease by 1.
 */
void asset_remove_change(u32 index);







#endif
