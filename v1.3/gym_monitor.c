#include "common.h"
#include "config.h"

#define QUEUE_LIMIT 20
#define LOST_LIMIT 30

int main(int argc, char *argv[]){

    char *nome_conf = (argc > 1) ? argv[1]: "conf_timeout.conf";

    //Aggancio a memoria condivisa
    int shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0444); //solo lettura ?
    if(shmid == -1){
        perror("[MONITOR] Palestra non attiva");
        exit(EXIT_FAILURE);
    }

    StatoPalestra *p = (StatoPalestra *)shmat(shmid, NULL, SHM_RDONLY);
    if(p == (void *)-1){
        perror("[MONITOR] Errore in shmat");
        exit(EXIT_FAILURE);
    }

    printf("[MONITOR] Analisi in tempo reale avviata...\n");

    while(1){
        struct shmid_ds buf;
        if(shmctl(shmid, IPC_STAT, &buf) == -1) break;

        printf("=== ANALISI MINUTO %d (Giorno %d) ===\n", p->min_correnti, p->giorno_corrente + 1);
        
        int issues = 0;
        //Analisi servizi persi
        for(int i = 0; i < NOF_SERVICES; i++){
            printf("[DEBUG] Servizio %d: Persi oggi = %d\n", i, p->stats[i].non_serviti_oggi);
            if(p->stats[i].non_serviti_oggi > LOST_LIMIT){
                printf("[CRITICAL] Servizio %d sta perdendo troppi clienti (%d persi oggi)!\n", i, p->stats[i].non_serviti_oggi);
                issues++;
            }
        }

        if(issues == 0) printf("[MONITOR] Stato palestra: OK\n");
        fflush(stdout);
        
        if(p->terminato){
            printf("[MONITOR] Flag terminato rilevato. Chiusura...\n");
            break;
        }

        sleep(2);
        printf("---------------------------------------------------\n");
    }


    printf("[MONITOR] Simulazione terminata. Analisi conclusa.\n");
    shmdt(p);
    return 0;
}