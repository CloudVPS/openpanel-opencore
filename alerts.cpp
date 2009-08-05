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

#include "alerts.h"
#include "paths.h"
#include "opencore.h"

#include <grace/smtp.h>
#include <grace/http.h>

// ==========================================================================
// METHOD AlertHandler::alert
// ==========================================================================
void AlertHandler::alert (const string &alertstr)
{
	value ev;
	ev["cmd"] = "alert";
	ev["data"] = alertstr;
	sendevent (ev);
}

// ==========================================================================
// METHOD AlertHandler::run
// ==========================================================================
void AlertHandler::run (void)
{
	q.loadshox (PATH_ALERTQ);
	CORE->log (log::info, "alerthdl", "Starting thread, %i alerts in queue"
								   						%format (q.count()));
	value ev;
	
	while (true)
	{
		while (q.count())
		{
			if (sendAlert (q[0]))
			{
				q.rmindex (0);
			}
			else
			{
				CORE->log (log::warning, "alerthdl", "Error routing an "
						   "alert, holding back 2 seconds");
				sleep (2);
				break;
			}
		}
		ev = waitevent (1000);
		if (ev)
		{
			if (ev["cmd"] == "die")
			{
				CORE->log (log::info, "alerthdl", "Shutting down thread "
						   "with %i alerts in queue" %format (q.count()));
						   
				q.saveshox (PATH_ALERTQ);
				shutdownCondition.broadcast ();
				return;
			}
			if (ev["cmd"] == "alert")
			{
				q.newval() = ev["data"];
			}
		}
	}
}

/*

	in conf
	/alert/routing = smtp/http
	/alert/smtp/alert_to
	/alert/smtp/subject
	/alert/smtp/toaddr
	/alert/http/post_url

*/

// ==========================================================================
// METHOD AlertHandler::sendAlert
// ==========================================================================
bool AlertHandler::sendAlert (const value &alertdata)
{
	caseselector (conf["routing"])
	{
		incaseof ("smtp") :
			return sendSMTP (alertdata);
			break;
		
		incaseof ("http") :
			return sendHTTP (alertdata);
			break;
			
		defaultcase :
			return true;
	}
}

// ==========================================================================
// METHOD AlertHandler::sendSMTP
// ==========================================================================
bool AlertHandler::sendSMTP (const value &alertdata)
{
	string smtphost = conf["smtp"]["server"];
	string subject = conf["smtp"]["subject"];
	string addr = conf["smtp"]["to"];
	string from = conf["smtp"]["from"];
	
	smtpsocket s;
	s.setsmtphost (smtphost);
	s.setsender (from, "OpenPanel Alert Handler");
	
	value rcpt;
	rcpt.newval() = addr;
	
	return s.sendmessage (rcpt, subject, alertdata.sval());
}

// ==========================================================================
// METHOD AlertHandler::sendHTTP
// ==========================================================================
bool AlertHandler::sendHTTP (const value &alertdata)
{
	value out;
	string outxml;
	string url;
	string res;
	
	out["message"] = alertdata;
	url = conf["http"]["url"];
	httpsocket hs;
	
	outxml = out.toxml ();
	
	res = hs.post (url, "application/xml", outxml);
	if (hs.status == 200) return true;
	return false;
}

AlertHandler *ALERT = NULL;
