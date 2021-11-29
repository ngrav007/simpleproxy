#ifndef _CACHETABLE_H_
#define _CACHETABLE_H_

#include <stdio.h>

typedef struct HTTP_R {
    char *method;
    char *url;
    char *version;
    char *host;
    char *status;
    char *content;
    int  port;
    int  status_l;
    int  content_l;
    int  hash; 
    int  max_age;
    int  start_age;
    int  curr_age;
    int  valid;
} *HTTP_R;

typedef struct Cache {
    HTTP_R *table;
    unsigned len;
    unsigned size;
} *Cache;

HTTP_R CacheObject_new(char *line);
Cache  Cache_new(int size);
void   CacheObject_free(HTTP_R cobj);
void   Cache_free(Cache cache);
void   Cache_update(Cache cache);
void   Cache_put(Cache cache, HTTP_R cobj);
HTTP_R Cache_get(Cache cache, HTTP_R cobj);

#endif
