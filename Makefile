# Compiler flags
CC = gcc
CFLAGS = -MMD -pedantic -std=gnu11 -Wall -Wextra
RELEASE_CFLAGS = -Os -s
DEBUG_CFLAGS = -g

# Directories
SRCDIR = src
OBJDIR = obj
RELEASEDIR = release
DEBUGDIR = debug
INSTALLDIR = /usr/local/bin

# File suffixes
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
DEPS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.d, $(SOURCES))
EXE = nino

# Default target
all: release

# Release build
release: CFLAGS += $(RELEASE_CFLAGS)
release: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE)

# Debug build
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files
-include $(DEPS)

.PHONY: all clean remake format install

# Rebuild all
remake: clean all

# Clean target
clean:
	rm -f $(OBJECTS) $(DEPS) $(EXE)

# Format all files
format:
	clang-format -i $(SRCDIR)/*.h $(SRCDIR)/*.c

# Install target
install: release
	cp $(EXE) $(INSTALLDIR)
	chmod +x $(INSTALLDIR)/$(EXE)
