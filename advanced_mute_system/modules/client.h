// C++
// Client Module

#include "../sdk.h"

// Typedefs

typedef void (__thiscall *PrintFn)(void *, uintptr_t, int, int);

typedef void (__thiscall *SaveStateFn)(void *, uintptr_t);

typedef void (__thiscall *SetPlayerBanFn)(void *, char *, bool);
typedef void *(__thiscall *InternalFindPlayerSquelchFn)(void *, char *);

typedef bool (__thiscall *IsPlayerBlockedFn)(void *, int);
typedef void (__thiscall *SetPlayerBlockedStateFn)(void *, int, bool);

typedef void (__thiscall *UpdateServerStateFn)(void *, bool);

typedef bool (*HACK_GetPlayerUniqueIDFn)(int, char *);

// Controls

bool InitClientModule();

void ReleaseClientModule();