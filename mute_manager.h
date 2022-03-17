#ifndef MUTE_MANAGER_H
#define MUTE_MANAGER_H

#ifdef _WIN32
#pragma once
#endif

bool LoadMuteManager();
void UnloadMuteManager();

void PauseMuteManager();
void UnpauseMuteManager();

#endif // MUTE_MANAGER_H