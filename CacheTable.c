#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include "CacheTable.h"
#include "checkerr.h"

/* ---------------------- Private Function Prototypes ----------------------- */

void Cache_init(Cache cache, int size);
int  Cache_hash(char *url, int port);
int  Cache_length(Cache cache);
int  Cache_size(Cache cache);

/* -------------------- HTTP_R Function Definitions -------------------- */

/* Object_new
 *      Purpose: Create a new Object struct and populate its fields with the
 *               supplied line. Store the line command in 'cmd' variable. 
 *   Parameters: Correctly formatted line containing the command and object
 *               values
 *      Returns: HTTP_R pointer
 */
HTTP_R CacheObject_new(char *line)
{
    /* Initialize Cursor to HTTP Request */
    char *cursor = line;
    
    /* Initialize Tokens */
    char *method_tkn = NULL;
    char *url_tkn    = NULL;
    char *v_tkn      = NULL;
    char *host_tkn   = NULL;
    char *port_tkn   = NULL;

    /* Parse HTTP Request */
    method_tkn = strtok_r(cursor, " ", &cursor);
    url_tkn = strtok_r(cursor, " ", &cursor);
    v_tkn = strtok_r(cursor, "\n", &cursor);
    strtok_r(cursor, " ", &cursor);
    host_tkn = strtok_r(cursor, ":\r\n\0", &cursor);

    /* Check for Port */
    char *urlcpy  = (url_tkn  != NULL) ? strdup(url_tkn)  : NULL;
    char *hostcpy = (host_tkn != NULL) ? strdup(host_tkn) : NULL;
    fprintf(stderr, "URLCPY = %s\n", urlcpy);
    fprintf(stderr, "HOSTCPY = %s\n", hostcpy);
    if (hostcpy != NULL) {
        cursor = hostcpy;
        strtok_r(cursor, ":", &cursor);
        port_tkn = strtok_r(cursor, "\n", &cursor);
    } /* Bug had else if here  F**K */
    if (port_tkn == NULL && urlcpy != NULL) {
        cursor = urlcpy;
        strtok_r(cursor, ":", &cursor);
        strtok_r(cursor, ":", &cursor);
        port_tkn = strtok_r(cursor, "\n", &cursor);
    }
    free(urlcpy);
    free(hostcpy);

    /* Create Object and Populate Members */
    HTTP_R cobj = malloc(sizeof(*cobj));
    check_mem_err(cobj);

    cobj->method    = (method_tkn != NULL) ? strdup(method_tkn)      : NULL;
    cobj->url       = (url_tkn    != NULL) ? strdup(url_tkn)         : NULL;
    cobj->version   = (v_tkn      != NULL) ? strdup(v_tkn)           : NULL;
    cobj->host      = (host_tkn   != NULL) ? strdup(host_tkn)        : NULL;
    cobj->port      = (port_tkn   != NULL) ? atoi(port_tkn)          : 80;
    cobj->hash      = Cache_hash(cobj->url, cobj->port);
    cobj->status    = NULL;
    cobj->content   = NULL;
    cobj->status_l  = -1;
    cobj->content_l = -1;
    cobj->max_age   = -1;
    cobj->start_age = time(NULL);
    cobj->curr_age  = 0;
    
    return cobj;
}

/* Object_free
 *      Purpose: Deallocates memory of an Object struct 
 *   Parameters: Object struct pointer
 *      Returns: None
 */
void CacheObject_free(HTTP_R cobj)
{
    free(cobj->method);
    free(cobj->url);
    free(cobj->version);
    free(cobj->host);
    free(cobj->status);
    free(cobj->content);
    free(cobj);
}

/* --------------------- Cache Function Definitions -------------------- */

/* Cache_new
 *      Purpose: Allocates space for a new cache and sets the caches size and
 *               length
 *   Parameters: The size of the cache that will be created
 *      Returns: The cache
 */
Cache Cache_new(int size)
{
    Cache cache  = malloc(sizeof(*cache));
    cache->table = malloc(sizeof(cache->table) * size);
    Cache_init(cache, size);
    cache->size = size;
    cache->len  = 0;

    return cache;
}

