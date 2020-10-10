/**
 *  blescan.cpp
 *
 *    2020/09/12
 */

#include <iostream>

#include "blescan.h"
#include "central.h"


using namespace std;


int main(int argc, char** argv)
{
	Central central;

	try {
		central.openDevice();
		central.startHciScan();

	} catch (TTBleException& exp) {
        cout << "Error occurred in main(): " << exp.what() << endl;
		return 1;
    }

	return 0;
}
