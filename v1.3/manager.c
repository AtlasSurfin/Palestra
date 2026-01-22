#include "common.h"
#include "config.h"
#include <stdio.h>
#include <signal.h>




void cleanup();
void handle_int(int sig);
void handle_tick(int sig);
void save_stats(StatoPalestra *p, StatServizio *t, Config c);
void lancia_processo(char *path, int id, int shmid, int msgid, char *conf_file);
void print_report(StatoPalestra *p, Config c);
void print_report_tot(StatoPalestra *p, int giorni_effettivi);


int min_trascorsi = 0;
pid_t *atleti_pids = NULL, *istruttori_pids = NULL, pid_cronometro = -1, pid_erogatore = -1, pid_manager;
int shmid = -1, semid = -1, msgid = -1; //spostate qui per cleanup
StatoPalestra *palestra = NULL;
Config conf;
char causa_chiusura[20] = "TIMEOUT";

//gestore segnale per SIGINT

void handle_int(int sig){
    (void)sig;
    strncpy(causa_chiusura, "INTERRUPTED", 20);
    cleanup();
}

//gestore segnale per SIGUSR1
void handle_tick(int sig){
    (void)sig;
    min_trascorsi++;
    if(palestra) palestra->min_correnti = min_trascorsi;
}

int main(int argc, char*argv[]){
    //Leggo da file di configurazione
    char *nome_conf = (argc > 1) ? argv[1] : "conf_timeout.conf";
    printf("[MANAGER] Uso file: %s\n", nome_conf); 

    conf = load_conf(nome_conf);
    pid_manager = getpid();
    int n_figli = conf.nof_workers + conf.nof_users + 1;


    //Configurazione per SIGUSR1
    struct sigaction sa;
    sa.sa_handler = handle_tick;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    //Configurazione per SIGINT
    struct sigaction sa_int;
    sa_int.sa_handler = handle_int;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);


    //Inizio blocco di auto-pulizia preventiva
    printf("[MANAGER] Inizio pulizia preventiva ...\n");
    system("pkill -9 istruttore 2>/dev/null");
    system("pkill -9 atleta 2>/dev/null");
    system("pkill -9 erogatore 2>/dev/null");
    system("pkill -9 cronometro 2>/dev/null");
    sleep(1);
    printf("[MANAGER] Pulizia completata. Nessun processo orfano rilevato.\n");

    //Creazione risorse IPC: memoria condivisa
    shmid = shmget(SHM_KEY, sizeof(StatoPalestra), IPC_CREAT | IPC_EXCL | 0666);
    if(shmid == -1 && errno == EEXIST){
        printf("[MANAGER] Rilevate IPC pendenti. Pulisco...\n");
        int old_shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0666);
        shmctl(old_shmid, IPC_RMID, NULL);

        //Riprovo la creazione
        shmid = shmget(SHM_KEY, sizeof(StatoPalestra), IPC_CREAT | 0666);
    }
    palestra = (StatoPalestra *)shmat(shmid, NULL, 0);

    //Creazione semafori
    semid  = semget(SEM_KEY, 2, IPC_CREAT | IPC_EXCL | 0666);
    if(semid == -1 && errno == EEXIST){
        printf("[MANAGER] Rilevate IPC pendenti. Pulisco...\n");
        int old_semid = semget(SEM_KEY, 2, 0666);
        semctl(old_semid, 0, IPC_RMID);

        //Riprovo la creazione
        semid = semget(SEM_KEY, 2, IPC_CREAT | 0666);
    }
    if(semid == -1){
        perror("Errore semget");
        exit(EXIT_FAILURE);
    }

    //Inizializzazione memoria e creazione coda messaggi
    memset(palestra, 0, sizeof(StatoPalestra));
    msgid = msgget(MSG_KEY, IPC_CREAT | IPC_EXCL | 0666);
    if(msgid == -1 && errno == EEXIST){
        printf("[MANAGER] Rilevate IPC pendenti. Pulisco...\n");
        int old_msgid = msgget(MSG_KEY, 0666);
        msgctl(old_msgid, IPC_RMID, NULL);

        //Riprovo la creazione
        msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    }

    palestra->terminato = 0;
    palestra->coda_erogatore = 0;
    palestra->totale_operatori_attivi = 0;


    //Inizializzazione semafori
    union semun arg;
    arg.val = 1;
    semctl(semid, MUX_STATS, SETVAL, arg); //Mutex libero

    arg.val = 0;
    semctl(semid, BARRIER_SEM, SETVAL, arg); //Barriera a 0

    //Lancio erogatore
    if((pid_erogatore = fork()) == 0) lancia_processo("./erogatore", 0, shmid, msgid, nome_conf);

    atleti_pids = malloc(sizeof(pid_t) * conf.nof_users);
    istruttori_pids = malloc(sizeof(pid_t) * conf.nof_workers);

    if(!atleti_pids || !istruttori_pids){
        perror("Errore malloc iniziale");
        exit(EXIT_FAILURE);
    }

    //Lancio Istruttori
    for(int i = 0; i < conf.nof_workers; i++){
        if((istruttori_pids[i] = fork()) == 0) lancia_processo("./istruttore", i, shmid, msgid, nome_conf);
    }

    //Aggiornamento contatore
    palestra->totale_operatori_attivi = conf.nof_workers;

    //Lancio Atleti
    for(int i = 0; i < conf.nof_users; i++){
        if((atleti_pids[i] = fork()) == 0) lancia_processo("./atleta", i, shmid, msgid, nome_conf);
    }

    //Barriera di Inizializzazione
    printf("[MANAGER] Attesa inizializzazione %d processi figli...\n", n_figli);
    struct sembuf wait_op = {BARRIER_SEM, -1, 0}; //Operazione p
    for(int i = 0; i < n_figli; i++){
        if(semop(semid, &wait_op, 1) == -1) perror("[MANAGER] Errore semop barriera\n");
    }


    printf("[MANAGER] Tutti i processi sono allineati. Inizio simulazione !\n");
    
    //Cronometro
    if((pid_cronometro = fork()) == 0){
        while(1){
            sleep_min(1, conf.n_nano_secs); //Conta il tempo simulato: 1 minuto
            kill(getppid(), SIGUSR1); //invia il segnale al MANAGER
        }
    }

    //Logica a giornate
    int g;
    extern char **environ;
    for(g = 0; g < conf.sim_duration; g++){
        palestra->giorno_corrente = g;
        min_trascorsi = 0;

        //Reset stats per giorno succ. (per monitor)

        sem_p(semid, MUX_STATS);
        for(int i = 0; i < NOF_SERVICES; i++){
            palestra->stats[i].serviti_oggi = 0;
            palestra->stats[i].non_serviti_oggi= 0;
            palestra->stats[i].tempo_attesa_oggi = 0;
            palestra->stats[i].tempo_erogazione_oggi = 0;
        }
        sem_v(semid, MUX_STATS);

        printf("--- [MANAGER] Inizio Giorno %d ---\n", g + 1);

        //Assegno servizi e alle postazioni
        for(int i = 0; i < conf.nof_worker_seats; i++){
            palestra->postazioni[i].servizio_corrente = i % NOF_SERVICES;
            palestra->postazioni[i].busy = 0;
            palestra->postazioni[i].id_atleta_serv = 0;
        }

        

        //Simuliamo una giornata di 400 minuti (= 8 ore)
        while(min_trascorsi < 400){   //usiamo il while per gestire più richieste nello stesso tick
            pause();        //Aspettiamo il tick del cronometro
            while(waitpid(-1, NULL, WNOHANG) > 0); //Controllo per aspettare eventuali processi "zombie" (versione non bloccante)
            if(palestra->terminato) break; //controllo che se la palestra è ancora attivo
            

            //Controllo se ci sono richieste da add_users
            struct msg_pacco req_ext;
            while(msgrcv(msgid, &req_ext, sizeof(struct msg_pacco) - sizeof(long), ENTRY_REQ, IPC_NOWAIT) != -1){

                struct msg_pacco ack;
                ack.mtype = req_ext.sender_id; //Rispondiamo al PID del chiamante
                //Se mancano meno di 30 min, richiesta viene negata
                if(min_trascorsi > 370){
                    ack.tkt_num = -1;
                    printf("[MANAGER] Minuto %d: Rifiutato ingresso nuovi atleti (Chiusura imminente) per PID %d.\n", min_trascorsi, req_ext.sender_id);
                }else{
                    ack.tkt_num = 1;
                    printf("[MANAGER] Minuto %d: Autorizzato ingresso richiesto da PID %d.\n", min_trascorsi, req_ext.sender_id);
                }

                //Invio risposta su coda
                while(msgsnd(msgid, &ack, sizeof(struct msg_pacco) - sizeof(long), 0) == -1){
                    if(errno == EINTR) continue;
                    perror("Errore msgsnd di ack verso add_users");
                    break;
                }
            }

            //Gestione Alert da gym_monitor (creazione dinamica)
            struct msg_pacco alert_req;
            if(msgrcv(msgid, &alert_req, sizeof(struct msg_pacco) - sizeof(long), ALERT_RESOURCES, IPC_NOWAIT) != -1){
                int pending_msgs = alert_req.service_type;

                int da_creare = (pending_msgs / 20);
                if(da_creare > 3) da_creare = 3;
                if(da_creare == 0) da_creare = 1;

                printf("[MANAGER] Alert ! %d messaggi in coda ! Creazione di %d nuovi istruttori...\n", pending_msgs, da_creare);

                for(int k = 0; k < da_creare; k++){
                    pid_t p_new = fork();

                    if(p_new < 0){
                        perror("[MANAGER] Errore fork istruttore dinamico");
                        break;
                    }

                    if(p_new == 0){
                        char s_shmid[16], s_msgid[16], s_idx[16];

                        sprintf(s_shmid, "%d", shmid);
                        sprintf(s_msgid, "%d", msgid);
                        sprintf(s_idx, "%d", conf.nof_workers + palestra->totale_operatori_attivi + 100);

                        char *args[] = {"./istruttore", s_shmid, s_msgid, s_idx, nome_conf, NULL};
                        execve(args[0], args, environ);
                        exit(EXIT_FAILURE);
                    }else if(p_new > 0){
                        palestra->totale_operatori_attivi++;
                        pid_t *temp = realloc(istruttori_pids, palestra->totale_operatori_attivi * sizeof(pid_t));

                        if(temp){
                            istruttori_pids = temp;
                            istruttori_pids[palestra->totale_operatori_attivi - 1] = p_new;
                            printf("[MANAGER] Istruttore extra %d aggiunto (Tot: %d).\n", p_new, palestra->totale_operatori_attivi);
                        }
                    }
                }
            }
        }
         

        printf("[MANAGER] Fine giornata %d. Notifico gli istruttori...\n", g + 1);

        for(int i = 0; i < palestra->totale_operatori_attivi; i++){
            if(istruttori_pids[i] > 0) kill(istruttori_pids[i], SIGUSR2);
        }

        //Pulizia coda per giorno succ. + conteggio reale
        struct msg_pacco dummy;
        int msg_residui = 0;

        sem_p(semid, MUX_STATS); //Proteggo accesso massiccio a risorse

        while(1){
            if(msgrcv(msgid, &dummy, sizeof(struct msg_pacco) - sizeof(long), 0, IPC_NOWAIT) == -1){
                if(errno == ENOMSG) break; //Coda vuota, esco dal loop
                if(errno == EINTR) continue; //Interrotto da tick, riproviamo
                break;
            }

            int s = dummy.service_type;
            if(s >= 0 && s < NOF_SERVICES){
                palestra->stats[s].non_serviti_oggi++;
                palestra->stats[s].non_serviti_tot++;
            }
            msg_residui++;
        }

        sem_v(semid, MUX_STATS); ///Rilascio il semaforo precedente

        print_report(palestra, conf);
        save_stats(palestra, NULL, conf);

        if(msg_residui > conf.explode_threshold){
            printf("[MANAGER] SOGLIA CRITICA SUPERATA (%d)! Chiusura.\n", msg_residui);
            strncpy(causa_chiusura, "EXPLODE", 20);
            break;
        }
 }

    cleanup();
    return 0;
}

