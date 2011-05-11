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

TSOBJ = techsupport.o dbmanager.o debug.o alerts.o

MKOBJ = mkmodulexml.o

all: cpp-api grace-api openpaneld.exe techsupport.exe mkmodulexml api/python/package/OpenPanel/error.py kickstart.panel.db coreval
	grace mkapp openpaneld
	grace mkapp techsupport

cpp-api:
	cd "api/c++/src" && ./configure && $(MAKE)

grace-api:
	cd "api/grace/src" && ./configure && $(MAKE)

version.cpp:
	grace mkversion version.cpp

api/python/package/OpenPanel/error.py: error.h
	api/python/makeerrorpy

rsrc/resources.xml: error.h
	./makeresourcesxml

coreval: coreval.o
	$(LD) $(LDFLAGS) -o coreval coreval.o $(LIBS) -lz

openpaneld.exe: $(OBJ) rsrc/resources.xml
	$(LD) $(LDFLAGS) -o openpaneld.exe $(OBJ) $(LIBS) -lz

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
	rm -f kickstart.panel.db
	rm -rf openpanel-core.app techsupport.app
	rm -f openpanel-core techsupport
	rm -f version.cpp
	rm -f api/python/package/OpenPanel/error.py rsrc/resources.xml
	rm -f mkmodulexml coreval
	cd "api/c++/src" && $(MAKE) clean
	cd "api/grace/src" && $(MAKE) clean

doc:
	@mkdir -p doc
	@mkdir -p doc/generated
	@mkdir -p doc/generated/html
	doxygen doxygen.conf

depend:
	@makedepend -Y. -fMakefile.depend *.cpp 2>/dev/null

install:
	mkdir -p ${DESTDIR}/etc/init.d/
	mkdir -p ${DESTDIR}/etc/openpanel/
	mkdir -p ${DESTDIR}/usr/bin/
	mkdir -p ${DESTDIR}/var/openpanel/
	mkdir -p ${DESTDIR}/var/openpanel/api
	mkdir -p ${DESTDIR}/var/openpanel/modules
	mkdir -p ${DESTDIR}/var/openpanel/bin
	mkdir -p ${DESTDIR}/var/openpanel/conf/rollback
	mkdir -p ${DESTDIR}/var/openpanel/conf/staging
	mkdir -p ${DESTDIR}/usr/include/openpanel-core/
	mkdir -p ${DESTDIR}/usr/include/grace-coreapi/
	mkdir -p ${DESTDIR}/usr/lib/openpanel-core/schemas/
	
	install -m 750 -d ${DESTDIR}/var/openpanel/log
	install -m 755 -d ${DESTDIR}/var/openpanel/sockets
	install -m 750 -d ${DESTDIR}/var/openpanel/cache
	install -m 700 -d ${DESTDIR}/var/openpanel/db
	install -m 770 -d ${DESTDIR}/var/openpanel/sockets/swupd
	install -m 770 -d ${DESTDIR}/var/openpanel/db/panel
	install -m 750 -d ${DESTDIR}/var/openpanel/debug
	install -m 755 contrib/openpanel.init ${DESTDIR}/etc/init.d/openpanel
	cp -r openpaneld.app ${DESTDIR}/var/openpanel/bin/
	cp -r techsupport.app ${DESTDIR}/var/openpanel/bin/
	install -m 600 kickstart.panel.db ${DESTDIR}/var/openpanel/db/panel/panel.db.setup
	install -m 755 openpaneld ${DESTDIR}/var/openpanel/bin/openpaneld
	install -m 755 techsupport ${DESTDIR}/var/openpanel/bin/techsupport
	
	cd "api/python/package/" && python setup.py install --root=${DESTDIR} --install-layout=deb
	
	cp -r api/sh ${DESTDIR}/var/openpanel/api/
	install -m 755 coreval ${DESTDIR}/usr/bin/coreval
	
	#
	install -m 644 api/c++/include/authdclient.h ${DESTDIR}/usr/include/openpanel-core/authdclient.h
	install -m 644 api/c++/include/moduleapp.h ${DESTDIR}/usr/include/openpanel-core/moduleapp.h
	install -m 644 api/c++/lib/libcoremodule.a ${DESTDIR}/usr/lib/openpanel-core/libcoremodule.a
	install -m 644 api/grace/include/grace-coreapi/module.h ${DESTDIR}/usr/include/grace-coreapi/module.h
	install -m 644 api/grace/lib/libgrace-coreapi.a ${DESTDIR}/usr/lib/libgrace-coreapi.a
	install -m 644 rsrc/com.openpanel.opencore.module.schema.xml ${DESTDIR}/usr/lib/openpanel-core/schemas/com.openpanel.opencore.module.schema.xml
	install -m 644 rsrc/com.openpanel.opencore.module.validator.xml ${DESTDIR}/usr/lib/openpanel-core/schemas/com.openpanel.opencore.module.validator.xml
	install -m 755 mkmodulexml ${DESTDIR}/usr/bin/mkmodulexml

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -g $<

include Makefile.depend
