/*
 * File:   naive.h
 * Author: mk
 *
 * Created on September 20, 2015, 8:02 PM
 */

#pragma once

#include<pthread.h>
#include "util.h"

typedef struct {
    integer db_size;
    char *db_naive_AS;
    char *db_naive_AS_shandow;
} db_naive_infomation;

int db_naive_init(void *db_naive_info, size_t db_size);

void *naive_read(size_t page_index);

int naive_write(size_t page_index, void *value);

void ckp_naive(int ckp_id, void *db_naive_info);

void db_naive_destroy(void *db_naive_info);