void lancia_processo(char *path, int id, int shmid, int msgid, char* conf_file){
    char s_shm[12], s_msg[12], s_id[12];
    sprintf(s_shm, "%d", shmid);
    sprintf(s_msg, "%d", msgid);
    sprintf(s_id, "%d", id);

    char *args[] = {path, s_shm, s_msg, s_id, conf_file, NULL};
    safe_exec(args[0], args);
}

void cleanup(){
    if(getpid() != pid_manager) exit(EXIT_SUCCESS);
    if(palestra) palestra->terminato = 1;

    printf("[MANAGER] Chiusura simulazione (Causa: %s)...\n", causa_chiusura);

    //Invia SIGTERM ai figli diretti
    if(atleti_pids){
        for(int i = 0; i < conf.nof_users; i++){
            if(atleti_pids[i] > 0) kill(atleti_pids[i], SIGTERM);
        }
    }

    if(istruttori_pids){
        for(int j = 0; j < palestra->totale_operatori_attivi; j++){
            if(istruttori_pids[j] > 0) kill(istruttori_pids[j], SIGTERM);
        } 
    }

    if(pid_erogatore > 0) kill(pid_erogatore, SIGTERM);
    if(pid_cronometro > 0) kill(pid_cronometro, SIGTERM);

    //Ciclo di attesa non bloccante, per gestire eventuali zombie
    printf("[MANAGER] Raccolta processi figli in corso. Attendere prego...\n");
    int status;
    pid_t died_pid;
    int attempts = 0;

    while((died_pid = waitpid(-1, &status, WNOHANG)) != -1){
        if(died_pid == 0){
             usleep(1000);
             attempts++;
             if(attempts > 100) break; //Timeout di sicurezza dopo 1 secondo circa
        }else{
            //processo figlio "raccolto", resetto i tentativi
            attempts = 0;
        }
    }


    //Salvo stats e rimuovo risorse IPC
    if(palestra != NULL){
        save_stats(palestra, NULL, conf);
        print_report_tot(palestra, palestra ->giorno_corrente + 1);
    }

    //Dealloco la memoria locale
    if(atleti_pids) free(atleti_pids);
    if(istruttori_pids) free(istruttori_pids);


    if(msgid != -1) msgctl(msgid, IPC_RMID, NULL);
    if(semid != -1) semctl(semid, 0, IPC_RMID);
    if(shmid != -1){
        shmdt(palestra);
        shmctl(shmid, IPC_RMID, NULL);
    }
    

    printf("[MANAGER] Risorse pulite correttamente. A domani !\n");
    exit(EXIT_SUCCESS);


}

