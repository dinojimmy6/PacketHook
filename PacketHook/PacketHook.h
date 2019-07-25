//#ifndef __PACKETHOOK_H__
//#define __PACKETHOOK_H__
#pragma once
#include <windows.h>
#include <detours.h>
#include <vector>
#include <list>
#include <map>
#include <queue>
#include <mutex>

#define CODE_INT 0
#define CODE_STR 1
#define CODE_BUFFER 2

using namespace std;

typedef void(__thiscall* CInPacketPtr)(void *, void *);
typedef void(__thiscall* MemoryFreePtr)(void *);
typedef char(__thiscall* Decode1Ptr)(void *);
typedef int(__thiscall* Decode2Ptr)(void *);
typedef int(__thiscall* Decode4Ptr)(void *);
typedef int64_t(__thiscall* Decode8Ptr)(void *);
typedef DWORD*(__thiscall* DecodeStrPtr)(void *, void *);
typedef void(__thiscall* DecodeBufferPtr)(void *, void *, int);

typedef void(__thiscall* COutPacketPtr)(void *, int);
typedef void(__thiscall* Encode1Ptr)(void *, char);
typedef void(__thiscall* Encode2Ptr)(void *, short);
typedef void(__thiscall* Encode4Ptr)(void *, int);
typedef void(__thiscall* Encode8Ptr)(void *, int64_t);
typedef void(__thiscall* EncodeStrPtr)(void *, void *);
typedef void(__thiscall* EncodeBufferPtr)(void *, void *, int);

extern CInPacketPtr _CInPacket;
extern MemoryFreePtr _MemoryFree;
extern Decode1Ptr _Decode1;
extern Decode2Ptr _Decode2;
extern Decode4Ptr _Decode4;
extern Decode8Ptr _Decode8;
extern DecodeStrPtr _DecodeStr;
extern DecodeBufferPtr _DecodeBuffer;

extern COutPacketPtr _COutPacket;
extern Encode1Ptr _Encode1;
extern Encode2Ptr _Encode2;
extern Encode4Ptr _Encode4;
extern Encode8Ptr _Encode8;
extern EncodeStrPtr _EncodeStr;
extern EncodeBufferPtr _EncodeBuffer;

extern vector<list<vector<char>*>*> packets;
extern map<int, BOOL> blacklist_in, blacklist_out;
extern BOOL logging;
extern mutex packets_mutex, packets_out_mutex;

BOOL DetourFunction(__in BOOL bState, __inout PVOID* ppPointer, __in PVOID pDetour);
char __fastcall Decode1_Detour(void*);
int __fastcall Decode2_Detour(void*);
int __fastcall Decode4_Detour(void*);
int64_t __fastcall Decode8_Detour(void*);
DWORD* __fastcall DecodeStr_Detour(void*, void*, void *);
void __fastcall DecodeBuffer_Detour(void*, void*, void *, int);
void __fastcall CInPacket_Detour(void*, void*, void*);
void __fastcall MemoryFree_Detour(void*);

void __fastcall Encode1_Detour(void*, void*, char);
void __fastcall Encode2_Detour(void*, void*, short);
void __fastcall Encode4_Detour(void*, void*, int);
void __fastcall Encode8_Detour(void*, void*, int64_t);
void __fastcall EncodeStr_Detour(void*, void*, void *);
void __fastcall EncodeBuffer_Detour(void*, void*, void *, int);
void __fastcall COutPacket_Detour(void*, void*, int);
void ClearPackets();
void SetWindow(HWND);
//#endif