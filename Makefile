EXECUTABLE      := Main

CUDA_PATH       ?= /usr/local/cuda-7.5
HOST_COMPILER ?= g++
NVCC          := $(CUDA_PATH)/bin/nvcc -ccbin $(HOST_COMPILER)

INCLUDES  += -I. -I/ncsu/gcc346/include/c++/ -I/ncsu/gcc346/include/c++/3.4.6/backward -I/common/inc 
LIB       := -L/ncsu/gcc346/lib

all: 
	$(NVCC) -g cuda_k.cu Main.cpp -o Main $(INCLUDES) $(LIB)

clean:
	rm -f *.o $(EXECUTABLE)