void save_stats(StatoPalestra *p, StatServizio *t, Config c){
    FILE *f = fopen("stats_simulazione.csv", "a");
    if(f == NULL) return;

    //Se il file è vuoto scrivo l'header
    fseek(f, 0, SEEK_END);
    if(ftell(f) == 0){
        fprintf(f, "Giorno;Atleti_Serviti;Servizi_Persi;Attesa_Media;Rapporto_Istr_Post\n");
    }

    StatServizio locale ={0};
    if(t == NULL){
        for(int i = 0; i < NOF_SERVICES; i++){
            locale.serviti_tot += p->stats[i].serviti_oggi;
            locale.non_serviti_tot += p->stats[i].non_serviti_oggi;
            locale.tempo_attesa_tot += p->stats[i].tempo_attesa_oggi;
        }
        t = &locale;
    }
    //Calcolo attesa media
    double media = (t->serviti_tot > 0) ? (double)t->tempo_attesa_tot / t->serviti_tot : 0;
    double rapporto = (double)c.nof_workers / c.nof_worker_seats;

    //Scrivo i dati
    fprintf(f, "%d;%d;%d;%.2f;%.2f\n", p->giorno_corrente + 1, t->serviti_tot, t->non_serviti_tot, media, rapporto);
    fclose(f);
    printf("[MANAGER] Statistiche salvate in stats_simulazione.csv\n");
}

