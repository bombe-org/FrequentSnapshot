#include"src/system/system.h"
#include"pingpong.h"

extern db_server DBServer;

int db_pingpong_init(void *pp_info, size_t db_size) {
    db_pingpong_infomation *info;

    info = pp_info;
    info->db_size = db_size;
    if (NULL == (info->db_pp_as = malloc(DBServer.unitSize * db_size))) {
        perror("db_pp_as malloc error");
        return -1;
    }
    memset(info->db_pp_as, 'S', DBServer.unitSize * db_size);

    if (NULL == (info->db_pp_as_odd = malloc(DBServer.unitSize * db_size))) {
        perror("db_pp_as_odd malloc error");
        return -1;
    }
    memset(info->db_pp_as_odd, 'S', DBServer.unitSize * db_size);

    if (NULL == (info->db_pp_as_even = malloc(DBServer.unitSize * db_size))) {
        perror("db_pp_as_even malloc error");
        return -1;
    }
    memset(info->db_pp_as_even, 'S', DBServer.unitSize * db_size);

    if (NULL == (info->db_pp_odd_ba = (bool*)malloc(db_size))) {
        perror("db_pp_current_odd malloc error");
        return -1;
    }
    memset(info->db_pp_odd_ba, 0, db_size);

    if (NULL == (info->db_pp_even_ba = (bool*)malloc(db_size))) {
        perror("db_pp_previous_ba malloc error");
        return -1;
    }
    memset(info->db_pp_even_ba, 1, db_size);

    if (NULL == (info->db_pp_as_previous = malloc(DBServer.unitSize * db_size))) {
        perror("db_pp_as_previous malloc error");
        return -1;
    }
    memset(info->db_pp_as_even, 'S', DBServer.unitSize * db_size);

    info->current = 0;
    return 0;
}

void db_pingpong_destroy(void *pp_info) {
    db_pingpong_infomation *info;

    info = pp_info;
    free(info->db_pp_as);
    free(info->db_pp_as_even);
    free(info->db_pp_as_odd);
    free(info->db_pp_as_previous);
    free(info->db_pp_even_ba);
    free(info->db_pp_odd_ba);
}

void *pingpong_read(size_t index) {
//    if (index > (DBServer.pingpongInfo).db_size)
//        index = index % (DBServer.pingpongInfo).db_size;
    return (DBServer.pingpongInfo).db_pp_as + index * DBServer.unitSize;
}

int pingpong_write(size_t index, void *value) {
    //index = index % (DBServer.pingpongInfo).db_size;
    memcpy((DBServer.pingpongInfo).db_pp_as + index * DBServer.unitSize, value, ITEM_SIZE);
    if (0 == (DBServer.pingpongInfo).current) {
        memcpy((DBServer.pingpongInfo).db_pp_as_odd + index * DBServer.unitSize + index % DBServer.unitSize, value, ITEM_SIZE);
        (DBServer.pingpongInfo).db_pp_odd_ba[index] = 1;
    } else {
        memcpy((DBServer.pingpongInfo).db_pp_as_even + index * DBServer.unitSize + index % DBServer.unitSize, value, ITEM_SIZE);
        (DBServer.pingpongInfo).db_pp_even_ba[index] = 1;
    }
    return 0;
}

void db_pingpong_ckp(int ckp_order, void *pp_info) {
    FILE *ckp_fd;
    char ckp_name[32];
    size_t i;
    size_t db_size;
    db_pingpong_infomation *info;
    char *currentBackup;
    bool *currentBA;
    long long timeStart;
    long long timeEnd;

    info = pp_info;
    sprintf(ckp_name, "./ckp_backup/dump_%d", ckp_order);
    if (NULL == (ckp_fd = fopen(ckp_name, "w+b"))) {
        perror("checkpoint file open error,checkout if the ckp_backup directory is exist");
        return;
    }
    //char* buf = (char*)malloc(1024L*1024*1024);
    //setvbuf(ckp_fd,buf,_IOFBF,1024L*1024*1024);
    setbuf(ckp_fd,NULL);
    db_size = info->db_size;
    long long time1= get_ntime();
    //prepare for checkpoint
    pthread_spin_lock(&(DBServer.presync));
    //db_lock(&(DBServer.pre_lock));

    timeStart = get_ntime();
    info->current = !(info->current);

    if (0 == info->current) {
        currentBackup = info->db_pp_as_odd;
        currentBA = info->db_pp_odd_ba;
    } else {
        currentBackup = info->db_pp_as_even;
        currentBA = info->db_pp_even_ba;
    }
    timeEnd = get_ntime();

    pthread_spin_unlock(&(DBServer.presync));
    //db_unlock(&(DBServer.pre_lock));
    add_prepare_log(&DBServer, timeEnd - timeStart);

    timeStart = get_ntime();
    for (i = 0; i < db_size; i++) {
        if (1 == currentBA[i]) {
            //info->db_pp_as_previous[i] = info->db_pp_as_even[i];
            memcpy(info->db_pp_as_previous + (size_t) i * DBServer.unitSize,
                   currentBackup + (size_t) i * DBServer.unitSize, (size_t) DBServer.unitSize);
            memset(currentBackup + (size_t) i * DBServer.unitSize, 0, (size_t) DBServer.unitSize);
            currentBA[i] = 0;
        }
    }

    for (int i = 0; i < db_size; ++i) {
        fwrite(info->db_pp_as_previous + (size_t) i * DBServer.unitSize, (size_t)(DBServer.unitSize), 1, ckp_fd);
    }

    fflush(ckp_fd);
    fclose(ckp_fd);
    timeEnd = get_ntime();
    add_overhead_log(&DBServer, timeEnd - timeStart);
    //free(buf);

    while (get_ntime() < time1 + 10000000000);
}


