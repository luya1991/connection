
#include "Headers.h"

int main(int argc, char** argv)
{
	Server s;
	s.WaitForConnection();

	/* 
	* Wait for signal interruption 
	* to return from the event loop 
	* and do the clean-up jobs here
	*/

	return 0;
}
