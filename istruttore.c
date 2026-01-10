#include "common.h"
#include "config.h"

int main(int argc, char *argv[]){
    Config conf = load_conf("palestra.conf");

    //Controllo args
    if(argc < 4) exit(EXIT_FAILURE);

    int shmid = atoi(argv[1]);
    int msgid = atoi(argv[2]);
    int id_istruttore = atoi(argv[3]);

    //Attach
    StatoPalestra *palestra = (StatoPalestra *)shmat(shmid, NULL, 0);
    if(palestra == (void *)-1){
        perror("Errore con attach");
        exit(EXIT_FAILURE);
    }
    srand(time(NULL) ^ (getpid() << 16));

    int mio_servizio = id_istruttore % NOF_SERVICES;
    int ultimo_giorno = -1;
    int pause_fatte = 0;

    printf("[ISTRUTTORE %d] Pronto. Mansione corrente: Servizio %d\n", id_istruttore, mio_servizio);

    while(1){
        while(palestra->giorno_corrente == ultimo_giorno) usleep(100000);

        //Inizio giornata
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
            while(palestra->min_correnti < 400) usleep(500000);
            continue;
        }

        printf("[ISTRUTTORE %d] Al lavoro (Postazione %d, Servizio %d)\n", id_istruttore, mia_postazione, mio_servizio);

        while(palestra->min_correnti < 400){
            struct msg_pacco pacco;

            if(msgrcv(msgid, &pacco, sizeof(struct msg_pacco) - sizeof(long), mio_servizio + 10, IPC_NOWAIT) != -1){

                int inizio_erog = palestra->min_correnti;
                int durata = 5 + (rand() % 11);
                
                printf("[ISTRUTTORE %d] Inizio servizio per Atleta %d (min %d)\n", id_istruttore, pacco.sender_id, inizio_erog);

                //Simulo tempo di lavoro
                sleep_min(durata, conf.n_nano_secs);

                //Aggiornamento Stats
                sem_wait(&(palestra->mux_stats));

                palestra->stats[mio_servizio].serviti_oggi++;
                palestra->stats[mio_servizio].serviti_tot++;

                long attesa = inizio_erog - pacco.min_inizio_attesa;
                if(attesa < 0) attesa = 0; //Protezione contro possibili valori negativi per attesa
                palestra->stats[mio_servizio].tempo_attesa_oggi += attesa;
                palestra->stats[mio_servizio].tempo_attesa_tot += attesa;

                palestra->stats[mio_servizio].tempo_erogazione_oggi += durata;
                palestra->stats[mio_servizio].tempo_erogazione_tot += durata;

                sem_post(&(palestra->mux_stats));
            }else{
                //Se nessuno è in coda valuto se andare in pausa o meno
                if((pause_fatte < conf.nof_pause) && ((rand() % 100) < 3)){
                    int durata_pausa = 15 + (rand() % 16);
                    printf("[ISTRUTTORE %d] Pausa di %d min (%d/%d eseguite)\n", id_istruttore, 
                            durata_pausa, pause_fatte + 1, conf.nof_pause);

                    sleep_min(durata_pausa, conf.n_nano_secs);

                    sem_wait(&(palestra->mux_stats));
                    palestra->pause_tot++;
                    sem_post(&(palestra->mux_stats));

                    pause_fatte++;
                }
                usleep(50000);
            }

        }
    }

    shmdt(palestra);
    return 0;
}