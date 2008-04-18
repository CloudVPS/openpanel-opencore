include makeinclude

OBJ			= 	coremodule.o

AR = ar cr

all:	../lib/libgrace-coreapi.a

../lib/libgrace-coreapi.a: $(OBJ)
	mkdir -p ../lib
	$(AR) ../lib/libgrace-coreapi.a $(OBJ)
	$(RANLIB) ../lib/libgrace-coreapi.a

clean:
	rm -f *.o *.a
	rm -rf ../lib

.SUFFIXES: .cpp .o
.cpp.o: 
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I../include -c -g $<

