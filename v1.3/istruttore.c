#include "common.h"
#include "config.h"
#include <errno.h>

#define LOG_SRC "ISTRUTTORE"

StatoPalestra *palestra = NULL;

void handle_term(int sig){
    (void)sig;
    if(palestra != (void *)-1 && palestra != NULL) shmdt(palestra);
    _exit(EXIT_SUCCESS);
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
    if(argc < 5){
        fprintf(stderr, "[ISTRUTTORE] Errore args insufficienti in %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *nome_conf = argv[4];
    Config conf = load_conf(nome_conf);

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

    make_log("Istruttore %d Pronto. Mansione corrente: Servizio %d\n", id_istruttore, mio_servizio);

    while(1){
        //Attesa nuova giornata
        while(palestra->giorno_corrente == ultimo_giorno) usleep(10000);
        int g = palestra->giorno_corrente;
        ultimo_giorno = g;
        int mia_postazione = -1;

        //Ricerca postazione
        for(int i = 0; i < conf.nof_worker_seats; i++ ){
            if(palestra->postazioni[i].servizio_corrente == mio_servizio){
                mia_postazione = i;
                break;
            }
        }

        if(mia_postazione == -1){
            make_log("Istruttore %d: Giorno %d: Servizio %d non richiesto. Oggi riposo.\n", id_istruttore, g + 1, mio_servizio);
            //Salto a fine giornata
            while(palestra->min_correnti < 400 && palestra->giorno_corrente == g) usleep(500000);
            continue;
        }

        //Aggiorno il totale di op attivi
        sem_p(semid, MUX_STATS);
        palestra->totale_operatori_attivi++;
        sem_v(semid, MUX_STATS);

        make_log("Istruttore %d: Al lavoro ! (Postazione %d, Servizio %d)\n", id_istruttore, mia_postazione, mio_servizio);
        palestra->postazioni[mia_postazione].busy = 0;
        palestra->postazioni[mia_postazione].id_atleta_serv = 0;

        while(palestra->min_correnti < 400 && palestra->giorno_corrente == g){
            struct msg_pacco pacco;
            
            //Aspetto atleta
            if(msgrcv(msgid, &pacco, sizeof(struct msg_pacco) - sizeof(long), mio_servizio + 10, 0) == -1){
                if(errno == EINTR) continue;
                if(errno == EIDRM || errno == EINVAL) break;
                continue;
            }


            //Servizio atleta
            int inizio_erog = palestra->min_correnti;
            int durata = 5 + (rand() % 11);
            
            //Aggiornamento Dashboard
            palestra->postazioni[mia_postazione].busy = 1;
            palestra->postazioni[mia_postazione].id_atleta_serv = pacco.sender_id;
            palestra->postazioni[mia_postazione].servizio_corrente = mio_servizio;
            palestra->postazioni[mia_postazione].tkt_corrente = pacco.tkt_num;

            make_log("Istruttore %d: Inizio servizio per Atleta %d (min %d), Durata: %d\n", id_istruttore, pacco.sender_id, inizio_erog, durata);

            //Simulo tempo di lavoro
            sleep_min(durata, conf.n_nano_secs);

            //Aggiornamento Dashboard (Fine servizio)
            palestra->postazioni[mia_postazione].busy = 0;

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
                make_log("Istruttore %d: Pausa caffè...\n", id_istruttore);
                sleep_min(durata_pausa, conf.n_nano_secs);
            }else{
                sem_v(semid, MUX_STATS);
            }

            //Notifica di fine servizio
            struct msg_pacco conferma; 
            conferma.mtype = pacco.sender_id;
            conferma.sender_id = getpid();
            conferma.tkt_num = pacco.tkt_num;

            if(msgsnd(msgid, &conferma, sizeof(struct msg_pacco) - sizeof(long), 0) == -1){
                perror("[ISTRUTTORE] Errore invio conferma fine servizio.");
            }else{
                make_log("Istruttore %d: Servizio completato per Atleta %d. Notifica inviata.\n", id_istruttore, (int)pacco.sender_id);
            }

        }

        //Decremento il semaforo perchè a fine giornata l'istruttore non è più attivo
        sem_p(semid, MUX_STATS);
        palestra->totale_operatori_attivi--;
        sem_v(semid, MUX_STATS);
    }
    return 0;
}