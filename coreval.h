#ifndef _coreval_H
#define _coreval_H 1
#include <grace/application.h>

//  -------------------------------------------------------------------------
/// Main application class.
//  -------------------------------------------------------------------------
class corevalApp : public application
{
public:
		 	 corevalApp (void) :
				application ("com.openpanel.tools.coreval")
			 {
			 }
			~corevalApp (void)
			 {
			 }

	int		 main (void);
	void		 loop (void);
};

#endif
