#Variabili#
CC = gcc
# -Wvla -Wextra -Werror sono richiesti per sicurezza dal progetto
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -g  #-g è usato per debugger di VS Code

LDFLAGS = -lpthread -lncurses
#nome exe finale
TARGETS = manager atleta istruttore erogatore add_users dashboard gym_monitor
OBJS_COMMON = config.o

#Header files
HEADERS = common.h config.h

#REGOLE#

all: $(TARGETS)

#Regola per l'oggetto comune (importante per modifiche a config.c)
config.o: config.c config.h
		$(CC) $(CFLAGS) -c config.c -o config.o

#In generale compila tutti i file.c in .o, ricompila se cambia un header#
%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $< -o $@
		
#per ogni nome in TARGETS usa il proprio .o + config.o#
$(TARGETS): %: %.o $(OBJS_COMMON)
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


#rimuove file.o ed exe per ripartire da zero #
clean:
		rm -f *.o $(TARGETS) stats_simulazione.csv
		@echo "Pulizia file completata."
	
clean_all:
		rm -f *.o $(TARGETS) stats_simulazione.csv palestra_sim.log
		@echo "Pulizia file totale completata."

#Funzione cleaner per le risorse IPC
wipe:
		@echo "Rimozione code, semafori e memorie condivise..."
		ipcs -q | grep $(USER) | awk '{print $$2}' | xargs -r ipcrm -q
		ipcs -s | grep $(USER) | awk '{print $$2}' | xargs -r ipcrm -s
		ipcs -m | grep $(USER) | awk '{print $$2}' | xargs -r ipcrm -m
		@echo "Risorse IPC ripulite."
 
#regola usata per evitare possibili conflitti con file chiamati 'clean' o 'all'#
.PHONY: all clean clean_all wipe time expl 

#REGOLE CUSTOM#

time: all
		./manager conf_timeout.conf

expl: all
		./manager conf_explode.conf
