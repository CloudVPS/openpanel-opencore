// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2007 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include <grace/value.h>
#include <grace/application.h>
#include <grace/filesystem.h>
#include <grace/system.h>
#include <grace/daemon.h>

#include "dbmanager.h"

// This is ugly, but we need it or we can't link.
#define techsupportApp opencoreApp

class techsupportApp : public daemon
{
public:
                         techsupportApp (void) :
                                daemon ("com.openpanel.svc.opencore.techsupport")
                         {
                         }
                        ~techsupportApp (void)
                         {
                         }

        int              main (void);
						// gather all kinds of information, write it out to
						// files in the current working directory
		void			 gather(void);
		
		void			 log (log::priority p, const string &who,
							  const string &what);

		void			 log (log::priority p, const string &who,
							  const char *fmt, ...);
		
						// everything below this line writes to separate files
						// in the current working directory
		void			 dumppaneldb(void);
		void			 dumpmodulecache(void);
		void			 getosinfo(void);
		void			 domodules(void);
		void 			 domodule (const statstring &mname);
		void 			 docmdlist (void);
		
protected:
						// non-keyed todo-list for fetching filesystem stuff
		value			 filelist;

						// keyed todo-list for gathering command output
		value			 cmdlist;
};

APPOBJECT(techsupportApp);

techsupportApp *CORE = NULL;

void techsupportApp::log (log::priority p, const string &who, const string &what)
{
	daemon::log (p, who, what);
}

void techsupportApp::log (log::priority p, const string &who, const char *fmt, ...)
{
	string what;
	va_list ap;
	va_start (ap, fmt);
	what.printf_va (fmt, &ap);
	va_end (ap);
	
	log (p, who, what);
}

void techsupportApp::gather(void)
{
	dumppaneldb();
	dumpmodulecache();
	getosinfo();
	domodules();
	docmdlist();
}

void techsupportApp::docmdlist(void)
{
	value rets;
	
	foreach (cmd, cmdlist)
	{
		string s;
		s.printf("%s > %s 2>&1 3>/dev/null", cmd.str(), cmd.id().str());
		rets[cmd.id()] = system(s.str());
	}
	
	rets.savexml("cmdrets.xml");
}

void techsupportApp::dumppaneldb(void)
{
	DBManager db;
    if (! db.init ())
    {
		ferr.printf("panel database error\n");
		return;
    }
	
	db.enableGodMode();
	
	value objs;
	
	if (! db.listObjects(objs))
	{
		ferr.printf("failed to fetch root object list\n");
		return;
	}
	
	objs.savexml("paneldb.xml");
	
	db.deinit();
}

void techsupportApp::domodules (void)
{
	value mdir = fs.ls (PATH_MODULES);

	// Go over the directory entries
	foreach (filename, mdir)
	{
		string mname;

		// Are we looking at a .module-directory?
		mname = filename.id().sval();
		if (mname.globcmp ("*.module"))
		{
		        domodule (mname);
		}
	}
	
	
	filelist.savexml("DEBUG.filelist");
	cmdlist.savexml("DEBUG.cmdlist");
}

void techsupportApp::domodule (const statstring &mname)
{
	string path;
	string tmppath;
	value l;

	path.printf (PATH_MODULES "/%s", mname.str());
	fs.mkdir(mname.str());
	
	string s,s2;
	s.printf("%s/verify", mname.str());
	s2.printf(PATH_MODULES "/%s/verify", mname.str());
	cmdlist[s] = s2;
	
	tmppath.printf ("%s/techsupport.cmdlist", path.str());
	if (fs.exists (tmppath))
	{
		l.loadxml(tmppath);
		foreach (e, l)
		{
			string s;
			s.printf("%s/%s", mname.str(), e.id().str());
			cmdlist[s] = e;
		}
	}

	tmppath.clear();
	tmppath.printf ("%s/techsupport.filelist", path.str());
	if (fs.exists (tmppath))
	{
		l.loadxml(tmppath);
		filelist << l;
	}
}

void techsupportApp::dumpmodulecache (void)
{
	string cachepath = PATH_CACHES;
	cachepath.strcat ("/module.cache");
	value cache;
	cache.loadshox(cachepath);
	cache.savexml("module.cache.xml");
}

void techsupportApp::getosinfo (void)
{
	filelist.newval() = "/etc/debian_version";
	filelist.newval() = "/etc/redhat-release";	
	filelist.newval() = "/etc/apt";
	filelist.newval() = "/etc/yum";
	filelist.newval() = "/etc/yum.conf";
	filelist.newval() = "/etc/yum.repos.d";
	filelist.newval() = "/etc/lsb-release";

	cmdlist["rpm.list"] = "rpm -qa";
	cmdlist["dpkg.list"] = "dpkg -l";
	cmdlist["free.out"] = "free";
	cmdlist["diskspace.out"] = "df -h";
	cmdlist["uptime"] = "uptime";
	
}

#if 0
void techsupportApp::fetchfiles (const string &path)
{
	value dirlist, fileinfo;
	
	fileinfo = fs.getinfo(path);
	if (fileinfo["type"] == fsFile)
	{
		fout.printf("fs.cp: %d\n", fs.cp(path, path.mid(1)));
		return;
	}
	
	if (fileinfo["type"] != fsDirectory)
	{
		// not a directory? let's ignore it
		return;
	}
	
	// it's a directory, let's walk it
	dirlist = fs.dir(path.cval());
	foreach (direntry, dirlist)
	{
		if (direntry.exists("path"))
		{
			fetchfiles(direntry["path"]);
		}
	}
}
#endif

int techsupportApp::main (void)
{
	CORE=this;

	setforeground();
	disablepidcheck();
	daemonize();
	
	// DEBUG.newSession();

	string tmppath;
	
	tmppath.printf("opencore-techsupport-%d", kernel.time.unow());
	if (! fs.mkdir(tmppath)
	||  ! fs.chmod(tmppath, 0700)
	||  ! fs.cd(tmppath)
	||  chdir(tmppath.cval()) == -1)
	{
		fout.printf("failed to create or access tmppath %s\n", tmppath.cval());
		return 1;
	}
	
	gather();
	
	fout.printf("\ntechsupport dump done in %s/\n", tmppath.cval());
	exit(0);
}
