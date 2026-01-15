#include "common.h"
#include "config.h"
#include <signal.h>

StatoPalestra *palestra = NULL;


void handle_term(int sig){
    (void)sig;
    if(palestra != (void *)-1 && palestra != NULL){
        shmdt(palestra);
    }
    exit(EXIT_SUCCESS);
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
        fprintf("[ATLETA] Errore: args insufficienti.\n", argv[0]);
        exit(EXIT_FAILURE);}

    //Carico la config
    char *nome_conf = argv[4];
    Config conf = load_conf(nome_conf);

    //Recupero ID passati dal manager come stringhe
    int shmid = atoi(argv[1]);
    int msgid = atoi(argv[2]);
    int id_atleta = atoi(argv[3]);

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
        //Aspetto inizio nuovo giorno
        while(palestra->giorno_corrente <= ultimo_giorno_gestito) usleep(50000);//100ms di attesa

            ultimo_giorno_gestito = palestra->giorno_corrente;
            double daily_thres = (double)rand() / RAND_MAX;

            //Decido se venire in palestra oggi
            if(daily_thres < p_serv){
                int servizio = rand() % NOF_SERVICES;
                struct msg_pacco msg;

                //Richiesta ticket all'erogatore
                msg.mtype = 1; //Richieste per erogatore
                msg.sender_id = getpid();
                msg.service_type = servizio;
                msg.min_inizio_attesa = palestra->min_correnti;

                if(msgsnd(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), IPC_NOWAIT) == -1){
                    if(errno == EAGAIN){
                        printf("[ATLETA %d] Coda erogatore piena. Torno a casa...\n", id_atleta);
                        goto fine_giornata;
                    }
                    break;
                }

                //Ricezione ticket
                if(msgrcv(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), getpid(), IPC_NOWAIT) == -1){
                    if(errno == EINVAL || errno == EIDRM) break;
                }

                printf("[ATLETA %d] Giorno %d: Vado ! Preso il ticket %d per servizio %d\n", 
                        id_atleta, ultimo_giorno_gestito + 1, msg.tkt_num, servizio);

                //Entrata in coda servizio
                msg.mtype = 10 + servizio; //mtype speciale 
                msg.sender_id = id_atleta;
                if(msgsnd(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), 0) == -1){
                    if(errno == EAGAIN){
                        printf("[ATLETA %d] Coda servizio %d piena. Ci rinuncio, torno a casa...\n", id_atleta, servizio);
                        goto fine_giornata;
                    }
                    break;
                }
            }else{
                printf("[ATLETA %d] Giorno %d: Oggi resto a casa\n", id_atleta, ultimo_giorno_gestito + 1);
            }                
            
fine_giornata:
            while(palestra->min_correnti < 400 && palestra->giorno_corrente == ultimo_giorno_gestito){
                usleep(200000);
            }
            //Fine allenamento
        }
        return 0;
}