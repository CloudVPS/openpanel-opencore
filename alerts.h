// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _OPENCORE_ALERTS_H
#define _OPENCORE_ALERTS_H 1

#include <grace/thread.h>
#include <grace/str.h>
#include <grace/value.h>

//  -------------------------------------------------------------------------
/// Thread for handling alert conditions in the opencore system.
/// Picks up alerts as events and sends them forward to either an SMTP
/// or a HTTP server.
//  -------------------------------------------------------------------------
class AlertHandler : public thread
{
public:
				 /// Constructor.
				 /// \param pconf The configuration tree under /alerts.
				 AlertHandler (const value &pconf) : thread ("AlertHandler")
				 {
				 	conf = pconf;
				 	spawn ();
				 }

				 /// Destructor.
				~AlertHandler (void)
				 {
				 }
	
				 /// Send an alert from another thread.
				 /// \param msg The alert message.
	void		 alert (const string &msg);
	
				 /// Run method. Dequeues any events that were
				 /// queueud earlier, then waits for events to
				 /// stuff on this same queue.
	void		 run (void);
	
				 /// Initiate a system shutdown. Any remaining
				 /// queued events are stored in a queue-file
				 /// prior to shutdown.
	void		 shutdown (void)
				 {
				 	value ev;
				 	ev["cmd"] = "die";
				 	sendevent (ev);
				 	shutdownCondition.wait ();
				 }

protected:
	conditional	 shutdownCondition; ///< Triggered when the thread exits.
	value		 conf; ///< Copy of the configuration data.
	value		 q; ///< The alert queue array.

				 /// Forard an alert to a back-end system. Determines
				 /// the delivery type and hands off the data to the
				 /// apropriate method.
	bool		 sendAlert (const value &alertdata);
	
				 /// Routes an alert message to an SMTP server. The
				 /// following configuration parameters are kept
				 /// into account (measured from the /alerts root):
				 /// - /smtp/server
				 /// - /smtp/subject
				 /// - /smtp/to
				 /// - /smtp/from
	bool		 sendSMTP (const value &alertdata);
	
				 /// Routes an alert message to an HTTP server. The
				 /// /http/url configuration parameter determines
				 /// the url to post to. The post will be sent with
				 /// a content-type of application/xml, with the
				 /// message inside the /message part of a gracexml
				 /// body.
	bool		 sendHTTP (const value &alertdata);
};

/// Global AlertHandler instance.
extern AlertHandler *ALERT;

#endif
