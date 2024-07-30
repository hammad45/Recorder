#include <stdio.h>
#include <sqlite3.h> 
#include "recorder-analysis.h"
#include "mpi.h"


#define SIZE 100
#define THRESHOLD 4

static RuleConfidence* cfg_conf = NULL;

static int top = -1;

void push(int x, int stack[])
{
    if (top != SIZE - 1)
    {
        top = top + 1;
        stack[top] = x;
    }
}

void pop(int stack[])
{
    if (top != -1)
    {
        printf("\nPopped element: %d", stack[top]);
        top = top - 1;
    }
}

void show(int stack[])
{
    if (top != -1)
    {
        printf("\nElements present in the stack: \n");
        for (int i = top; i >= 0; --i)
            printf("%d\n", stack[i]);
    }
}

int custom_cmp(Symbol* matched_rule, int val){
    if (matched_rule->val == val){
        return 0;
    }
    return 1;
}

void extract_io_behavior(RecorderLogger* logger, int stack[], Knowledge *knowledge){
    int tmp_stack[SIZE];
    int tmp_top = top;

    for (int i = 0; i <= top; i++) {
        tmp_stack[i] = stack[i];
    }

    int terminal_id = 0;

    for (int i = 0; i <= tmp_top; i++){
        terminal_id = tmp_stack[i];
        
        CallSignature *s;
        for (s = logger->cst; s != NULL; s = (CallSignature *)(s->hh.next)) {
            if (s->terminal_id == terminal_id){
                Record * record = cs_to_record(s);
                const char * func_name = get_function_name_by_id(record->func_id);

                if (strcmp(func_name, "MPI_File_write_at_all") == 0 || strcmp(func_name, "MPI_File_read_at_all") == 0){
                    knowledge->collective = 1;
                }
                else if (strcmp(func_name, "MPI_File_write_at") == 0 || strcmp(func_name, "MPI_File_read_at") == 0){
                    knowledge->collective = 0;
                }
                else if (strcmp(func_name, "write") == 0){
                    sprintf(knowledge->operation, "write");
                    sprintf(knowledge->file_name, record->args[0]);
                    knowledge->transfer_size = record->args[2];
                }
                else if (strcmp(func_name, "read") == 0){
                    sprintf(knowledge->operation, "read");
                    sprintf(knowledge->file_name, record->args[0]);
                }
            }  
        } 
    }
}

void find_rule_confidence(RecorderLogger* logger, CallSignature *entry){
    Symbol *rule, *sym, *firstRule;
    int rules_count = 0;
    DL_COUNT(logger->cfg.rules, rule, rules_count);

    bool first = true;
    bool found = false;

    if (rules_count > 1){
        DL_FOREACH(logger->cfg.rules, rule) {
            if (first){
                firstRule = rule;
                first = false;
            }
            else {
                int count = 0;
                DL_FOREACH(firstRule->rule_body, sym) {
                    if (sym->val == rule->val){
                        if (sym->exp > 0){
                            count += sym->exp;
                        }
                        else{
                            count += 1;
                        }
                    }
                }

                RuleConfidence *entry = NULL;
                HASH_FIND(hh, cfg_conf, &rule->val, sizeof(int), entry);
                if(entry) {    
                    entry->count = count;
                } else {                          
                    entry = (RuleConfidence*) recorder_malloc(sizeof(RuleConfidence));
                    entry->rule = rule->val;
                    entry->count = count; 
                    entry->probability = 0;
                    HASH_ADD(hh, cfg_conf, rule, sizeof(int), entry);
                }
            }
        }

        RuleConfidence *s;
        for (s = cfg_conf; s != NULL; s = (RuleConfidence *)(s->hh.next)) {
            found = false;
            DL_FOREACH(logger->cfg.rules, rule) {
                if (s->rule == rule->val){
                    found = true;
                    break;
                }
            }
            if (!found){
                RuleConfidence *rs, *tmp;
                HASH_ITER(hh, cfg_conf, rs, tmp) {
                    if (rs->rule == s->rule){
                        HASH_DEL(cfg_conf, rs);
                    }
                }
            }
        }

        // for (s = logger->cfg_conf; s != NULL; s = (RuleConfidence *)(s->hh.next)) {
        //     printf("Rule %d, Count %d, Probability %d\n", s->rule, s->count, s->probability);  
        // }
    }
}

