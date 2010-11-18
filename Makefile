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
	rm mkmodulexml

doc:
	@mkdir -p doc
	@mkdir -p doc/generated
	@mkdir -p doc/generated/html
	doxygen doxygen.conf

depend:
	@makedepend -Y. -fMakefile.depend *.cpp 2>/dev/null

install:
	mkdir -p ${DESTDIR}/etc/init.d/
	mkdir -p ${DESTDIR}/usr/bin/
	mkdir -p ${DESTDIR}/var/opencore/
	mkdir -p ${DESTDIR}/var/opencore/api
	mkdir -p ${DESTDIR}/var/opencore/bin
	mkdir -p ${DESTDIR}/var/opencore/conf/rollback
	mkdir -p ${DESTDIR}/var/opencore/conf/staging
	mkdir -p ${DESTDIR}/usr/include/opencore/
	mkdir -p ${DESTDIR}/usr/include/grace-coreapi/
	mkdir -p ${DESTDIR}/usr/lib/opencore/schemas/
	
	install -m 750 -d ${DESTDIR}/var/opencore/log
	install -m 755 -d ${DESTDIR}/var/opencore/sockets
	install -m 750 -d ${DESTDIR}/var/opencore/cache
	install -m 700 -d ${DESTDIR}/var/opencore/db
	install -m 770 -d ${DESTDIR}/var/opencore/sockets/swupd
	install -m 770 -d ${DESTDIR}/var/opencore/db/panel
	install -m 750 -d ${DESTDIR}/var/opencore/debug
	install -m 755 contrib/debian.init ${DESTDIR}/etc/init.d/openpanel-core
	install -m 755 contrib/openpanel.init ${DESTDIR}/etc/init.d/openpanel
	cp -r opencore.app ${DESTDIR}/var/opencore/bin/
	cp -r techsupport.app ${DESTDIR}/var/opencore/bin/
	install -m 600 kickstart.panel.db ${DESTDIR}/var/opencore/db/panel/panel.db.setup
	install -m 755 opencore ${DESTDIR}/var/opencore/bin/opencore 
	install -m 755 techsupport ${DESTDIR}/var/opencore/bin/techsupport
	cp -r api/python ${DESTDIR}/var/opencore/api/
	cp -r api/sh ${DESTDIR}/var/opencore/api/
	#
	install -m 644 api/c++/include/authdclient.h ${DESTDIR}/usr/include/opencore/authdclient.h
	install -m 644 api/c++/include/moduleapp.h ${DESTDIR}/usr/include/opencore/moduleapp.h
	install -m 644 api/c++/lib/libcoremodule.a ${DESTDIR}/usr/lib/opencore/libcoremodule.a
	install -m 644 api/grace/include/grace-coreapi/module.h ${DESTDIR}/usr/include/grace-coreapi/module.h
	install -m 644 api/grace/lib/libgrace-coreapi.a ${DESTDIR}/usr/lib/libgrace-coreapi.a
	install -m 644 rsrc/com.openpanel.opencore.module.schema.xml ${DESTDIR}/usr/lib/opencore/schemas/com.openpanel.opencore.module.schema.xml
	install -m 644 rsrc/com.openpanel.opencore.module.validator.xml ${DESTDIR}/usr/lib/opencore/schemas/com.openpanel.opencore.module.validator.xml
	install -m 755 mkmodulexml ${DESTDIR}/usr/bin/mkmodulexml

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<

include Makefile.depend
