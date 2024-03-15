CC=clang
CFLAGS=$(shell pkgconf --cflags x11 alsa) -Wall -pedantic -Wextra
LDFLAGS=$(shell pkgconf --libs x11 alsa)

TARGET=kasy

all: $(TARGET)

$(TARGET): %: %.c
	mkdir build
	$(CC) $(CFLAGS) $(LDFLAGS) -o build/$@ $<

clean:
	rm -rf build

.PHONY: all clean
