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

#ifndef _OPENCORE_DEBUG_H
#define _OPENCORE_DEBUG_H 1

#include <grace/str.h>
#include <grace/lock.h>
#include <pthread.h>

//  -------------------------------------------------------------------------
/// Keeper of a thread-specific debug session uuid.
//  -------------------------------------------------------------------------
class ThreadUUID
{
public:
	///{
	/// List links.
	ThreadUUID *next, *prev;
	///}
	
	/// Thread identifier.
	pthread_t thr;
	
	/// Session uuid.
	string uuid;
	
	/// Session serial (increments on each logging action).
	int serial;
};

//  -------------------------------------------------------------------------
/// Class for dispatching debugging information in a usable format. A
/// thread can distinguish a new session, then log string or value objects
/// to numbered files in /var/opencore/debug/$subsystem.
//  -------------------------------------------------------------------------
class Debugger
{
public:
					 /// Constructor. Initializes linked list.
					 Debugger (void);
					 
					 /// Destructor.
					~Debugger (void);
	
					 /// Register a new session for this thread. The
					 /// session-id is a uuid created on the fly. Keep
					 /// in mind that this has no relationship to the
					 /// id of a CoreSession, rather most debugging
					 /// sessions will represent a single round of
					 /// interaction stemming from an rpc command.
	void			 newSession (void);
	
					 /// Get the debug session uuid for this thread.
	const string	&uuid (void);
	
					 /// Store a data object. Either a string value that
					 /// will get saved literally, or a rich node that
					 /// will get converted to XML.
					 /// \param subsystem The subsystem name.
					 /// \param action The action involved.
					 /// \param data The data object.
					 /// \param function Optional function name.
	void			 storeFile (const string &subsystem, const string &action,
								const value &data, const string &f = "");
					
					 /// Set up a subsystem filter.
	void			 setFilter (const value &filterlist);
									
protected:

					 /// Get node for the current thread.
	ThreadUUID		*getNode (void);
	
					 ///{
					 /// Linked list links.
	ThreadUUID		*first, *last;
					 ///}
					 
	lock<int>		 lck; ///< Lock on linked list.
	value			 filter; /// < Filter subsystem list
};

extern Debugger DEBUG;

#endif
