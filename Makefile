CXX = gcc
FLAGS = -D_GNU_SOURCE -O3 -std=c11
DEBUG = -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -ggdb
PREFLAGS = -E

TARGET = test.bin

SRCDIR = src
BUILDDIR = .
OBJDIR = .obj

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))

all: CXXFLAGS = $(DEBUG)
all: $(TARGET)

normal: CXXFLAGS = $(FLAGS)
normal: $(TARGET)

pre: CXXFLAGS = $(PREFLAGS)
pre: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

run: all
	clear
	./$(TARGET)
