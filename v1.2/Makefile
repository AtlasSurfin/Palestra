#Variabili#
CC = gcc
# -Wvla -Wextra -Werror sono richiesti per sicurezza dal progetto
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -g  #-g è usato per debugger di VS Code

LDFLAGS = -lpthread
#nome exe finale
TARGETS = manager atleta istruttore erogatore add_users
OBJS_COMMON = config.o

#Header files
HEADERS = common.h config.h

#REGOLE#

all: $(TARGETS)

#per ogni nome in TARGETS usa il proprio .o + config.o#
$(TARGETS): %: %.o $(OBJS_COMMON)
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

#In generale compila tutti i file.c in .o, ricompila se cambia un header#
%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $< -o $@


#rimuove file.o ed exe per ripartire da zero #
clean:
		rm -f *.o $(TARGETS) stats_simulazione.csv
 
#regola usata per evitare possibili conflitti con file chiamati 'clean' o 'all'#
.PHONY: all clean test_timeout test_explode
