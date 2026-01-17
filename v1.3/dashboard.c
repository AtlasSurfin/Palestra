#include "common.h"
#include <ncurses.h>


void view_status(StatoPalestra *p){
    clear();


    //Header con colore
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(1, 2, "--- DASHBOARD PALESTRA v1.3 ---");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(3, 2, "Giorno corrente: %d", p->giorno_corrente);
    mvprintw(4, 2, "Minuto attuale: %d", p->min_correnti);
    mvprintw(5, 2, "Stato Manager: %s", p->terminato ? "TERMINATO" : "IN CORSO");
    mvprintw(7, 2, "--- STATO POSTAZIONI ---");

    for(int i = 0; i < p->totale_operatori_attivi; i++){
        if(p->postazioni[i].busy){
            attron(COLOR_PAIR(2));
            mvprintw(8 + i, 4, "Postazione: [%d]: OCCUPATA (Atleta: %d, Servizio: %d)",
                    i, p->postazioni[i].id_atleta_serv, p->postazioni[i].servizio_corrente);
            attroff(COLOR_PAIR(2));
        }else{
            attron(COLOR_PAIR(1));
            mvprintw(8 + i, 4, "Postazione [%d]: LIBERA / PAUSA", i);
            attroff(COLOR_PAIR(1));
        }
    }

    mvprintw(9 + p->totale_operatori_attivi, 2, "Coda Erogatore: %d persone", p->coda_erogatore);
    mvprintw(11 + p->totale_operatori_attivi, 2, "Premi 'q' per uscire.");

    refresh();

}

int main(){

    //Aggancio a memoria condivisa
    int shmid = shmget(SHM_KEY, sizeof(StatoPalestra), 0666);
    if(shmid == -1){
        perror("Dashboard: Errore shmget (La palestra è attiva ?)");
        exit(EXIT_FAILURE);
    }

    StatoPalestra *palestra = (StatoPalestra *)shmat(shmid, NULL, 0);

    //Feedback immediato
    printf("Dashboard connessa alla SHM %d.\n", SHM_KEY);
    printf("Inizializzazione interfaccia...\n");
    sleep(1);

    //Inizializzazione ncurses
    initscr();
    start_color();
    noecho();
    curs_set(0);
    timeout(500); //Aggiornamento ogni 500s
    
    //Definisco i colori
    init_pair(1, COLOR_GREEN, COLOR_BLACK); //per testo libero/ok
    init_pair(2, COLOR_RED, COLOR_BLACK);   //per testo occupato
    init_pair(3, COLOR_CYAN, COLOR_BLACK);  //per header

    //Main loop
    while(1){
        view_status(palestra);

        char c = getch();
        if(c == 'q' || palestra->terminato) break;
    }

    //Chiusura
    endwin();
    shmdt(palestra);
    printf("Dashboard disconessa.\n");


    return 0;
}