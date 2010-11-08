// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/


#include "debug.h"
#include "opencore.h"
#include "paths.h"

bool DISABLE_DEBUGGING = false;

// ==========================================================================
// CONSTRUCTOR Debugger
// ==========================================================================
Debugger::Debugger (void)
{
	first = last = NULL;
}

// ==========================================================================
// DESTRUCTOR Debugger
// ==========================================================================
Debugger::~Debugger (void)
{
	ThreadUUID *c, *nc;
	
	c = first;
	while (c)
	{
		nc = c->next;
		delete c;
		c = nc;
	}
}

// ==========================================================================
// METHOD Debugger::newSession
// ==========================================================================
void Debugger::newSession (void)
{
	ThreadUUID *c = getNode();
	if (! c) return;
	
	c->uuid = strutil::uuid();
	c->serial = 0;
	CORE->log (log::debug, "Debugger", "Created debugging session <%s>",
			   c->uuid.str());
}

// ==========================================================================
// METHOD Debugger::uuid
// ==========================================================================
const string &Debugger::uuid (void)
{
	static string nothing;
	ThreadUUID *c = getNode();
	if (c) return c->uuid;
	return nothing;
}

// ==========================================================================
// METHOD Debugger::setFilter
// ==========================================================================
void Debugger::setFilter (const value &filterlist)
{
	filter = filterlist;
}

// ==========================================================================
// METHOD Debugger::storeFile
// ==========================================================================
void Debugger::storeFile (const string &subsystem, const string &action,
						  const value &idata, const string &function)
{
	if (DISABLE_DEBUGGING) return;
	if (filter.count() && (! filter.exists (subsystem))) return;
	
	string data;
	
	ThreadUUID *c = getNode();
	if (idata.count() || (idata.type() != t_string))
	{
		data = idata.toxml();
	}
	else
	{
		data = idata;
	}
	
	string fname;
	
	fname.printf (PATH_DEBUG "/%s", subsystem.str());
	if (! fs.exists (fname))
	{
		fs.mkdir (fname);
		fs.chgrp (fname, "opencore");
		fs.chmod (fname, 0700);
	}
	
	if (function)
	{
		fname.printf ("/%s", function.str());
		if (! fs.exists (fname))
		{
			fs.mkdir (fname);
			fs.chgrp (fname, "opencore");
			fs.chmod (fname, 0700);
		}
	}
	
	fname.printf ("/%s-%03i-%s.debug", c->uuid.str(), c->serial++,
									   action.str());
	
	fs.save (fname, data);
}

// ==========================================================================
// METHOD Debugger::getNode
// ==========================================================================
ThreadUUID *Debugger::getNode (void)
{
	pthread_t me = pthread_self();
	ThreadUUID *c = NULL;
	
	sharedsection (lck)
	{
		c = first;
		while (c)
		{
			if (c->thr == me) breaksection return c;
			c = c->next;
		}
	}
	
	c = new ThreadUUID;
	c->serial = 0;
	c->thr = me;
	c->uuid = "nosession";
	
	exclusivesection (lck)
	{
		if (last)
		{
			c->prev = last;
			c->next = NULL;
			last->next = c;
			last = c;
		}
		else
		{
			c->prev = c->next = NULL;
			first = last = c;
		}
	}
	
	return c;
}

Debugger DEBUG;
