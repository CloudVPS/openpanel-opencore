# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

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