void find_pattern_within(RecorderLogger* logger, Symbol* rule, int stack[]){
    Symbol *sym;
    DL_FOREACH(rule->rule_body, sym) {
        if (sym->val < 0) {
            Symbol *matched_rule;
            DL_SEARCH(logger->cfg.rules, matched_rule, sym->val, custom_cmp);
            if (matched_rule){
                find_pattern_within(logger, matched_rule, stack);
            }
        }
        push(sym->val, stack);
    }
}

int find_pattern_start(RecorderLogger* logger, int search_value, bool initial, int stack[]){
    Symbol *rule, *sym;
    int target_rule = 0, rules_count = 0;
    DL_COUNT(logger->cfg.rules, rule, rules_count);

    if (initial && rules_count > 1){
        bool first = true;
        DL_FOREACH(logger->cfg.rules, rule) {
            if (!first) {
                DL_FOREACH(rule->rule_body, sym) {
                    if (sym->val == search_value){
                        target_rule = rule->val;     
                        return target_rule;
                    }
                    else {
                        break;
                    }  
                }
            }
            first = false;
        }  
    }
    else if (!initial){
        DL_FOREACH(logger->cfg.rules, rule) {
            if (rule->val == search_value) {
                DL_FOREACH(rule->rule_body, sym) {
                    if (sym->val < 0) {
                        Symbol *matched_rule;
                        DL_SEARCH(logger->cfg.rules, matched_rule, sym->val, custom_cmp);
                        if (matched_rule){
                            find_pattern_within(logger, matched_rule, stack);
                        }   
                    }
                    else {
                        push(sym->val, stack);
                    }  
                }
            }
        }
    }
    return 0;
}

void analysis_cs(RecorderLogger* logger, CallSignature *entry, Knowledge *knowledge){
    // Symbol *rule1, *sym1;
    // int rules_count1 = 0, symbols_count1 = 0;
    // DL_COUNT(logger->cfg.rules, rule1, rules_count1);

    // DL_FOREACH(logger->cfg.rules, rule1) {
    //     int count1;
    //     DL_COUNT(rule1->rule_body, sym1, count1);
    //     symbols_count1 += count1;

    //     RECORDER_LOGINFO("Rule %d :-> ", rule1->val);

    //     DL_FOREACH(rule1->rule_body, sym1) {
    //         if(sym1->exp > 1)
    //             RECORDER_LOGINFO("%d^%d ", sym1->val, sym1->exp);
    //         else
    //             RECORDER_LOGINFO("%d ", sym1->val);
    //     }
    //     RECORDER_LOGINFO("\n");
    // }

    int stack[SIZE];
    top = -1;

    int target_rule = find_pattern_start(logger, entry->terminal_id, true, stack);
    int max_rule = target_rule;
    int max_rule_count = 0;

    RuleConfidence *s = NULL;
    HASH_FIND_INT(cfg_conf, &target_rule, s);
    if (s){
        max_rule_count = s->count;
    }

    if (target_rule < -1){
        Symbol *rule, *sym;
        bool first = true;

        DL_FOREACH(logger->cfg.rules, rule) {
            if (!first) {
                DL_FOREACH(rule->rule_body, sym) {
                    if (sym->val == target_rule){
                        RuleConfidence *s = NULL;
                        HASH_FIND_INT(cfg_conf, &rule->val, s);
                        if (s){
                            if (s->count > max_rule_count){
                                max_rule = s->rule;
                                max_rule_count = s->count;
                            }
                        }
                    }
                    else {
                        break;
                    }    
                }
            }
            first = false;
        }

        if (max_rule_count > THRESHOLD){
            // printf("Chosen rule %d, count %d, terminal ID %d , Rank %d\n\n", max_rule, max_rule_count, entry->terminal_id, logger->rank);
            find_pattern_start(logger, max_rule, false, stack);
        }
    }

    if (top != -1){
        extract_io_behavior(logger, stack, knowledge);
    }
}

