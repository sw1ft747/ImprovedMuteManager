// C++
// Engine Module

#include "../sdk.h"

// a

extern int g_nLastIndexedPlayer;

// Typedefs

typedef qboolean (*FnGetPlayerUniqueID)(int, char [16]);

// Controls

bool InitEngineModule();

void ReleaseEngineModule();