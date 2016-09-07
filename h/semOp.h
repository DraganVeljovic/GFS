#ifndef SEMOP_H
#define SEMOP_H

#include <windows.h>
#include <cstdlib>

#define signal(x) ReleaseSemaphore(x,1,NULL)
#define wait(x) WaitForSingleObject(x,INFINITE)

using namespace std;

#endif