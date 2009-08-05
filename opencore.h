// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2008 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#ifndef _opencore_H
#define _opencore_H 1
#include <grace/daemon.h>
#include <grace/configdb.h>
#include <grace/terminal.h>

#include "moduledb.h"
#include "session.h"
#include "dbmanager.h"
#include "opencorerpc.h"

//  -------------------------------------------------------------------------
/// Implementation template for application config.
//  -------------------------------------------------------------------------
typedef configdb<class opencoreApp> appconfig;

//  -------------------------------------------------------------------------
/// Main daemon class.
//  -------------------------------------------------------------------------
class opencoreApp : public daemon
{
public:
						 /// Default constructor.
		 				 opencoreApp (void);
		 				 
		 				 /// Destructor.
		 				~opencoreApp (void);
	
						 /// Main loop. Initializes all threads and waits
						 /// for exciting things to happen (or currently
						 /// it jumps into the shell).
	int					 main (void);
	
						 /// Emit dancing bears.
	void				 dancingBears (void);
	
	cli<opencoreApp>	 shell; ///< Command line shell interpreter.

						 /// Log an error, als keeps a carbon copy
						 /// available for coreclients using the
						 /// OpenCORE:ErrorLog CoreClass.
	void				 logerror (const string &who, const string &what);
	
	void				 log (log::priority prio, const string &who,
							  const string &what);
	
	void				 log (log::priority prio, const string &who,
							  const char *fmt, ...);
	
						 /// Get last logged errors.
	value				*geterrors (void);
	
						 /// Resolve a regular expression through
						 /// the regexpdb.
	string				*regexp (const string &from);
	
protected:
						 // Configuration watcher to setup the logging
						 // process
	bool				 confSystem (config::action act, keypath &path,
								     const value &nval, const value &oval);

						 // Configuration watcher to set up 
						 // communication sockets
	bool				 confRpc (config::action act, keypath &path,
								  const value &nval, const value &oval);
	
						 /// Tab-expansion source for a session-id. This
						 /// will go once we go full daemon mode. The
						 /// implementation of configd may make this day
						 /// come sooner rather than later.
	value				*srcSessionId (const value &, int);
	
						 /// Sets up the shell and runs it.
	void				 commandline (void);
	
						 ///{
						 /// CLI handler.
	int					 cmdShowSessions (const value &);
	int					 cmdShowSession (const value &);
	int					 cmdShowVersion (const value &);
	int					 cmdShowThreads (const value &);
	int					 cmdShowClasses (const value &);
						 ///}
						 
						 /// CLI handler: exit all.
	int					 cmdExit (const value &v) { return 1; }
	
						 /// Checks whether authd runs.
						 /// \return False if authd is unavailable.
	bool				 checkAuthDaemon (void);
	
	appconfig			 conf; ///< Configuration database.
public: // TODO: temporary hack for module session! FIXME!
	ModuleDB			*mdb; ///< Module manager.
	sessiondb			*sdb; ///< Session manager.
protected:
	opencorerpc			*rpc; ///< RPC manager.
	sessionexpire		*sexp; ///< Session expire thread.
	lock<value>			 errors; ///< Logged errors.
	value				 debugfilter; ///< Filter for debug logging.
	lock<value>			 regexpdb; /// < Regular expression class definitions.
};

extern opencoreApp *CORE;

#endif