/* Cache_free
 *      Purpose: Deallocates all objects in the cache including the cache itself
 *   Parameters: The cache to free
 *      Returns: None
 */
void Cache_free(Cache cache)
{
    for(unsigned i = 0; i < cache->size; i++) {
        if (cache->table[i] != NULL) {
            CacheObject_free(cache->table[i]);
        }
    }
    free(cache->table);
    free(cache);
}

/* Cache_update
 *      Purpose: Updates the current age of each object in the cache
 *   Parameters: The cache to update
 *      Returns: None
 */
void Cache_update(Cache cache)
{
    sleep(1);
    int size = cache->size;
    long curr_time;
    for (int i = 0; i < size; i++) {
        if (cache->table[i] != NULL) {
            curr_time = time(NULL);
            cache->table[i]->curr_age = curr_time - 
                                        (cache->table[i]->start_age);
        }
    }
}

/* Cache_put
 *      Purpose: Inserts an objet into the cache table
 *   Parameters: The cache to insert the object into, and the object to insert
 *      Returns: None
 */
void Cache_put(Cache cache, HTTP_R cobj)
{    
    int cache_size = cache->size;
    int hash = cobj->hash;

    /* Find Available Spot in HashTable for New Object */
    for (int i = 0; i < cache_size; i++) {
        hash++;
        if (cache->table[hash % cache_size] == NULL) {
            cache->table[hash % cache_size] = cobj;
            cache->len++;
            return;
        } else if (cache->table[hash % cache_size]->max_age < 
                   cache->table[hash % cache_size]->curr_age) {
            CacheObject_free(cache->table[hash % cache_size]);
            cache->table[hash % cache_size] = cobj;
            return;
        }
    }

    /* HashTable is Full of Valid Objects -- Evict the Oldest */
    HTTP_R *old = cache->table;
    for (int i = 0; i < cache_size; i++) {
        if ((*old)->curr_age < cache->table[i]->curr_age) {
            old = cache->table + i;
        }
    }
    CacheObject_free(*old);
    *old = cobj;
}

/* Cache_get
 *      Purpose: Determines if the object passed (cobj) is in the cache, if so
 *               it returns 1 (true), otherwise, this function calls another
 *               function to add the object to the cache.
 *   Parameters: The cache and the cache object to query
 *      Returns: Object if object is in cache, else returns NULL
 */
HTTP_R Cache_get(Cache cache, HTTP_R cobj)
{
    int hash = cobj->hash;
    int size = cache->size;

    for (int i = 0; i < size; i++) {
        hash++;
        if (cache->table[hash % size] != NULL) {
            if (cache->table[hash % size]->hash == cobj->hash) {
                //cache->table[hash % size]->curr_age = 0;
                return cache->table[hash % size];
            }
        }
    }

    Cache_put(cache, cobj);

    return NULL;
}

/* Cache_length
 *      Purpose: Returns the number of items in the cache
 *   Parameters: The cache to query
 *      Returns: Returns the length (i.e. number of items) in the cache
 */
int Cache_length(Cache cache)
{
    return cache->len;
}

/* Cache_size
 *      Purpose: Returns the capacity of the cache
 *   Parameters: The cache to size
 *      Returns: The capcity (i.e. size) of the cache
 */
int Cache_size(Cache cache)
{
    return cache->size;
}

/* hash
 *      Purpose: Simple hash function to hash HTTP_R url 
 *   Parameters: url string
 *      Returns: Hash value for respective url string
 */
int Cache_hash(char *url, int port)
{
    int url_sum = 0;
    if (url != NULL) {
        size_t url_l = strlen(url);
        for (int i = 0; i < url_l; i++) {
            url_sum += (int) url[i];
        }
    }

    return url_sum + port;
}

/* Cache_init
 *      Purpose: Initialize Cache to NULL
 *   Parameters: Pointer to table in cache
 *      Returns: Hash value for respective url string
 */
void Cache_init(Cache cache, int size)
{
    for (int i = 0; i < size; i++) {
        cache->table[i] = NULL;
    }
}
