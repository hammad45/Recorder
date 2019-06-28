#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "recorder.h"

/* Global handle for the local trace log */
static FILE *__datafh;
static FILE *__metafh;

/* Global (defined in recorder.h) function name to id map */
hashmap_map *__func2id_map;
/* Global (defined in recorder.h) filename to id map */
hashmap_map *__filename2id_map;

typedef struct IoOperation {
    unsigned char func_id;
    unsigned char filename_id;
    double start_time;
    double end_time;
    /**
     * attr1 = offset for read/write/seek
     * attr1 = mode for open
     *
     * attr2 = counts for read/write
     * attr2 = whence for seek
     */
    size_t attr1;
    size_t attr2;
} IoOperation_t;


static inline unsigned char get_func_id(const char *func) {
    int id = -1;
    if (!func || !__func2id_map)
        return id;

    /* filename already exists */
    if (hashmap_get(__func2id_map, func, &id) == MAP_OK)
        return id;

    /* insert filename into the map */
    id = hashmap_length(__func2id_map);
    hashmap_put(__func2id_map, func, id);
    return id;
}

static inline unsigned char get_filename_id(const char *filename) {
    int id = -1;
    if (!filename || !__filename2id_map)
        return id;

    /* filename already exists */
    if (hashmap_get(__filename2id_map, filename, &id) == MAP_OK)
        return id;

    /* insert filename into the map */
    id = hashmap_length(__filename2id_map);
    hashmap_put(__filename2id_map, filename, id);
    return id;
}


static inline void write_in_binary(IoOperation_t *op) {
    if (__datafh != NULL ) {
        MAP_OR_FAIL(fwrite)
        RECORDER_MPI_CALL(fwrite) (op, sizeof(IoOperation_t), 1, __datafh);
    }
}

static inline void write_in_text(double tstart, double tend, const char* log_text) {
    if (__datafh != NULL)
        fprintf(__datafh, "%.6f %s %.6f\n", tstart, log_text, tend-tstart);
}

void write_data_operation(const char *func, const char *filename, double start, double end,
                          size_t offset, size_t count_or_whence, const char *log_text) {
    IoOperation_t op = {
        .func_id = get_func_id(func),
        .filename_id = get_filename_id(filename),
        .start_time = start,
        .end_time = end,
        .attr1 = offset,
        .attr2 = count_or_whence
    };
    #ifndef DISABLE_POSIX_TRACE
    //write_in_text(start, end, log_text);
    write_in_binary(&op);
    #endif
}

void logger_init(int rank) {
    __func2id_map = hashmap_new();
    __filename2id_map = hashmap_new();

    MAP_OR_FAIL(fopen)
    if (rank == 0) {
        mkdir("logs", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    char logfile_name[100];
    char metafile_name[100];
    sprintf(logfile_name, "logs/%d.itf", rank);
    sprintf(metafile_name, "logs/%d.mt", rank);
    __datafh = RECORDER_MPI_CALL(fopen) (logfile_name, "wb");
    __metafh = RECORDER_MPI_CALL(fopen) (metafile_name, "w+");
}

void logger_exit() {
    /* Write out the function and filename mappings */
    int i;
    if (hashmap_length(__func2id_map) <= 0 ) return;
    for(i = 0; i< __func2id_map->table_size; i++) {
        if(__func2id_map->data[i].in_use != 0) {
            const char *func = __func2id_map->data[i].key;
            int id = __func2id_map->data[i].data;
            fprintf(__metafh, "%s %d\n", func, id);
        }
    }
    if (hashmap_length(__filename2id_map) <= 0 ) return;
    for(i = 0; i< __filename2id_map->table_size; i++) {
        if(__filename2id_map->data[i].in_use != 0) {
            const char *func = __filename2id_map->data[i].key;
            int id = __filename2id_map->data[i].data;
            fprintf(__metafh, "%s %d\n", func, id);
        }
    }

    hashmap_free(__func2id_map);
    hashmap_free(__filename2id_map);

    MAP_OR_FAIL(fclose)
    if ( __metafh)
        RECORDER_MPI_CALL(fclose) (__metafh);

    // Close the log file would cause a segmentation error
    if ( __datafh )
        RECORDER_MPI_CALL(fclose) (__datafh);
}
