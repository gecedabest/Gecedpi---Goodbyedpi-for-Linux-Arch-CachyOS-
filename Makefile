CC      = gcc
PREFIX  ?= $(CURDIR)

CFLAGS  = -std=c11 -O2 -Wall -Wextra \
          -D_GNU_SOURCE \
          -Isrc/ \
          -Isrc/utils/

LDFLAGS = -Wl,-O1,--as-needed

LIBS    = -lnetfilter_queue -lnfnetlink -lmnl -lpthread

SRCS    = src/main.c \
          src/packet.c \
          src/conntrack.c \
          src/fakepackets.c \
          src/fragment.c \
          src/nfqueue.c \
          src/autotune.c

OBJS    = $(SRCS:.c=.o)
TARGET  = gecedpi

.PHONY: all clean install uninstall debug

all: $(TARGET)

debug: CFLAGS += -DDEBUG -g -O0
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@
	@echo "Build complete: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	@echo "Installed to $(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
