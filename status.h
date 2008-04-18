// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2008 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#ifndef _OPENCORE_STATUS_H
#define _OPENCORE_STATUS_H 1

/// Return status from an opencore API call.
typedef enum {
	status_ok = 0, ///< Successful transaction
	status_failed = 1, ///< Complete failure
	status_postponed = 2 ///< Incomplete, module will call back.
} corestatus_t;

#endif
