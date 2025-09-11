# --- config ---
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -march=native -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS  ?=
LDLIBS   ?= $(FS_LIB)   # set FS_LIB=-lstdc++fs if your toolchain needs it

# --- targets & files ---
TARGET := lru-cache
SRCS   := lru-cache.cc lru-cache-main.cc
OBJS   := $(SRCS:.cc=.o)
DEPS   := $(OBJS:.o=.d)

# --- rules ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cc lru-cache.hh
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

# If you see "undefined reference to std::filesystem..." with older GCC,
# uncomment the next line or run: make FS_LIB=-lstdc++fs
# FS_LIB=-lstdc++fs
