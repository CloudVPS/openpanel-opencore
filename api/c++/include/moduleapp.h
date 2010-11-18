// This file is part of the OpenPanel API
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _MODULE_APP_H
#define _MODULE_APP_H 1


#include <grace/application.h>
#include <grace/str.h>
#include "authdclient.h"

namespace moderr {
	enum err {
		ok= 				0x00, // No errors
		err_module= 		0x02, // Module-internal error
		err_noid= 			0x03, // No Id found
		err_context= 		0x04, // Context related error
		err_value= 			0x05, // Value related error
		err_notfound= 		0x06, // Something is not found
		err_command= 		0x07, // Command not supported or wrongly used
		err_writefile= 		0x11, // Error writing files
		err_rmfile= 		0x12, // Error removing files
		err_unknown= 		0x99, // Unknown problem. Poop.
		err_authdaemon= 	0x31, // Any authdaemon problem
	};
}

class moduleapp : public application
{

public:
					 moduleapp (const char *name) 
					 			: application (name),
					 			  authd (name)
					 {
					 	string 	indata, size;
					 	sentresult = false;
					 	
					 	// Read line from stdin
					 	size  = fin.gets ();
					 	int sz = size.toint();
					 	
					 	// If the size of the data > 0
					 	if (sz > 0 )
					 	{
					 		// read data from stdin with a
					 		// 5 seconds timeout
					 		while (indata.strlen() != sz)
								indata.strcat (fin.read (sz-indata.strlen()));
							
							// Read grace xml
							data.fromxml (indata);
							
							if (data.exists ("OpenCORE:Context"))
							{
								classid = data["OpenCORE:Context"];
								if (data.exists (classid))
								{
									uuid = data[classid]["uuid"];
									metaid = data[classid]["metaid"];
									owner = data[classid]("owner");
								}
							}
							if (data.exists ("OpenCORE:Command"))
							{
								command = data["OpenCORE:Command"];
							}
					 	}	 	
					 }
					~moduleapp (void)
					 {
						authd.quit();
					 }

	virtual int		 main (void)
					 {
						return 1;
					 }
					 
					 // Event handler called before the sendresult has printed 
					 // his data to stdout
	virtual void	 onsendresult (void)
					 {
					 }
					 
					 // Event handler called before the sendresult has printed
	virtual void	 onsendresult (int code)
					 {
					 }
					
protected:
	value			data;	///< The data xml received through stdin
	string			classid; ///< The class context
	string			command; ///< The opencore command
	string			uuid; ///< The uuid of the context object
	string			metaid; ///< The metaid of the context object
	string			owner; ///< The owner of the context object.
	
	authdclient		authd;
	bool			sentresult;
	
	void			sendresult (int code, const string &text, 	
								const value &data)
					{
						if (sentresult) return;
						sentresult = true;
						
						onsendresult ();
						onsendresult (code);
						
						value outdata;
						outdata.type (creator);
						outdata = data;
						
						
						// If ok...
						if (code == 1000)
						{
							outdata["OpenCORE:Result"]["error"] 	 = code;
							outdata["OpenCORE:Result"]["message"] = "OK";
						}
						else
						{
							outdata["OpenCORE:Result"]["error"]		= code;
							outdata["OpenCORE:Result"]["message"]	= text;
						}
						
						// Write xml to standard output
						string out;
						
						out = outdata.toxml();
						
						fout.printf ("%u\n", out.strlen());
						fout.puts (out);	
					}
	
					// Send result to stdout
	void			sendresult (int code, const string &text)
					{
						if (sentresult) return;
						sentresult = true;
						
						onsendresult ();
						
						value outdata;
						outdata.type (creator);
						
						// If ok...
						if (code == 0)
						{
							outdata["OpenCORE:Result"]["code"] 	 = code;
							outdata["OpenCORE:Result"]["message"] = "OK";
						}
						else
						{
							outdata["OpenCORE:Result"]["error"]		= code;
							outdata["OpenCORE:Result"]["message"]	= text;
						}
						
						// Write xml to standard output
						string out;
						
						out = outdata.toxml();
						
						fout.printf ("%u\n", out.strlen());
						
						
						fout.puts (out);						
					}

/*				
	void			addresultobject (const statstring &classname,
									 const statstring &id,
									 const value &object)
					{
						ret[classname]("type") = "class";
						ret[classname][id] = object;
						ret[classname][id]("type") = "object";
					} */
};



#endif
