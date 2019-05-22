SOURCE  := Main.cpp Memory_System/pcm_sim_memory_system.cc Controller/pcm_sim_controller.cc Configs/pcm_sim_config.cc Array_Architecture/pcm_sim_array.cc Trace/pcm_sim_trace.cc

CC      := g++
FLAGS   := -O3 -std=c++11 -w
TARGET  := PCMSim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm $(TARGET)                                                                                   
