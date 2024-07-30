#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "recorder.h"
#include "recorder-sequitur.h"


/**
 * Key:
 *   thread id:     sizeof(pthread_t)
 *   func id:       sizeof(record->func_id)
 *   call_depth:    sizeof(record->call_depth)
 *   arg count:     sizeof(record->arg_count)
 *   arg strlen:    sizeof(int)
 *   args:          arg_strlen
 *
 * arguments seperated by space ' '
 */
int cs_key_args_start() {
    Record r;
    size_t args_start = sizeof(pthread_t) + sizeof(r.func_id) +
                  sizeof(r.call_depth) + sizeof(r.arg_count) + sizeof(int);
    return ((int)args_start);
}

int cs_key_args_strlen(Record* record) {
    int arg_count = record->arg_count;
    char **args = record->args;

    char invalid_str[] = "???";
    int invalid_str_len = strlen(invalid_str);

    int args_strlen = arg_count;
    for(int i = 0; i < arg_count; i++) {
        if(args[i]) {
            for(int j = 0; j < strlen(args[i]); j++)
                if(args[i][j] == ' ') args[i][j] = '_';
            args_strlen += strlen(args[i]);
        } else {
            args_strlen += strlen(invalid_str);
        }
    }

    return args_strlen;
}

int cs_key_length(Record* record) {
    int key_len = cs_key_args_start() + cs_key_args_strlen(record);
    return key_len;
}

char* compose_cs_key(Record* record, int* key_len) {
    int arg_count = record->arg_count;
    char **args = record->args;

    char invalid_str[] = "???";
    int invalid_str_len = strlen(invalid_str);

    int args_strlen = cs_key_args_strlen(record);
    *key_len = cs_key_length(record);

    char* key = recorder_malloc(*key_len);
    int pos = 0;
    memcpy(key+pos, &record->tid, sizeof(pthread_t));
    pos += sizeof(pthread_t);
    memcpy(key+pos, &record->func_id, sizeof(record->func_id));
    pos += sizeof(record->func_id);
    memcpy(key+pos, &record->call_depth, sizeof(record->call_depth));
    pos += sizeof(record->call_depth);
    memcpy(key+pos, &record->arg_count, sizeof(record->arg_count));
    pos += sizeof(record->arg_count);
    memcpy(key+pos, &args_strlen, sizeof(int));
    pos += sizeof(int);

    for(int i = 0; i < arg_count; i++) {
        if(args[i]) {
            memcpy(key+pos, args[i], strlen(args[i]));
            pos += strlen(args[i]);
        } else {
            memcpy(key+pos, invalid_str, strlen(invalid_str));
            pos += invalid_str_len;
        }
        key[pos] = ' ';
        pos += 1;
    }

    return key;
}

// Construct a Recorder* from a call signature key
// Caller needs to free the record after use
Record* cs_to_record(CallSignature *cs) {

    Record *record = recorder_malloc(sizeof(Record));
    record->res = NULL; // we don't keep return value in Call Signature

    char* key = cs->key;

    int pos = 0;
    memcpy(&record->tid, key+pos, sizeof(pthread_t));
    pos += sizeof(pthread_t);
    memcpy(&record->func_id, key+pos, sizeof(record->func_id));
    pos += sizeof(record->func_id);
    memcpy(&record->call_depth, key+pos, sizeof(record->call_depth));
    pos += sizeof(record->call_depth);
    memcpy(&record->arg_count, key+pos, sizeof(record->arg_count));
    pos += sizeof(record->arg_count);

    record->args = recorder_malloc(sizeof(char*) * record->arg_count);

    int arg_strlen;
    memcpy(&arg_strlen, key+pos, sizeof(int));
    pos += sizeof(int);

    char* arg_str = key+pos;
    int ai = 0;
    int start = 0;
    for(int i = 0; i < arg_strlen; i++) {
        if(arg_str[i] == ' ') {
            record->args[ai++] = strndup(arg_str+start, (i-start));
            start = i + 1;
        }
    }

    assert(ai == record->arg_count);
    return record;
}

void cleanup_cst(CallSignature* cst) {
    CallSignature *entry, *tmp;
    HASH_ITER(hh, cst, entry, tmp) {
        HASH_DEL(cst, entry);
        recorder_free(entry->key, entry->key_len);
        recorder_free(entry, sizeof(CallSignature));
    }
    cst = NULL;
}

void* serialize_cst(CallSignature *cst, size_t *len) {
    *len = sizeof(int);

    CallSignature *entry, *tmp;
    HASH_ITER(hh, cst, entry, tmp) {
        *len = *len + entry->key_len + sizeof(int)*3 + sizeof(unsigned);
    }

    int entries = HASH_COUNT(cst);
    void *res = recorder_malloc(*len);
    void *ptr = res;

    memcpy(ptr, &entries, sizeof(int));
    ptr += sizeof(int);

    HASH_ITER(hh, cst, entry, tmp) {

        memcpy(ptr, &entry->terminal_id, sizeof(int));
        ptr = ptr + sizeof(int);

        memcpy(ptr, &entry->rank, sizeof(int));
        ptr = ptr + sizeof(int);

        memcpy(ptr, &entry->key_len, sizeof(int));
        ptr = ptr + sizeof(int);

        memcpy(ptr, &entry->count, sizeof(unsigned));
        ptr = ptr + sizeof(unsigned);

        memcpy(ptr, entry->key, entry->key_len);
        ptr = ptr + entry->key_len;
    }

    return res;
}

