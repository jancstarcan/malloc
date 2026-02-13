CC = gcc

FLAGS = -D_GNU_SOURCE -O3 -std=c11
DEBUG = -D_GNU_SOURCE -DMM_DEBUG -std=c11 -Wall -Wextra -Wpedantic -ggdb

SRCDIR = src
BUILDDIR = .

BIN = $(BUILDDIR)/test.bin
DYNAMICLIB = $(BUILDDIR)/malloc.so
STATICLIB = $(BUILDDIR)/malloc.a

LIB_SRC = $(wildcard $(SRCDIR)/*.c)
EXE_SRC = $(LIB_SRC) $(wildcard $(SRCDIR)/tests/*.c)

.PHONY: release debug rlib dlib rstatic dstatic clean

# Entry points
release:
	$(MAKE) MODE=release CFLAGS="$(FLAGS)" build-exe

debug:
	$(MAKE) MODE=debug CFLAGS="$(DEBUG)" build-exe

rdynlib:
	$(MAKE) MODE=rdynlib CFLAGS="$(FLAGS) -fPIC" build-lib

ddynlib:
	$(MAKE) MODE=ddynlib CFLAGS="$(DEBUG) -fPIC" build-lib

rstatlib:
	$(MAKE) MODE=rstatlib CFLAGS="$(FLAGS)" build-static

dstatlib:
	$(MAKE) MODE=dstatlib CFLAGS="$(DEBUG)" build-static

bear: clean
	bear -- make debug

# Build logic
MODE ?= release
OBJDIR := .obj/$(MODE)

EXE_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(EXE_SRC))
LIB_OBJ := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SRC))

# Targets
build-exe: $(BIN)
build-lib: $(DYNAMICLIB)
build-static: $(STATICLIB)

$(BIN): $(EXE_OBJ)
	$(CC) $^ -o $@

$(DYNAMICLIB): $(LIB_OBJ)
	$(CC) $^ -shared -o $@

$(STATICLIB): $(LIB_OBJ)
	ar rcs $@ $^

# Rules
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/tests/%.o: $(SRCDIR)/tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	rm -rf .obj $(BIN) $(DYNAMICLIB) $(STATICLIB)
