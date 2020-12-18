#ifndef _NPSHELL_H_
#define _NPSHELL_H_

#include <vector>
#include <string>
using namespace std;


struct Pipes{
   int rw[2];   // 0 = read, 1 = write
};
struct Numpipes{
    int rw[2];
    int count;
};

struct UserPipe{
    int from;
    int to;
    int rw[2];
};



int npshell();

#endif
