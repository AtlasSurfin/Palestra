#include "common.h"
#include "config.h"
#include <errno.h>

StatoPalestra *palestra = NULL;

void handle_term(int sig){
    (void)sig;
    if(palestra != (void *)-1 && palestra != NULL) shmdt(palestra);
    exit(EXIT_SUCCESS);
}

void handle_wake(int sig){
    (void)sig;//Non fa nulla, lo usiamo solo per interrompere msgrcv
}

int main(int argc, char *argv[]){

    struct sigaction sa;
    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sa_wake;
    sa_wake.sa_handler = handle_wake;
    sigemptyset(&sa_wake.sa_mask);
    sa_wake.sa_flags = 0;
    sigaction(SIGUSR2, &sa_wake, NULL);

    //Controllo args
    if(argc < 4) exit(EXIT_FAILURE);

    Config conf = load_conf("conf_timeout.conf");

    int shmid = atoi(argv[1]);
    int msgid = atoi(argv[2]);
    int id_istruttore = atoi(argv[3]);

    //Risorse IPC
    palestra = (StatoPalestra *)shmat(shmid, NULL, 0);
    if(palestra == (void *)-1){
        perror("Errore con attach");
        exit(EXIT_FAILURE);
    }

    int semid = semget(SEM_KEY, 2, 0666);
    if(semid == -1){
        perror("[ISTRUTTORE] Errore semget");
        exit(EXIT_FAILURE);
    }
    //Calcolo servizio dell'istruttore
    srand(time(NULL) ^ (getpid() << 16));

    int mio_servizio = id_istruttore % NOF_SERVICES;
    int ultimo_giorno = -1;

    barrier_signal(semid);

    printf("[ISTRUTTORE %d] Pronto. Mansione corrente: Servizio %d\n", id_istruttore, mio_servizio);

    while(1){
        //Attesa nuova giornata
        while(palestra->giorno_corrente == ultimo_giorno) usleep(10000);
        int g = palestra->giorno_corrente;
        ultimo_giorno = g;
        int mia_postazione = -1;

        //Ricerca postazione
        for(int i = 0; i < conf.nof_worker_seats; i++ ){
            if(palestra->servizio_postazione[i] == mio_servizio){
                mia_postazione = i;
                break;
            }
        }

        if(mia_postazione == -1){
            printf("[ISTRUTTORE %d] Giorno %d: Servizio %d non richiesto. Oggi riposo.\n", id_istruttore, g + 1, mio_servizio);
            //Salto a fine giornata
            while(palestra->min_correnti < 400 && palestra->giorno_corrente == g) usleep(500000);
            continue;
        }

        //Aggiorno il totale di op attivi
        sem_p(semid, MUX_STATS);
        palestra->totale_operatori_attivi++;
        sem_v(semid, MUX_STATS);

        printf("[ISTRUTTORE %d] Al lavoro (Postazione %d, Servizio %d)\n", id_istruttore, mia_postazione, mio_servizio);

        while(palestra->min_correnti < 400 && palestra->giorno_corrente == g){
            struct msg_pacco pacco;

            if(msgrcv(msgid, &pacco, sizeof(struct msg_pacco) - sizeof(long), mio_servizio + 10, 0) == -1){
                if(errno == EINTR) continue;
                if(errno == EIDRM || errno == EINVAL) break;
                continue;
            }


            //Servizio atleta
            int inizio_erog = palestra->min_correnti;
                int durata = 5 + (rand() % 11);
                
                printf("[ISTRUTTORE %d] Inizio servizio per Atleta %d (min %d), Durata: %d\n", id_istruttore, pacco.sender_id, inizio_erog, durata);

                //Simulo tempo di lavoro
                sleep_min(durata, conf.n_nano_secs);

                //Aggiornamento Stats
                sem_p(semid, MUX_STATS);

                palestra->stats[mio_servizio].serviti_oggi++;
                palestra->stats[mio_servizio].serviti_tot++;

                long attesa = inizio_erog - pacco.min_inizio_attesa;
                if(attesa < 0) attesa = 0; //Protezione contro possibili valori negativi per attesa

                palestra->stats[mio_servizio].tempo_attesa_oggi += attesa;
                palestra->stats[mio_servizio].tempo_attesa_tot += attesa;
                palestra->stats[mio_servizio].tempo_erogazione_oggi += durata;
                palestra->stats[mio_servizio].tempo_erogazione_tot += durata;

                //Gestione pausa
                if((rand() % 100) < 15){
                    palestra->pause_tot++;
                    sem_v(semid, MUX_STATS);

                    int durata_pausa = 5 + (rand() % 6); //pausa di 5/10 min
                    printf("[ISTRUTTORE %d] Pausa caffè...\n", id_istruttore);
                    sleep_min(durata_pausa, conf.n_nano_secs);
                }else{
                    sem_v(semid, MUX_STATS);
                }

        }

        //Decremento il semaforo perchè a fine giornata l'istruttore non è più attivo
        sem_p(semid, MUX_STATS);
        palestra->totale_operatori_attivi--;
        sem_v(semid, MUX_STATS);
    }
    return 0;
}