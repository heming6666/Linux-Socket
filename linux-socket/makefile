cc=gcc
prom=client server
all : $(prom)

client: cli.h client.c myMsgDeal.c myOpen.c nonFork_cli.c
	$(cc) -o client  cli.h client.c myMsgDeal.c myOpen.c nonFork_cli.c

server: server.c myMsgDeal.c myOpen.c
	$(cc) -o server  server.c myMsgDeal.c myOpen.c

.PHONY: clean
clean:
	rm -f $(prom) *pid.txt
