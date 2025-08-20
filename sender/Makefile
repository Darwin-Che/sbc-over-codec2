CFLAGS = -Wall $(shell pkg-config --cflags libpipewire-0.3)
LDFLAGS = $(shell pkg-config --libs libpipewire-0.3) -lm

OBJS = main.o pw-stream.o

all: sender

run: sender
	./sender

sender: $(OBJS)
	g++ -o $@ $^ $(LDFLAGS)

%.o: %.cc
	g++ $(CFLAGS) -c $< -o $@

clean:
	rm -f sender $(OBJS)

.PHONY: all clean run
