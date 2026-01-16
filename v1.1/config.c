#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

Config load_conf(const char *filename){
    Config conf = {0}; //per sicurezza, inizializzo tutto a zero
    FILE *file = fopen(filename, "r");

    if(!file){
        perror("Errore critico: Impossibile aprire il file .conf");
        exit(EXIT_FAILURE);
    }

    char linea[256];
    while(fgets(linea, sizeof(linea), file)){
        //Salta commenti e righe vuote
        if(linea[0] == '\n' || linea[0] == '\r' || linea[0] == '#') continue;

        char chiave[64];
        double valore_dec;
        //legge tutto fino ad uguale, poi cerca il numero dopo l'uguale
        if(sscanf(linea, "%[^=]=%lf", chiave, &valore_dec) == 2){
            strtok(chiave, " \t\n\r");
            if(strcmp(chiave, "SIM_DURATION") == 0) conf.sim_duration = valore_dec;
            else if(strcmp(chiave, "N_NANO_SECS") == 0) conf.n_nano_secs = valore_dec;
            else if(strcmp(chiave, "NOF_WORKERS") == 0) conf.nof_workers = (int)valore_dec;
            else if(strcmp(chiave, "NOF_USERS") == 0) conf.nof_users = (int)valore_dec;
            else if(strcmp(chiave, "NOF_WORKER_SEATS") == 0) conf.nof_worker_seats = (int)valore_dec;
            else if(strcmp(chiave, "NOF_PAUSE") == 0) conf.nof_pause = (int)valore_dec;
            else if(strcmp(chiave, "EXPLODE_THRESHOLD") == 0) conf.explode_threshold = (int)valore_dec;
            else if(strcmp(chiave, "P_SERV_MIN") == 0) conf.p_serv_min = valore_dec;
            else if(strcmp(chiave, "P_SERV_MAX") == 0) conf.p_serv_max = valore_dec;
        }
    }

    fclose(file);
    return conf;
}