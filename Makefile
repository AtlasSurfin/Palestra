#Variabili#
CC = gcc
# -Wvla -Wextra -Werror sono richiesti per sicurezza dal progetto
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -g  #-g è usato per debugger di VS Code

LDFLAGS = -lpthread -lrt #necessario per semafori POSIX

#nome exe finale
TARGETS = manager atleta istruttore erogatore

#Header files
HEADERS = common.h config.h

#REGOLE#

all: $(TARGETS) #questa viene eseguita scrivendo solo 'make' #

manager: manager.o config.o
		$(CC) $(CFLAGS) -o manager manager.o config.o $(LDFLAGS)
atleta: atleta.o config.o
		$(CC) $(CFLAGS) -o atleta atleta.o config.o $(LDFLAGS)

istruttore: istruttore.o config.o
		$(CC) $(CFLAGS) -o istruttore istruttore.o config.o $(LDFLAGS)

erogatore: erogatore.o
		$(CC) $(CFLAGS) -o erogatore erogatore.o $(LDFLAGS)

#regole generiche
%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $<

#rimuove file.o ed exe per ripartire da zero #
clean:
		rm -f *.o $(TARGETS)

#regola usata per evitare possibili conflitti con file chiamati 'clean' o 'all'#
.PHONY: all clean