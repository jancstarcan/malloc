CXX = gcc
FLAGS = -D_GNU_SOURCE -O3 -std=c11
DEBUG = -D_GNU_SOURCE -DDEBUG -std=c11 -Wall -Wextra -Wpedantic -ggdb
PREFLAGS = -E

TARGET = test.bin

SRCDIR = src
BUILDDIR = .
OBJDIR := .obj

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))

release: CXXFLAGS = $(FLAGS)
release: $(TARGET)

debug: CXXFLAGS = $(DEBUG)
debug: $(TARGET)

pre: CXXFLAGS = $(PREFLAGS)
pre: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

run: all
	clear
	./$(TARGET)
