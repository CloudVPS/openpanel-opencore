#include "coreclient.h"
#include <grace/http.h>
#include <grace/value.h>
#include <grace/sslsocket.h>

APPOBJECT(coreclientApp);

//  =========================================================================
/// Main method.
//  =========================================================================
int coreclientApp::main (void)
{
	string url = "https://%s:4089/json" %format (argv["--host"]);
	
	value vreq;
	value vres;
	vreq["header"] = $("command", argv["--command"]) ->
					 $("session_id", argv["--sessionid"]);
					 
	vreq["header"]["command"] = argv["--command"];
	for (int i=0; i<argv["*"].count(); i+=2)
	{
		string name = argv["*"][i];
		string value = argv["*"][i+1];
		
		if (name.strchr ('.') >= 0)
		{
			string name1 = name.cutat ('.');
			vreq["body"][name1][name] = value;
		}
		else
		{
			vreq["body"][name] = value;
		}
	}
	
	string req;
	string res;
	
	if (argv.exists ("--verbose"))
	{
		fout.writeln (vreq.toxml());
		fout.writeln ("<-----------------------------------------------------"
					  "---------------------->");
	}
	
	req = vreq.tojson ();
	httpssocket ht;
	ht.nocertcheck ();
	ht.postheaders["X-OpenCORE-Origin"] = "debugclient";
	res = ht.post (url, "application/x-php-serialized", req);
	vres.fromjson (res);
	fout.puts (vres.toxml());
	return 0;
}
