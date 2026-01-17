#include "common.h"
#include "config.h"

int main(int argc, char *argv[]){

    //Controllo args
    if(argc < 2){
        fprintf(stderr, "[ADD_USERS] Errore: specificare n.utenti da aggiungere.\n");
        fprintf(stderr, "Esempio: %s 5\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int new_users = atoi(argv[1]);
    if(new_users <= 0){
        fprintf(stderr, "[ADD_USERS] Errore: n.utenti deve essere maggiore di 0.\n");
        exit(EXIT_FAILURE);
    }

    //Aggancio a risorse IPC esistenti
    int shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0666);
    int semid = semget(SEM_KEY, 2, 0666);
    int msgid = (MSG_KEY, 0666);

    if(shmid == -1 || semid == -1 || msgid == -1){
        perror("[ADD_USERS] Impossibile agganciarsi a risorse IPC. Palestra è attiva ?");
        exit(EXIT_FAILURE);
    }


    //Collegamento a mem condivisa
    StatoPalestra *palestra = (StatoPalestra *)shmat(shmid, NULL, 0);
    if(palestra == (void *)-1){
        perror("[ADD_USERS] Errore shmat");
        exit(EXIT_FAILURE);
    }

    printf("[ADD_USERS] Sto aggiungendo %d nuovi atleti alla simulazione (Giorno %d)...\n", new_users, palestra->giorno_corrente + 1);

    //Fork ed execv atleti
    for(int i = 0; i < new_users; i++){
        pid_t pid = fork();

        if(pid < 0){
            perror("[ADD_USERS] Errore fork");
            break;
        }

        if(pid == 0){
            char s_shmid[16], s_msgid[16], s_id[16];
            /*Usiamo buffer da 16 e non da 12 per evitare eventuali buffer overflow
            Ma soprattutto perchè con chiavi definite potremmo avere valori più grandi*/

            sprintf(s_shmid, "%d", shmid);
            sprintf(s_msgid, "%d", msgid);
            sprintf(s_id, "%d", 900 + i);

            //Preparo nuovo argv
            char *n_argv[] = {
                "./atleta",
                s_shmid,
                s_msgid,
                s_id,
                "conf_timeout.conf",
                NULL
            };

            //Preparo array dell'ambiente (envp)
            char *n_envp[] = { NULL };

            execve(n_argv[0], n_argv, n_envp);

            perror("[ADD_USERS] Errore execve");
            exit(EXIT_FAILURE);
        }
    }
    shmdt(palestra);
    printf("%d atleti aggiunti con successo.\n", new_users);
    return 0;

}