#include "common.h"
#include "config.h"

#define QUEUE_LIMIT 5
#define LOST_LIMIT 10

int main(int argc, char *argv[]){
    char *nome_conf = (argc > 1) ? argv[1]: "conf_timeout.conf";

    //Aggancio a memoria condivisa
    int shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0444); //solo lettura ?
    if(shmid == -1){
        perror("[MONITOR] Palestra non attiva");
        exit(EXIT_FAILURE);
    }
    StatoPalestra *p = (StatoPalestra *)shmat(shmid, NULL, SHM_RDONLY);
    printf("[MONITOR] Analisi in tempo reale avviata...\n");

    while(!p->terminato){
        int issues = 0;

        //Analisi coda di erogatore
        if(p->coda_erogatore > QUEUE_LIMIT){
            printf("\a[WARNING] Coda eccessiva alla reception: %d atleti in attesa !\n", p->coda_erogatore);
            issues++;
        }

        //Analisi servizi persi
        for(int i = 0; i < NOF_SERVICES; i++){
            if(p->stats[i].non_serviti_oggi > LOST_LIMIT){
                printf("[CRITICAL] Servizio %d sta perdendo troppi clienti (%d persi oggi)!\n", i, p->stats[i].non_serviti_oggi);
                issues++;
            }
        }


        if(issues == 0) printf("[MONITOR] Stato palestra: OK\n");

        sleep(2);
    }



    printf("[MOMNITOR] Simulazione terminata. Analisi conclusa.\n");
    shmdt(p);
    return 0;
}