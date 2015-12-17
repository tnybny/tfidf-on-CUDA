EXECUTABLE      := Main

CUDA_PATH       ?= /usr/local/cuda-7.5
HOST_COMPILER ?= g++
NVCC          := $(CUDA_PATH)/bin/nvcc -ccbin $(HOST_COMPILER)

INCLUDES  += -I. -I/ncsu/gcc346/include/c++/ -I/common/inc 

all: 
	$(NVCC) -g cuda_k.cu Main.cpp -o Main $(INCLUDES)

clean:
	rm -f *.o $(EXECUTABLE)

