#ifndef BLESCAN_H
#define BLESCAN_H

/**
 *  blescan.h
 *
 *    2020/09/12
 */

#include <exception>

#include "tph.h"


/**
 * CLASS scanner_worker
 */

class scanner_worker {

public:
	scanner_worker(void) {}
	~scanner_worker() {}

	void operator()(tph_datastore& datastore, std::exception_ptr& ep);

};


// end of blescan.h

#endif
