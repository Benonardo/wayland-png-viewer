IDIR = include
SRCDIR = src

CFLAGS += -I$(IDIR) -Wall -Wextra -Werror
LDFLAGS += -lwayland-client -lpng

ifdef DEBUG
ODIR=build
CFLAGS += -O0 -DDEBUG -g
else
ODIR=build_opt
CFLAGS += -O3 -flto -march=native
LDFLAGS += -s
endif

_HEADERS = xdg-shell.h
HEADERS = $(patsubst %,$(IDIR)/%,$(_HEADERS))

_OBJ = main.o xdg-shell.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/viewer: $(OBJ) | $(ODIR)
	clang -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(ODIR):
	mkdir $(ODIR)

$(ODIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(ODIR)
	clang -c -o $@ $< $(CFLAGS)

run: $(ODIR)/viewer
	./$<

.PHONY: clean

clean:
	rm -rf build build_opt