CallSignature* deserialize_cst(void *data) {
    int num;
    memcpy(&num, data, sizeof(int));

    void *ptr = data + sizeof(int);

    CallSignature *cst= NULL, *entry = NULL;
    for(int i = 0; i < num; i++) {
        entry = recorder_malloc(sizeof(CallSignature));

        memcpy( &(entry->terminal_id), ptr, sizeof(int) );
        ptr += sizeof(int);

        memcpy( &(entry->rank), ptr, sizeof(int) );
        ptr += sizeof(int);

        memcpy( &(entry->key_len), ptr, sizeof(int) );
        ptr += sizeof(int);

        memcpy( &(entry->count), ptr, sizeof(unsigned) );
        ptr += sizeof(unsigned);

        entry->key = recorder_malloc(entry->key_len);
        memcpy( entry->key, ptr, entry->key_len );
        ptr += entry->key_len;

        HASH_ADD_KEYPTR(hh, cst, entry->key, entry->key_len, entry);
    }

    return cst;
}


void save_cst_local(RecorderLogger* logger) {
    FILE* f = GOTCHA_REAL_CALL(fopen) (logger->cst_path, "wb");
    size_t len;
    void* data = serialize_cst(logger->cst, &len);
    recorder_write_zlib((unsigned char*)data, len, f);
    GOTCHA_REAL_CALL(fclose)(f);
}

CallSignature* copy_cst(CallSignature* origin) {
    CallSignature* cst = NULL;
    CallSignature *entry, *tmp, *new_entry;
    HASH_ITER(hh, origin, entry, tmp) {
        new_entry = recorder_malloc(sizeof(CallSignature));
        new_entry->terminal_id = entry->terminal_id;
        new_entry->key_len = entry->key_len;
        new_entry->rank = entry->rank;
        new_entry->count = entry->count;
        new_entry->key = recorder_malloc(entry->key_len);
        memcpy(new_entry->key, entry->key, entry->key_len);
        HASH_ADD_KEYPTR(hh, cst, new_entry->key, new_entry->key_len, new_entry);
    }
    return cst;
}

CallSignature* compress_csts(RecorderLogger* logger) {

    int my_rank = logger->rank;
    int other_rank;
    int mask = 1;
    bool done = false;

    int phases = recorder_ceil(recorder_log2(logger->nprocs));

    CallSignature* merged_cst = copy_cst(logger->cst);

    for(int k = 0; k < phases; k++, mask*=2) {
        if(done) break;

        other_rank = my_rank ^ mask;     // other_rank = my_rank XOR 2^k

        if(other_rank >= logger->nprocs) continue;

        size_t size, remain_cst_size;
        void* buf;
        void* buf_ptr;

        // bigger ranks send to smaller ranks
        if(my_rank < other_rank) {
            recorder_recv(&size, sizeof(size), other_rank, mask, MPI_COMM_WORLD);
            buf = recorder_malloc(size);
            recorder_recv(buf, size, other_rank, mask, MPI_COMM_WORLD);

            int cst_rank, entries, key_len;
            unsigned count;
            void *ptr = buf;
            memcpy(&entries, ptr, sizeof(int));
            ptr = ptr + sizeof(int);
            for(int i = 0; i < entries; i++) {
                // skip 4 bytes terminal id
                ptr = ptr + sizeof(int);

                // 4 bytes rank
                memcpy(&cst_rank, ptr, sizeof(int));
                ptr = ptr + sizeof(int);

                // 4 bytes key length
                memcpy(&key_len, ptr, sizeof(int));
                ptr = ptr + sizeof(int);

                // 4 bytes count
                memcpy(&count, ptr, sizeof(unsigned));
                ptr = ptr + sizeof(unsigned);

                // key length bytes key
                void *key = recorder_malloc(key_len);
                memcpy(key, ptr, key_len);
                ptr = ptr + key_len;

                // Check to see if this function entry is already in the cst
                CallSignature *entry = NULL;
                HASH_FIND(hh, merged_cst, key, key_len, entry);
                if(entry) {
                    recorder_free(key, key_len);
                    entry->count += count;
                } else {                                // Not exist, add to cst
                    entry = (CallSignature*) recorder_malloc(sizeof(CallSignature));
                    entry->key = key;
                    entry->key_len = key_len;
                    entry->rank = cst_rank;
                    entry->count = count;
                    HASH_ADD_KEYPTR(hh, merged_cst, key, key_len, entry);

                    //*key_len = sizeof(pthread_t) + sizeof(record->func_id) + sizeof(record->call_depth) +
                    //           sizeof(record->arg_count) + sizeof(int) + arg_strlen;
                    /*
                    pthread_t thread_id;
                    unsigned char func_id, call_depth, arg_count;
                    int arg_strlen;
                    memcpy(&thread_id, key, sizeof(thread_id));
                    memcpy(&func_id, key+sizeof(thread_id), sizeof(func_id));
                    memcpy(&call_depth, key+sizeof(thread_id)+sizeof(func_id), sizeof(call_depth));
                    memcpy(&arg_count, key+sizeof(thread_id)+sizeof(func_id)+sizeof(call_depth), sizeof(arg_count));
                    memcpy(&arg_strlen, key+sizeof(thread_id)+sizeof(func_id)+sizeof(call_depth)+sizeof(arg_count), sizeof(arg_strlen));
                    printf("CHEN2 rank:%d tid: %ld, func_id:%d, call_depth:%d, arg_count:%d, arg_strlen:%d, count:%u, key:%s\n",
                    cst_rank, thread_id, func_id, call_depth, arg_count, arg_strlen, count, ((char*)key)+15);
                    */
                }
            }
            recorder_free(buf, size);

        } else {   // SENDER
            buf = serialize_cst(merged_cst, &size);
            recorder_send(&size, sizeof(size), other_rank, mask, MPI_COMM_WORLD);
            recorder_send(buf, size, other_rank, mask, MPI_COMM_WORLD);
            recorder_free(buf, size);
            done = true;
        }
    }

    // Eventually the root (rank 0) will get the fully merged CST
    // Update (re-assign) terminal id for all unique signatures
    if(my_rank == 0) {
        //linear_regression(merged_cst);
        int terminal_id = 0;
        CallSignature *entry, *tmp;
        HASH_ITER(hh, merged_cst, entry, tmp) {
            entry->terminal_id = terminal_id++;
        }
    } else {
        cleanup_cst(merged_cst);
    }
    return merged_cst;
}


