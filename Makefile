include makeinclude

OBJ =	alerts.o api.o dbmanager.o main.o module.o moduledb.o \
		internalclass.o session.o debug.o opencorerpc.o \
		rpcrequesthandler.o rpc.o version.o

TSOBJ = techsupport.o dbmanager.o debug.o

all: opencore.exe techsupport.exe api/python/package/OpenPanel/error.py
	mkapp opencore
	mkapp techsupport

version.cpp:
	mkversion version.cpp

api/python/package/OpenPanel/error.py: error.h
	api/python/makeerrorpy

rsrc/resources.xml: error.h
	./makeresourcesxml

opencore.exe: $(OBJ) rsrc/resources.xml
	$(LD) $(LDFLAGS) -o opencore.exe $(OBJ) $(LIBS)

techsupport.exe: $(TSOBJ) rsrc/resources.xml
	$(LD) $(LDFLAGS) -o techsupport.exe $(TSOBJ) $(LIBS)

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