int recorder_analysis(RecorderLogger* logger, Record* record, CallSignature* entry, Knowledge *knowledge, HDF5Optimizations* hdf5_optimizations) {
    
    
    const char * func_name = get_function_name_by_id(record->func_id);
    size_t len = sizeof(collective_func_list) / sizeof(char *);
    
    if ((strcmp(func_name, "H5Pcreate") == 0)){
        char *temp_dcpl_id = (char*) record->args[0];
        knowledge->dcpl_id = strtol(temp_dcpl_id, NULL, 10);
    }

    if ((strcmp(func_name, "H5Pset_fapl_mpio") == 0)){
        char *temp_fapl_Id = (char*) record->args[0];
        knowledge->faplID = strtol(temp_fapl_Id, NULL, 10);
    }


    unsigned char i;
    bool is_collective = false;
    for(i = 0; i < len; i++) {
        if (strcmp(collective_func_list[i], func_name) == 0){
            is_collective = true;
            break;
        }
    }

    if(is_collective){  
        int* recvcounts = NULL;
        if (logger->rank == 0){
            recvcounts = (int *) recorder_malloc(logger->nprocs*sizeof(int)); 
        }
        int mylen = strlen(knowledge->file_name); 

        GOTCHA_SET_REAL_CALL(MPI_Gather, RECORDER_MPI);
        GOTCHA_REAL_CALL(MPI_Gather)(&mylen,1,MPI_INT,recvcounts,1,MPI_INT,0,MPI_COMM_WORLD);
        
        int totlen = 0; 
        int* displs = NULL;
        char* totalstring = NULL;

        if(logger->rank == 0)
        {   
            displs = (int *) recorder_malloc(logger->nprocs*sizeof(int));
            displs[0] = 0; 
            totlen += recvcounts[0] + 1; 
            for(int i=1; i < logger->nprocs; i++)
            {
                totlen += recvcounts[i]+1; 
                displs[i] = displs[i-1] + recvcounts[i-1] + 1; 
            }
            
            totalstring = (char *) recorder_malloc(totlen*sizeof(char)); 
            for (int i = 0; i < totlen - 1; i++) {
                totalstring[i] = ' '; 
            
            totalstring[totlen - 1] = '\0'; 
            }
        }

        GOTCHA_SET_REAL_CALL(MPI_Gatherv, RECORDER_MPI);
        GOTCHA_REAL_CALL(MPI_Gatherv)(&knowledge->file_name, mylen, MPI_CHAR,
                    totalstring, recvcounts, displs, MPI_CHAR,
                    0, MPI_COMM_WORLD);

        if (logger->rank == 0){
            char * token = strtok(totalstring, " ");
            char * first_token = token;
            bool shared_file = true;
            while( token != NULL ) {
                // printf( " %s\n", token ); 
                token = strtok(NULL, " ");
                if (token != NULL){
                    if (strcmp(first_token, token) != 0){
                        shared_file = false;
                        break;
                    }
                } 
            }

            if (!shared_file){
                sprintf(knowledge->file_operation, "file-per-process");
            } else {
                sprintf(knowledge->file_operation, "shared_file");
            }

            // printf("Knowledge for rank %d --> Operation %s, Transfer Size %d, Collective I/O %d, Spatial Locality %s, File Operation %s, File Name %s \n", 
            //         logger->rank, knowledge->operation, knowledge->transfer_size, knowledge->collective, knowledge->spatial_locality, knowledge->file_operation, knowledge->file_name);
            
            free(totalstring);
            free(displs);
            free(recvcounts);
        }

        if (strcmp(knowledge->operation, "write") == 0){
            if (knowledge->transfer_size < 16777216){
                if (hdf5_optimizations->alignment == false){
                    GOTCHA_SET_REAL_CALL(H5Pset_alignment, RECORDER_HDF5);
                    GOTCHA_REAL_CALL(H5Pset_alignment)(knowledge->faplID, 1, 1);
                    hdf5_optimizations->alignment = true;
                }
            }
            else {
                if (hdf5_optimizations->alignment == false){
                    GOTCHA_SET_REAL_CALL(H5Pset_alignment, RECORDER_HDF5);
                    GOTCHA_REAL_CALL(H5Pset_alignment)(knowledge->faplID, 1, 1);
                    hdf5_optimizations->alignment = true;
                }

                if (!hdf5_optimizations->chunking){
                    hsize_t	chunk_dims[2] = {2, 2};
                    GOTCHA_SET_REAL_CALL(H5Pset_chunk, RECORDER_HDF5);
                    GOTCHA_REAL_CALL(H5Pset_chunk)(knowledge->dcpl_id, 2, chunk_dims);
                    hdf5_optimizations->chunking = true;
                }
            }
        }
    }

    find_rule_confidence(logger, entry);
    analysis_cs(logger, entry, knowledge);    
    
    return 0;
}
