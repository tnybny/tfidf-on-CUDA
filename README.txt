The program is fully parallelized using CUDA.
Cleaning the directory
$make clean
Compiling the files
$make
To run:
./Main `pwd`/ g b
where g and b are the number of blocks per grid and number of threads per block respectively.
