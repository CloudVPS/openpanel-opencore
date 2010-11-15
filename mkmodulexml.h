// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _mkmodulexml_H
#define _mkmodulexml_H 1
#include <grace/application.h>

//  -------------------------------------------------------------------------
/// Main application class.
//  -------------------------------------------------------------------------
class mkmodulexmlApp : public application
{
public:
		 	 mkmodulexmlApp (void) :
				application ("com.openpanel.tools.mkmodulexml")
			 {
			 }
			~mkmodulexmlApp (void)
			 {
			 }

	int		 main (void);
};

#endif
