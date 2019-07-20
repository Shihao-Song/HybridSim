SOURCE  := Main.cc include/Sim/config.cc include/protobuf/cpu_trace.pb.cc include/System/mmu.cc
CC      := g++
FLAGS   := -O3 -std=c++17 -w -I include
LD	:= -lprotobuf
TARGET  := Sim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) $(SOURCE) -o $(TARGET) $(LD)

clean:
	rm $(TARGET)                                                                                   
