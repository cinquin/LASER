######################################################################
## This is the make file to compile LASER and TRACE programs
## Assuming the LAPACK and BLAS libraries were installed in /usr/lib64
## Assuming the GSL and Armadillo libraries were installed in ~/usr/lib
## Statically link to libgsl.a, libgslcblas.a, liblapack.a, libblas.a
##
## Type "make laser" to compile LASER
## Type "make trace" to compile TRACE
## Type "make" to compile both LASER and TRACE
#######################################################################

all: laser trace

LOCAL = /usr/local/
## Change $(LOCAL) if GSL and Armadillo were installed in a different directory

G++FLAG = -O3 -fopenmp
CXX = g++6

laser: laser.o
	$(CXX) $(G++FLAG) -static -o laser laser.o \
	/usr/local/lib/libgsl.a \
	/usr/local/lib/libgslcblas.a \
	/usr/local/lib/liblapack.a \
	$(LOCAL)/lib/libopenblas.a \
	-lgfortran -lpthread -lstdc++ -std=c++11 -lquadmath

laser.o: laser.cpp
	$(CXX) $(G++FLAG) -c laser.cpp -I /usr/include -I $(LOCAL)/include
	

trace: trace.o
	$(CXX) $(G++FLAG) -static -o trace trace.o \
	/usr/local/lib/libgsl.a \
	/usr/local/lib/libgslcblas.a \
	/usr/local/lib/liblapack.a \
	$(LOCAL)/lib/libopenblas.a \
	-lgfortran -lpthread -lstdc++ -std=c++11 -lquadmath

trace.o: trace.cpp
	$(CXX) $(G++FLAG) -c trace.cpp -I /usr/include -I $(LOCAL)/include


clean:
	rm *.o 

