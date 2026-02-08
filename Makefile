CC = gcc

FLAGS = -D_GNU_SOURCE -O3 -std=c11
DEBUG = -D_GNU_SOURCE -DDEBUG -std=c11 -Wall -Wextra -Wpedantic -ggdb

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
	$(MAKE) MODE=release CCFLAGS="$(FLAGS)" build-exe

debug:
	$(MAKE) MODE=debug CCFLAGS="$(DEBUG)" build-exe

rdynlib:
	$(MAKE) MODE=rdynlib CCFLAGS="$(FLAGS) -fPIC" build-lib

ddynlib:
	$(MAKE) MODE=ddynlib CCFLAGS="$(DEBUG) -fPIC" build-lib

rstatlib:
	$(MAKE) MODE=rstatlib CCFLAGS="$(FLAGS)" build-static

dstatlib:
	$(MAKE) MODE=dstatlib CCFLAGS="$(DEBUG)" build-static

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
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJDIR)/tests/%.o: $(SRCDIR)/tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) -c $< -o $@

# Cleanup
clean:
	rm -rf .obj $(BIN) $(DYNAMICLIB) $(STATICLIB)