void print_report(StatoPalestra *p, Config c){
    printf("\n======== REPORT GIORNATA %d ========\n", p->giorno_corrente + 1);

    int serviti_oggi = 0;

    printf("%-15s  | %-8s | %-8s | %-12s | %-12s\n", "Servizio", "Serviti", "Persi", "Attesa Med", "Erogaz Med");
    printf("------------------------------------------------------------\n");

    for(int i = 0; i < NOF_SERVICES; i++){
        serviti_oggi += p->stats[i].serviti_oggi; 
        double attesa_media = (p->stats[i].serviti_oggi > 0) ? (double)p->stats[i].tempo_attesa_oggi / p->stats[i].serviti_oggi : 0;
        double ero_med = (p->stats[i].serviti_oggi > 0) ? (double)p->stats[i].tempo_erogazione_oggi / p->stats[i].serviti_oggi : 0;

        printf("Servizio %-6d | %-8d | %-8d | %-10.2f min | %-10.2f min\n", i, p->stats[i].serviti_oggi, p->stats[i].non_serviti_oggi, attesa_media, ero_med);
    }

    printf("------------------------------------------------------\n");
    printf("Utenti serviti oggi: %d | Rapporto Istruttori/Postazioni: %.2f\n", serviti_oggi, (double)p->totale_operatori_attivi / c.nof_worker_seats);
    printf("==========================================================\n\n");

    }

void print_report_tot(StatoPalestra *p, int giorni_effettivi){
    printf("\n********** REPORT FINALE **********\n");
    int tot_serviti = 0, tot_persi = 0;
    long tot_attesa = 0;

    for(int i = 0; i < NOF_SERVICES; i++){
        tot_serviti += p->stats[i].serviti_tot;
        tot_persi += p->stats[i].non_serviti_tot;
        tot_attesa += p->stats[i].tempo_attesa_tot;
    }

    printf("Giorni simulati: %d\n", giorni_effettivi);
    printf("Totale utenti serviti: %d (Media/giorno: %.2f)\n", tot_serviti, (double)tot_serviti / giorni_effettivi);
    printf("Totale servizi persi: %d\n", tot_persi);
    printf("Tempo medio attesa globale: %.2f min\n", (tot_serviti > 0) ? (double)tot_attesa / tot_serviti : 0);
    printf("Pause totali: %d\n", p->pause_tot);
    printf("****************************************************\n");
}
