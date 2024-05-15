CXX = g++-13

CXXFLAGS += -std=c++23
CXXFLAGS += -O3
CXXFLAGS += -s

CXXFLAGS += -fPIC
CXXFLAGS += -Wall
CXXFLAGS += -Wfatal-errors
CXXFLAGS += -Wextra
CXXFLAGS += -Wwrite-strings
CXXFLAGS += -Wundef
CXXFLAGS += -Wredundant-decls
CXXFLAGS += -Wdisabled-optimization
CXXFLAGS += -Wdouble-promotion
CXXFLAGS += -Wno-parentheses
CXXFLAGS += -Wpedantic
CXXFLAGS += -pedantic-errors
CXXFLAGS += -Warray-bounds
CXXFLAGS += -Wconversion
CXXFLAGS += -Wsign-conversion
CXXFLAGS += -Wstrict-overflow=2
CXXFLAGS += -Wformat=2
CXXFLAGS += -Wcast-qual
CXXFLAGS += -Wno-unused-function
CXXFLAGS += -Weffc++
CXXFLAGS += -Wuseless-cast

CXXFLAGS += -fsanitize=leak
CXXFLAGS += -fsanitize=undefined

RM = /bin/rm 

TARGET = wc

all: $(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all clean
.DELETE_ON_ERROR:

