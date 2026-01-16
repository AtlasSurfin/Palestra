#ifndef CONFIG_H
#define CONFIG_H

typedef struct{
    long sim_duration;
    long n_nano_secs;
    int nof_workers;
    int nof_worker_seats;
    int nof_users;
    int nof_pause;
    int explode_threshold;
    double p_serv_min;
    double p_serv_max;
}Config;

Config load_conf(const char *filename);

#endif