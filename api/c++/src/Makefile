include makeinclude

OBJ			= 	authdclient.o

AR = ar cr

all:	../lib/libcoremodule.a

../lib/libcoremodule.a: $(OBJ)
	mkdir -p ../lib
	$(AR) ../lib/libcoremodule.a $(OBJ)
	$(RANLIB) ../lib/libcoremodule.a

clean:
	rm -f *.o *.a
	rm -f ../lib/authdclient.a
	rm -rf ../lib

.SUFFIXES: .cpp .o
.cpp.o: 
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<

