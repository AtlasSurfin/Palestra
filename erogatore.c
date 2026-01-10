#include "common.h"
#include "config.h"

StatoPalestra *palestra = NULL;

void handle_term(int sig){
    (void)sig;
    if(palestra != (void *)-1 && palestra != NULL) shmdt(palestra);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    signal(SIGTERM, handle_term);

    if(argc < 3){
        fprintf(stderr, "Uso: %s <shmid> <msgid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shmid = atoi(argv[1]);
    int msgid = atoi(argv[2]);

    //Collegamento mem condivisa
    palestra = (StatoPalestra *)shmat(shmid, NULL, 0);
    if(palestra == (void *)-1){
        perror("[EROGATORE] Fallimento attach");
        exit(EXIT_FAILURE);
    }

    //Contatori per ticket
    int tkt_counter[NOF_SERVICES] = {0};
    int ultimo_giorno_visto = -1;
    struct msg_pacco msg;
    printf("[EROGATORE] Receptionist automatica pronta.\n");

    while(1){
        //Controllo giornata per resettare i ticket
        if(palestra->giorno_corrente > ultimo_giorno_visto){
            ultimo_giorno_visto = palestra->giorno_corrente;
            for(int i = 0; i < NOF_SERVICES; i++){ tkt_counter[i] = 0;}
            printf("[EROGATORE] Inizio giorno %d: contatori ticket resettati.\n", ultimo_giorno_visto + 1);
        }

        //Resta in attesa di richieste da atleti (mtype = 1)
        if(msgrcv(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), 1, 0) == -1){
            if(errno == EINVAL || errno == EIDRM) break;
            perror("[EROGATORE] Fallimento ricezione messaggio.\n");
            break;
        }

        int servizio = msg.service_type;
        int utente_id = msg.sender_id;

        if(servizio >= 0 && servizio < NOF_SERVICES){
            //Aumento ticket per quel servizio
            tkt_counter[servizio]++;
            int num_assegnato = tkt_counter[servizio];

            printf("[EROGATORE] Atleta %d richiede servizio %d. Assegnato ticket: %d\n", utente_id, servizio, num_assegnato);//

            //Prepara risposta 
            msg.mtype = utente_id;
            msg.sender_id = -1; //-1 = Erogatore risponde
            msg.tkt_num = num_assegnato;
            snprintf(msg.testo, TESTO_MAX, "Ticket assegnato: %d - Servizio: %d", num_assegnato, servizio);

            //Invio risposta
            if(msgsnd(msgid, &msg, sizeof(struct msg_pacco) - sizeof(long), 0) == -1){
                if(errno == EINVAL || errno == EIDRM) break;
                perror("[EROGATORE] Errore durante invio risposta");
            }
        }
    }

    shmdt(palestra);
    return 0;
}