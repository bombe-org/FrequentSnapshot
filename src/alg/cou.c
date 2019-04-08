#include"src/system/system.h"
#include"cou.h"

extern db_server DBServer;

void db_cou_lock(long index) {
    unsigned char expected = 0;

    while (!__atomic_compare_exchange_1(DBServer.couInfo.db_cou_access + index, &expected,
                                        1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        expected = 0;
    }
}

void db_cou_unlock(long index) {
    __atomic_store_n(DBServer.couInfo.db_cou_access + index, 0, __ATOMIC_SEQ_CST);
}

int db_cou_init(void *cou_info, size_t db_size) {
    db_cou_infomation *info;
    info = cou_info;
    info->db_size = db_size;

    if (NULL == (info->db_cou_primary = (char *) malloc(DBServer.unitSize * db_size))) {
        perror("db_cou_primary malloc error");
        return -1;
    }
    memset(info->db_cou_primary, 'S', DBServer.unitSize * db_size);

    if (NULL == (info->db_cou_shandow = (char *) malloc(DBServer.unitSize * db_size))) {
        perror("db_cou_shandow malloc error");
        return -1;
    }
    memset(info->db_cou_shandow, 'S', DBServer.unitSize * db_size);

    if (NULL == (info->db_cou_curBA = (unsigned char *) malloc(db_size))) {
        perror("db_cou_bitarray malloc error");
        return -1;
    }

    if (NULL == (info->db_cou_chgBA = (unsigned char *) malloc(db_size))) {
        perror("db_cou_bitarray malloc error");
        return -1;
    }

    if (NULL == (info->db_cou_preBA = (unsigned char *) malloc(db_size))) {
        perror("db_cou_bitarray malloc error");
        return -1;
    }
    if(NULL == (info->db_cou_access = (unsigned char*) malloc(db_size))){
        perror("db_cou_bitarray malloc error");
    }
    memset(info->db_cou_curBA, 0, db_size);
    memset(info->db_cou_preBA, 0, db_size);
    memset(info->db_cou_chgBA, 0, db_size);
    memset(info->db_cou_access, 0, db_size);
    return 0;
}

void db_cou_destroy(void *cou_info) {
    db_cou_infomation *info;
    info = cou_info;
    free(info->db_cou_chgBA);
    free(info->db_cou_curBA);
    free(info->db_cou_preBA);
    free(info->db_cou_shandow);
    free(info->db_cou_primary);
    free(info->db_cou_access);
}

void *cou_read(size_t index) {
    void *result;
    //if (index > DBServer.dbSize)
    //    index = index % DBServer.dbSize;
    result = (DBServer.couInfo).db_cou_primary + index * DBServer.unitSize;
    return result;
}

int cou_write(size_t index, void *value) {
    long index_page = index >> 12;
    if (!DBServer.couInfo.db_cou_curBA[index_page]) {
        db_cou_lock(index_page);
        if (DBServer.couInfo.db_cou_chgBA[index_page])
            memcpy(DBServer.couInfo.db_cou_shandow + index, value, ITEM_SIZE);
        DBServer.couInfo.db_cou_curBA[index_page] = 1;
        db_cou_unlock(index_page);
    }
    memcpy(DBServer.couInfo.db_cou_primary + index, value, ITEM_SIZE);
    return 0;
}




void ckp_cou(int ckp_order, void *cou_info) {
    FILE *ckp_fd;
    char ckp_name[32];
    size_t i;
    size_t db_size;
    db_cou_infomation *info;
    long long timeStart;
    long long timeEnd;
    static int times = 0;
    info = cou_info;
    sprintf(ckp_name, "./ckp_backup/dump_%d", ckp_order);
    db_size = info->db_size;

    long long time1= get_mtime();

    db_lock(&(DBServer.pre_lock));
    timeStart = get_ntime(); // MUST after lock
    for (i = 0; i < db_size; i++) {
        info->db_cou_chgBA[i] = info->db_cou_curBA[i] | info->db_cou_preBA[i];
        info->db_cou_preBA[i] = info->db_cou_curBA[i];
        info->db_cou_curBA[i] = 1;
    }
    timeEnd = get_ntime();
    db_unlock(&(DBServer.pre_lock));
    add_prepare_log(&DBServer, timeEnd - timeStart);
    timeStart = get_mtime();

    if (NULL == (ckp_fd = fopen(ckp_name, "w+b"))) {
        perror("checkpoint file open error,checkout if the ckp_backup directory "
               "is exist");
        return;
    }
    setbuf(ckp_fd, NULL);
    if (!times) {
        fwrite(info->db_cou_shandow, (size_t) DBServer.unitSize * db_size, 1, ckp_fd);
        times++;
    } else {
        for (i = 0; i < db_size; i++) {
            if (info->db_cou_chgBA[i]) {
                db_cou_lock(i);
                if (info->db_cou_curBA[i]) {
                    fwrite(info->db_cou_shandow + (size_t) i * DBServer.unitSize, (size_t) DBServer.unitSize, 1,
                           ckp_fd);
                    db_cou_unlock(i);
                } else {
                    fwrite(info->db_cou_primary + (size_t) i * DBServer.unitSize, (size_t) DBServer.unitSize, 1,
                           ckp_fd);
                    db_cou_unlock(i);
                }
            }
        }
    }
    fflush(ckp_fd);
    fclose(ckp_fd);
    timeEnd = get_mtime();
    add_overhead_log(&DBServer, timeEnd - timeStart);

    while (get_mtime() < time1 + 10000);  // 10s
    //free(buf);
}


