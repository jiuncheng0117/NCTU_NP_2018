#ifndef NPSHELL_H
#define NPSHELL_H
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

int npshell();

#endif
