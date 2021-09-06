#ifndef _RECORDER_READER_H_
#define _RECORDER_READER_H_
#include <stdbool.h>
#include "recorder-log-format.h"


#define POSIX_SEMANTICS 0
#define COMMIT_SEMANTICS 1
#define SESSION_SEMANTICS 2

typedef struct RecorderReader_t {
    int total_ranks;
    double time_resolution;
    char func_list[256][64];
    char logs_dir[1024];
} RecorderReader;

typedef struct Interval_t {
    int rank;
    int seqId;              // The sequence id of the I/O call
    double tstart;
    size_t offset;
    size_t count;
    bool isRead;
} Interval;

typedef struct IntervalsMap_t {
    char* filename;
    size_t num_intervals;
    Interval *intervals;    // Pointer to Interval, copied from vector<Interval>

    int *num_opens;         // num_opens[rank] is list of number of opens for rank
    int *num_closes;
    int *num_commits;

    double **topens;        // topens[rank] is a list of open timestamps for rank
    double **tcloses;
    double **tcommits;
} IntervalsMap;



typedef struct CallSignature_t {
    int terminal;
    int key_len;
    char* key;
} CallSignature;

typedef struct CST_t {
    int entries;
    CallSignature *cst_list;
} CST;

typedef struct RuleHash_t {
    int rule_id;
    int *rule_body;     // 2i+0: val of symbol i,  2i+1: exp of symbol i
    int symbols;        // There are a total of 2*symbols integers in the rule body
    UT_hash_handle hh;
} RuleHash;

typedef struct CFG_t {
    int rules;
    RuleHash* cfg_head;
} CFG;


void recorder_init_reader(const char* logs_dir, RecorderReader *reader);
void recorder_free_reader(RecorderReader *reader);

void recorder_read_cst(RecorderReader *reader, int rank, CST *cst);
void recorder_free_cst(CST *cst);

void recorder_read_cfg(RecorderReader *reader, int rank, CFG *cfg);
void recorder_free_cfg(CFG *cfg);

void recorder_decode_records(CST *cst, CFG *cfg, void (*user_op)(Record* r, int exp, void* user_arg), void* user_arg);
const char* recorder_get_func_name(RecorderReader* reader, int func_id);


IntervalsMap* build_offset_intervals(RecorderReader reader, int *num_files, int semantics);

#endif
