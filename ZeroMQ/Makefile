# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Linker flags
LDFLAGS = -lzmq -lpthread -lcrypto

# Target executable
TARGET = netiomp_test  # Change this to your desired output name

# Source files
SRCS = src/main.cpp \
       src/NetIOMPReqRep.cpp \
       src/NetIOMPDealerRouter.cpp \
       src/NetIOMPFactory.cpp \
       src/Party.cpp \
       src/AdditiveSecretSharing.cpp  # <-- Added here

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files to object files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
