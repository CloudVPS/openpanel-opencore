# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

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

