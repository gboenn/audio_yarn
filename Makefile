SRCS = main.cpp

HDRS =

OBJS = main.o

CC = g++
CFLAGS = -g -O0 -Wall --std=c++11
TARGETS = audio_yarn
PORTAUDIO = -lportaudio

.PHONY: install uninstall

all: $(TARGETS)

audio_yarn: $(OBJS) $(HDRS)
	$(CC) $(OBJS) $(PORTAUDIO) -o audio_yarn

clean:
	rm -f $(TARGETS) $(OBJS)

tidy:
	rm -f $(OBJS)

install:
	cp audio_yarn /usr/local/bin/.

uninstall:
	rm /usr/local/bin/audio_yarn

main.o: main.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $(@:.o=.cpp)

