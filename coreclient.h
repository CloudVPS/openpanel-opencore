#ifndef _coreclient_H
#define _coreclient_H 1
#include <grace/application.h>

//  -------------------------------------------------------------------------
/// Main application class.
//  -------------------------------------------------------------------------
class coreclientApp : public application
{
public:
		 	 coreclientApp (void) :
				application ("com.openpanel.tools.coreclient")
			 {
			 	opt = $("-c", $("long", "--command")) ->
			 		  $("-s", $("long", "--sessionid")) ->
			 		  $("-h", $("long", "--host")) ->
			 		  $("-v", $("long", "--verbose")) ->
			 		  $("--command", $("argc", 1)) ->
			 		  $("--sessionid", $("argc", 1)) ->
			 		  $("--host", $("argc", 1)) ->
			 		  $("--verbose", $("argc", 0));
			 }
			~coreclientApp (void)
			 {
			 }

	int		 main (void);
};

#endif
