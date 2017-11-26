//
// Created by fedya on 01.11.17.
//

#ifndef SEM_SHMEM_PRINT_SEMS_H
#define SEM_SHMEM_PRINT_SEMS_H

#endif //SEM_SHMEM_PRINT_SEMS_H

int Print_Sems(int semid){
    union semun sss;
    unsigned short arr[N_SEM] = {};
    sss.array = arr;
    semctl(semid, 0, GETALL, sss);
    struct timeval tm = {};

    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    gettimeofday(&tm, NULL);
    printf ("\n%s%ld\n", asctime(timeinfo), tm.tv_usec);
    for (int i = 0; i < N_SEM; i++)
        printf("sem %-12s = %d\n", names[i], arr[i]);
    fflush(stdout);
}