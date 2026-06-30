/* ************************************************************************** */
/*                                                                            */
/*   hashmap.c — primary index: username -> player_t*                         */
/*                                                                            */
/*   Holds one pointer per live player; the player_t objects are owned here   */
/*   and shared by reference with the skip list. hashmap_put is LWW (the      */
/*   latest record for a username wins).                                      */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: define struct s_hashmap (bucket array + entries)
// TODO: hashmap_create / hashmap_destroy
// TODO: hashmap_get / hashmap_put / hashmap_foreach
// TODO: static helper — hash_username (djb2 or fnv-1a), forward-declared up top