void save_cst_merged(RecorderLogger* logger) {
    // 1. Inter-process copmression for CSTs
    // Eventually, rank 0 will have the compressed cst.
    CallSignature* compressed_cst = compress_csts(logger);

    // 2. Broadcast the merged CST to all ranks
    size_t cst_stream_size;
    void *cst_stream;

    if(logger->rank == 0) {
        cst_stream = serialize_cst(compressed_cst, &cst_stream_size);

        recorder_bcast(&cst_stream_size, sizeof(cst_stream_size), 0, MPI_COMM_WORLD);
        recorder_bcast(cst_stream, cst_stream_size, 0, MPI_COMM_WORLD);

        // 3. Rank 0 write out the compressed CST
        errno = 0;
        char cst_fname[1096];
        sprintf(cst_fname, "%s/recorder.cst", logger->traces_dir);
        FILE *cst_file = fopen(cst_fname, "wb");
        if(cst_file) {
            recorder_write_zlib(cst_stream, cst_stream_size, cst_file);
            GOTCHA_REAL_CALL(fclose)(cst_file);
        } else {
            printf("[Recorder] Open file: %s failed, errno: %d\n", cst_fname, errno);
        }
    } else {
        recorder_bcast(&cst_stream_size, sizeof(cst_stream_size), 0, MPI_COMM_WORLD);
        cst_stream = recorder_malloc(cst_stream_size);
        recorder_bcast(cst_stream, cst_stream_size, 0, MPI_COMM_WORLD);

        // 3. Other rank get the compressed cst stream from rank 0
        // then convert it to the CST
        compressed_cst = deserialize_cst(cst_stream);
    }

    // 4. Update function entry's terminal id
    int *update_terminal_id = recorder_malloc(sizeof(int) * logger->current_cfg_terminal);
    CallSignature *entry, *tmp, *res;
    HASH_ITER(hh, logger->cst, entry, tmp) {
        HASH_FIND(hh, compressed_cst, entry->key, entry->key_len, res);
        if(res)
            update_terminal_id[entry->terminal_id] = res->terminal_id;
        else
            printf("[Recorder] %d Not possible! Not exist in merged cst?\n", logger->rank);
    }


    cleanup_cst(compressed_cst);
    recorder_free(cst_stream, cst_stream_size);

    sequitur_update(&(logger->cfg), update_terminal_id);
    recorder_free(update_terminal_id, sizeof(int)* logger->current_cfg_terminal);
}


void save_cfg_local(RecorderLogger* logger) {
    FILE* f = GOTCHA_REAL_CALL(fopen) (logger->cfg_path, "wb");
    int integers;
    int* data = serialize_grammar(&logger->cfg, &integers);
    recorder_write_zlib((unsigned char*)data, sizeof(int)*integers, f);
    GOTCHA_REAL_CALL(fclose)(f);
}

void save_cfg_merged(RecorderLogger* logger) {
    sequitur_save_unique_grammars(logger->traces_dir, &logger->cfg, logger->rank, logger->nprocs);
}
