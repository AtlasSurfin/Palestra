#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <time.h>


#define TESTO_MAX 64
#define NOF_SERVICES 6

//Costanti per servizi
#define SERVIZIO_PESI 0
#define SERVIZIO_CARDIO 1
#define SERVIZIO_VALUTAZIONE 2
#define SERVIZIO_CORSO 3
#define SERVIZIO_NUTRIZIONE 4
#define SERVIZIO_SHOP 5

//Strutture per Statistiche
typedef struct{
    int serviti_oggi;
    int serviti_tot;
    int non_serviti_oggi;
    int non_serviti_tot;
    long tempo_attesa_oggi;
    long tempo_attesa_tot;
    long tempo_erogazione_oggi;
    long tempo_erogazione_tot;
}StatServizio;


//Struttura in Memoria Condivisa
typedef struct{
    int min_correnti;
    int giorno_corrente;
    int pause_tot;
    int totale_operatori_attivi;
    int servizio_postazione[20]; //Max 20 postazioni/sportelli
    StatServizio stats[NOF_SERVICES];
    sem_t mux_stats; //Semaforo per aggiornare stats
}StatoPalestra;

//Struttura Messaggi System V
struct msg_pacco{
    long mtype;
    int sender_id;
    int service_type;
    int tkt_num;
    int min_inizio_attesa;
    char testo[TESTO_MAX];
};

static void sleep_min(int min, long n_nano_secs){
    struct timespec ts;
    long long tot_nsecs = (long long) min * n_nano_secs; //forzo calcolo a 64 bit, con long long potrò avere num > 2 miliardi per nsecs

    ts.tv_sec = (time_t)(tot_nsecs / 1000000000);
    ts.tv_nsec = (long)(tot_nsecs % 1000000000);

    nanosleep(&ts, NULL);
}

#endif