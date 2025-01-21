# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -c

# Linker flags
LDFLAGS = -lzmq -lpthread

# Target executable
TARGET = netiomp_test  # Change this to your desired output name

# Source files
SRC = src/main.cpp src/NetIOMPReqRep.cpp src/NetIOMPDealerRouter.cpp src/NetIOMPFactory.cpp src/Party.cpp

# Object files
OBJ = src/main.o src/NetIOMPReqRep.o src/NetIOMPDealerRouter.o src/NetIOMPFactory.o src/Party.o

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJ)
	$(CXX) -std=c++17 -Wall -Wextra -O2 -o $(TARGET) $(OBJ) $(LDFLAGS)

# Compile source files to object files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

# Clean up build files
clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
