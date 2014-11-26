#ifndef MENU_HDR
#define MENU_HDR
#include "u_map.h"

enum {
    kMenuMain    = 1 << 0,
    kMenuOptions = 1 << 2,
    kMenuCredits = 1 << 3,
    kMenuConsole = 1 << 5
};

extern int gMenuState;
extern u::map<u::string, int*> gMenuReferences;

void menuUpdate();
void menuReset();

// Register variable with menu
void menuRegister(const u::string &name, int &ref);
void menuRegister(const u::string &name, bool &ref);

#endif
