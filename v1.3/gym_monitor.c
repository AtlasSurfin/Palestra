#include "common.h"
#include "config.h"

#define QUEUE_LIMIT 40
#define LOST_LIMIT 60

int main(int argc, char *argv[]){

    char *nome_conf = (argc > 1) ? argv[1]: "conf_timeout.conf";
    //Carico la conf per avere il num.totale di postazioni
    Config conf = load_conf(nome_conf);

    //Aggancio a memoria condivisa
    int shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0444); //solo lettura
    if(shmid == -1){
        perror("[MONITOR] Palestra non attiva");
        exit(EXIT_FAILURE);
    }

    StatoPalestra *p = (StatoPalestra *)shmat(shmid, NULL, SHM_RDONLY);
    if(p == (void *)-1){
        perror("[MONITOR] Errore in shmat");
        exit(EXIT_FAILURE);
    }

    //Ottengo l'ID della coda messaggi per il controllo "live"
    int msgid = msgget(MSG_KEY, 0666);

    printf("[MONITOR] Analisi in tempo reale avviata...\n");

    while(1){
        struct shmid_ds buf;
        if(shmctl(shmid, IPC_STAT, &buf) == -1) break;

        printf("=== ANALISI MINUTO %d (Giorno %d) ===\n", p->min_correnti, p->giorno_corrente + 1);
        
        int issues = 0;
        

        //Analisi Coda Erogatore
        if(p->coda_erogatore > QUEUE_LIMIT){
            printf("\a[WARNING] Coda eccessiva alla reception: %d atleti in attesa !\n", p->coda_erogatore);
            issues++;
        }

        //Analisi Saturazione Istruttori
        int occupati = 0;
        for(int i = 0; i < conf.nof_worker_seats; i++){
            if(p->postazioni[i].busy) occupati++;
        }

        if(occupati == conf.nof_worker_seats && p->min_correnti < 390){
            printf("[WARNING] Saturazione totale: tutti i %d istruttori sono occupati !\n", occupati);
            issues++;
        }
        
        //Analisi coda messagi
        struct msqid_ds q_stat;
        if(msgid != -1 && msgctl(msgid, IPC_STAT, &q_stat) != -1){
            if(q_stat.msg_qnum > 25){
                printf("[CRITICAL] Rischio perdita servizi: %ld richieste in attesa di istruttore ! \n", q_stat.msg_qnum);
                issues++;
            }
        }

        //Analisi servizi persi
        for(int i = 0; i < NOF_SERVICES; i++){
            if(p->stats[i].non_serviti_oggi > 0){
                printf("[DEBUG] Servizio %d: Persi oggi = %d\n", i, p->stats[i].non_serviti_oggi);
            }

            if(p->stats[i].non_serviti_oggi > LOST_LIMIT){
                printf("[CRITICAL] Servizio %d sta perdendo troppi clienti (%d persi oggi) !\n", i, p->stats[i].non_serviti_oggi);
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