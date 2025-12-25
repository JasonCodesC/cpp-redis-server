CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -march=native -pipe -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
LDFLAGS ?=

TARGET := kvserv
SRCDIR := src
OBJDIR := build

SRCS := $(shell find $(SRCDIR) -name '*.cpp')
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

.PHONY: all debug run clean format

all: $(TARGET)

debug: CXXFLAGS := $(filter-out -O3,$(CXXFLAGS))
debug: CXXFLAGS += -g -O0
debug: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJDIR) $(TARGET)

format:
	clang-format -i $(shell find $(SRCDIR) -name '*.cpp' -o -name '*.hpp')
