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

#include "debug.h"
#include "opencore.h"
#include "paths.h"

bool DISABLE_DEBUGGING = false;

Debugger::Debugger (void)
{
	first = last = NULL;
}

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

void Debugger::newSession (void)
{
	ThreadUUID *c = getNode();
	if (! c) return;
	
	c->uuid = strutil::uuid();
	c->serial = 0;
	CORE->log (log::debug, "Debugger", "Created debugging session <%s>",
			   c->uuid.str());
}

const string &Debugger::uuid (void)
{
	static string nothing;
	ThreadUUID *c = getNode();
	if (c) return c->uuid;
	return nothing;
}

void Debugger::setFilter (const value &filterlist)
{
	filter = filterlist;
}

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
