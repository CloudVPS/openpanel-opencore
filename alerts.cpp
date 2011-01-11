// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/


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
	if (fs.exists (PATH_ALERTYQ)) q.loadshox (PATH_ALERTQ);
	log::write (log::info, "alerthdl", "Starting thread, %i alerts in queue"
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
				log::write (log::warning, "alerthdl", "Error routing an "
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
				log::write (log::info, "alerthdl", "Shutting down thread "
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
