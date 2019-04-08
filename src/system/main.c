#include "util.h"
#include "system.h"
#include "config.h"

db_server DBServer;

int main(int argc, char *argv[]) {
    int i;
    FILE *rf;
    pthread_t *update_thread_array;
    pthread_t db_thread_id;
    pthread_barrier_t brr_exit;
    char logName[128];
    if (argc != 6) {
        perror("usage:./app [algorithm type:0-6] [page num] [page size] [update frequency (k/tick)] [random file name]");
    }
    DBServer.updateThrNum = 1;
    DBServer.algType = atoi(argv[1]);
    DBServer.dbSize = atoi(argv[2]);
    DBServer.unitSize = atoi(argv[3]);
    DBServer.updateFrequency = atoi(argv[4])*1000;
    printf("workload file from: %s\n",argv[5]);
    DBServer.ckpID = 0;
    DBServer.dbState = 0;
    DBServer.ckpMaxNum = CHECKPOINT_COUNT;
    DBServer.update_count = 0;

    DBServer.ckpOverheadLog = malloc(sizeof(long long) * DBServer.ckpMaxNum);
    DBServer.ckpPrepareLog = malloc(sizeof(long long) * DBServer.ckpMaxNum);
    DBServer.ckpTotalOverheadLog = malloc(sizeof(long long) * DBServer.ckpMaxNum);

    DBServer.globaltick = 0;
DBServer.pre_lock = UNLOCK;

    if (NULL == (rf = fopen(argv[5], "r"))) {
        perror("random file open error!\n");
        return -1;
    }
    DBServer.rfBufSize = (long long) DBServer.updateFrequency;
    DBServer.rfBuf = (long *) malloc(DBServer.rfBufSize * sizeof(long));
    // load workload data
    long long _i;
    long long dataset_len = (long long) DBServer.updateFrequency;
    for (_i = 0; _i < dataset_len; _i++) {
        fscanf(rf, "%ld\n", DBServer.rfBuf + _i);
    }
    fclose(rf);

    pthread_mutex_init(&(DBServer.accessMutex), NULL);
    pthread_mutex_init(&(DBServer.dbStateRWLock), NULL);

    pthread_barrier_init(&brr_exit, NULL, DBServer.updateThrNum + 1);

    if (0 != db_thread_start(&db_thread_id, &brr_exit, &DBServer)) {
        perror("db thread start fail!");
        exit(1);
    }
    DBServer.dbStartTime = get_mtime();
    if (0 != update_thread_start(&update_thread_array, &brr_exit,
                                 &DBServer)) {
        return -3;
    }
    //wait for quit
    pthread_join(db_thread_id, NULL);
    DBServer.dbEndTime = get_mtime();
    for (i = 0; i < DBServer.updateThrNum; i++) {
        pthread_join(update_thread_array[i], NULL);
        printf("update thread %d exit!\n", i);
    }
    free(update_thread_array);
    pthread_barrier_destroy(&brr_exit);
    sprintf(logName, "./log/%d_overhead_%dk_%ld_%d.log",
            DBServer.algType, DBServer.updateFrequency / 1000,
            DBServer.dbSize, DBServer.unitSize);
    write_overhead_log(&DBServer, logName);

    //print the database throughput
    FILE *throughputFile;
    char logPath[64];
    sprintf(logPath, "./log/tps_%d", DBServer.algType);
    throughputFile = fopen(logPath, "w+");
    fprintf(throughputFile, "TPS:%lld", DBServer.update_count / (DBServer.dbEndTime - DBServer.dbStartTime));
    fclose(throughputFile);
    exit(1);
}
