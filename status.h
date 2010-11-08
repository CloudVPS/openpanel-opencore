// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _OPENCORE_STATUS_H
#define _OPENCORE_STATUS_H 1

/// Return status from an opencore API call.
typedef enum {
	status_ok = 0, ///< Successful transaction
	status_failed = 1, ///< Complete failure
	status_postponed = 2 ///< Incomplete, module will call back.
} corestatus_t;

#endif
