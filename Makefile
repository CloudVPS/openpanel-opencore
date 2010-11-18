# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

include makeinclude

OBJ =	alerts.o api.o dbmanager.o main.o module.o moduledb.o \
		internalclass.o session.o debug.o opencorerpc.o \
		rpcrequesthandler.o rpc.o version.o

TSOBJ = techsupport.o dbmanager.o debug.o

MKOBJ = mkmodulexml.o

all: opencore.exe techsupport.exe mkmodulexml api/python/package/OpenPanel/error.py 
	grace mkapp opencore
	grace mkapp techsupport

version.cpp:
	grace mkversion version.cpp

api/python/package/OpenPanel/error.py: error.h
	api/python/makeerrorpy

rsrc/resources.xml: error.h
	./makeresourcesxml

opencore.exe: $(OBJ) rsrc/resources.xml
	$(LD) $(LDFLAGS) -o opencore.exe $(OBJ) $(LIBS) -lz

techsupport.exe: $(TSOBJ) rsrc/resources.xml
	$(LD) $(LDFLAGS) -o techsupport.exe $(TSOBJ) $(LIBS)

mkmodulexml: $(MKOBJ)
	$(LD) $(LDFLAGS) -o mkmodulexml $(MKOBJ) $(LIBS)

kickstart.panel.db: sqlite/SCHEMA sqlite/DBCONTENT
	rm -f kickstart.panel.db
	sqlite3 kickstart.panel.db < sqlite/SCHEMA
	sqlite3 kickstart.panel.db < sqlite/DBCONTENT

clean:
	rm -f *.o *.exe
	rm -rf opencore.app techsupport.app
	rm -f opencore techsupport
	rm -f version.cpp version.id
	rm -f api/python/package/OpenPanel/error.py rsrc/resources.xml

doc:
	@mkdir -p doc
	@mkdir -p doc/generated
	@mkdir -p doc/generated/html
	doxygen doxygen.conf

depend:
	@makedepend -Y. -fMakefile.depend *.cpp 2>/dev/null

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<

include Makefile.depend
