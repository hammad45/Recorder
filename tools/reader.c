#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "./reader.h"

void read_global_metadata(char* path, RecorderGlobalDef *RGD) {
    FILE* fp = fopen(path, "r+b");
    fread(RGD, sizeof(RecorderGlobalDef), 1, fp);
    fclose(fp);
}

void read_func_list(char* path, RecorderReader *reader) {
    FILE* fp = fopen(path, "r+b");

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp) - sizeof(RecorderGlobalDef);
    char buf[fsize];

    fseek(fp, sizeof(RecorderGlobalDef), SEEK_SET); // skip GlobalDef object
    fread(buf, 1, fsize, fp);

    int start_pos = 0, end_pos = 0;
    int func_id = 0;

    for(end_pos = 0; end_pos < fsize; end_pos++) {
        if(buf[end_pos] == '\n') {
            memset(reader->func_list[func_id], 0, sizeof(reader->func_list[func_id]));
            memcpy(reader->func_list[func_id], buf+start_pos, end_pos-start_pos);
            start_pos = end_pos+1;
            func_id++;
        }
    }

    fclose(fp);
}

void recorder_init_reader(const char* logs_dir, RecorderReader *reader) {

    char global_metadata_file[256];
    strcpy(reader->logs_dir, logs_dir);

    RecorderGlobalDef RGD;
    sprintf(global_metadata_file, "%s/recorder.mt", logs_dir);
    read_global_metadata(global_metadata_file, &RGD);
    reader->total_ranks = RGD.total_ranks;
    reader->time_resolution = RGD.time_resolution;

    read_func_list(global_metadata_file, reader);
}

void recorder_free_reader(RecorderReader *reader) {
}

const char* recorder_get_func_name(RecorderReader* reader, int func_id) {
    return reader->func_list[func_id];
}

void cs_to_record(CallSignature *cs, Record *record) {
    char* key = cs->key;

    int pos = 0;
    memcpy(&record->func_id, key+pos, sizeof(record->func_id));
    pos += sizeof(record->func_id);
    memcpy(&record->res, key+pos, sizeof(record->res));
    pos += sizeof(record->res);
    memcpy(&record->arg_count, key+pos, sizeof(record->arg_count));
    pos += sizeof(record->arg_count);

    record->args = malloc(sizeof(char*) * record->arg_count);

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
}

void recorder_free_cst(CST* cst) {
    for(int i = 0; i < cst->entries; i++)
        free(cst->cst_list[i].key);
    free(cst->cst_list);
}

void recorder_free_cfg(CFG* cfg) {
    RuleHash *r, *tmp;
    HASH_ITER(hh, cfg->cfg_head, r, tmp) {
        HASH_DEL(cfg->cfg_head, r);
        free(r->rule_body);
        free(r);
    }
}

void recorder_free_record(Record* r) {
    for(int i = 0; i < r->arg_count; i++)
        free(r->args[i]);
    free(r->args);
}

void recorder_read_cst(RecorderReader *reader, int rank, CST *cst) {
    cst->rank = rank;
    char cst_filename[1096] = {0};
    sprintf(cst_filename, "%s/%d.cst", reader->logs_dir, rank);

    FILE* f = fopen(cst_filename, "rb");

    int key_len, terminal;
    fread(&cst->entries, sizeof(int), 1, f);

    cst->cst_list = malloc(cst->entries * sizeof(CallSignature));

    for(int i = 0; i < cst->entries; i++) {
        fread(&cst->cst_list[i].terminal, sizeof(int), 1, f);
        fread(&cst->cst_list[i].key_len, sizeof(int), 1, f);

        cst->cst_list[i].key = malloc(cst->cst_list[i].key_len);
        fread(cst->cst_list[i].key, 1, cst->cst_list[i].key_len, f);

        assert(terminal < cst->entries);
    }
    fclose(f);

    //for(int i = 0; i < cst->entries; i++)
    //    printf("%d, terminal %d, key len: %d\n", i, cst->cst_list[i].terminal, cst->cst_list[i].key_len);
}

void recorder_read_cfg(RecorderReader *reader, int rank, CFG* cfg) {
    cfg->rank = rank;
    char cfg_filename[1096] = {0};
    sprintf(cfg_filename, "%s/%d.cfg", reader->logs_dir, rank);

    FILE* f = fopen(cfg_filename, "rb");

    fread(&cfg->rules, sizeof(int), 1, f);

    cfg->cfg_head = NULL;
    for(int i = 0; i < cfg->rules; i++) {
        RuleHash *rule = malloc(sizeof(RuleHash));

        fread(&(rule->rule_id), sizeof(int), 1, f);
        fread(&(rule->symbols), sizeof(int), 1, f);
        //printf("rule id: %d, symbols: %d\n", rule->rule_id, rule->symbols);

        rule->rule_body = (int*) malloc(sizeof(int)*rule->symbols*2);
        fread(rule->rule_body, sizeof(int), rule->symbols*2, f);
        HASH_ADD_INT(cfg->cfg_head, rule_id, rule);
    }
    fclose(f);
}


#define TERMINAL_START_ID 0

void rule_application(RecorderReader* reader, RuleHash* rules, int rule_id, CallSignature *cst_list, FILE* ts_file,
                      void (*user_op)(Record*, void*), void* user_arg) {

    RuleHash *rule = NULL;
    HASH_FIND_INT(rules, &rule_id, rule);
    assert(rule != NULL);

    for(int i = 0; i < rule->symbols; i++) {
        int sym_val = rule->rule_body[2*i+0];
        int sym_exp = rule->rule_body[2*i+1];
        if (sym_val >= TERMINAL_START_ID) { // terminal
            for(int j = 0; j < sym_exp; j++) {
                Record record;
                cs_to_record(&cst_list[sym_val], &record);

                // Fill in timestamps
                int ts[2];
                fread(ts, sizeof(int), 2, ts_file);
                record.tstart = ts[0] * reader->time_resolution;
                record.tend   = ts[1] * reader->time_resolution;

                user_op(&record, user_arg);

                recorder_free_record(&record);
            }
        } else {                            // non-terminal (i.e., rule)
            for(int j = 0; j < sym_exp; j++)
                rule_application(reader, rules, sym_val, cst_list, ts_file, user_op, user_arg);
        }
    }
}

void recorder_decode_records(RecorderReader *reader, CST *cst, CFG *cfg,
                             void (*user_op)(Record*, void*), void* user_arg) {

    assert(cst->rank == cfg->rank);

    char ts_filename[1096] = {0};
    sprintf(ts_filename, "%s/%d.ts", reader->logs_dir, cst->rank);
    FILE* ts_file = fopen(ts_filename, "rb");

    rule_application(reader, cfg->cfg_head, -1, cst->cst_list, ts_file, user_op, user_arg);

    fclose(ts_file);
}

