#include "common.h"
#include "config.h"
#include <signal.h>

StatoPalestra *palestra = NULL;


void handle_term(int sig){
    (void)sig;
    if(palestra != (void *)-1 && palestra != NULL) shmdt(palestra);
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    //Configurazione segnali
    struct sigaction sa;
    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);


    //COntrollo args: 0 = nome, 1 = shmid, 2 = msgid, 3 = id_atleta
    if(argc < 5){
        fprintf(stderr, "[ATLETA] Errore: args insufficienti in %s\n", argv[0]);
        exit(EXIT_FAILURE);}

    //Carico la config
    char *nome_conf = argv[4];
    Config conf = load_conf(nome_conf);

    //Recupero ID passati dal manager come stringhe
    int shmid = atoi(argv[1]);
    int msgid = atoi(argv[2]);
    int id_atleta = getpid();

    //Risorse IPC
    palestra = (StatoPalestra *)shmat(shmid, NULL, 0);
    if(palestra == (void *)-1){
        perror("[ATLETA] Errore shmat");
        exit(EXIT_FAILURE);
    }

    int semid = semget(SEM_KEY, 2, 0666);
    if(semid == -1){
        perror("[ATLETA] Errore semget");
        exit(EXIT_FAILURE);
    }

    barrier_signal(semid);

    //Seed per rand unico per ogni processo
    srand(time(NULL) ^ (getpid() << 16));

    
    //Probabilità di andare in palestra
    //printf("[ATLETA %d] Caricato: P_MIN=%.2f, P_MAX=%.2f\n", id_atleta, conf.p_serv_min, conf.p_serv_max);//Debug

    double p_serv = conf.p_serv_min + ((double)rand() / RAND_MAX) * (conf.p_serv_max - conf.p_serv_min);
    int ultimo_giorno_gestito = -1;

    while(1){
        //Controllo terminazione
        if(palestra->terminato){
            printf("[ATLETA %d] Simulazione conclusa. Esco.\n", id_atleta);
            break;
        }

        //Aspetto inizio nuovo giorno
        while(palestra->giorno_corrente <= ultimo_giorno_gestito){
            if(palestra->terminato){
                shmdt(palestra);
                exit(EXIT_SUCCESS);
            }
             usleep(50000);//100ms di attesa
        }

            ultimo_giorno_gestito = palestra->giorno_corrente;
            double daily_thres = (double)rand() / RAND_MAX;

            //Decido se venire in palestra oggi
            if(daily_thres < p_serv){
                int n_richieste = (rand() % conf.n_requests) + 1;
                printf("[ATLETA %d] Giorno %d: Entro con una lista di %d servizi.\n", id_atleta, ultimo_giorno_gestito + 1, n_richieste);
                for(int r = 0; r < n_richieste; r++){
                    
                    //Controllo se la giornata è finita mentre ero occupato
                    if(palestra->min_correnti >= 390){
                        printf("[ATLETA %d] Troppo tardi per il servizio %d/%d, vado a casa...\n", id_atleta, r + 1, n_richieste);
                        break;
                    }

                int servizio = rand() % NOF_SERVICES;
                struct msg_pacco msg;

                //Richiesta ticket all'erogatore
                msg.mtype = 1; //Richieste per erogatore
                msg.sender_id = getpid();
                msg.service_type = servizio;
                msg.tkt_num = 0;
                msg.min_inizio_attesa = palestra->min_correnti;

                if(msgsnd(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), 0) == -1) break;

                //Ricezione ticket
                if(msgrcv(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), getpid(), 0) == -1){
                    if(errno == EIDRM || errno == EINVAL){
                        shmdt(palestra);
                        exit(EXIT_SUCCESS);
                    }

                    break;
                }

                printf("[ATLETA %d] Vado ! Preso il ticket %d per servizio %d (%d/%d)\n", 
                        id_atleta, msg.tkt_num, servizio, r + 1, n_richieste);

                //Entrata in coda servizio
                msg.mtype = 10 + servizio; //mtype speciale 
                msg.sender_id = getpid();
                if(msgsnd(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), 0) == -1) break;
                if(msgrcv(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), getpid(), 0) == -1){
                    if(errno == EIDRM || errno == EINVAL){
                        shmdt(palestra);
                        exit(EXIT_SUCCESS);
                    }

                    break;
                }

                printf("[ATLETA %d] Servizio %d completato. Passo al prossimo...\n", id_atleta, servizio);
            }

        }else{
                printf("[ATLETA %d] Giorno %d: Oggi resto a casa\n", id_atleta, ultimo_giorno_gestito + 1);
        }                
            
fine_giornata:
            while(palestra->giorno_corrente == ultimo_giorno_gestito){
                if(palestra->terminato) break; //Esco per andare al controllo di terminazione (main loop)
                usleep(100000);
            }
            //Fine allenamento
        }
        return 0;
}