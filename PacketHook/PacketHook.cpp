// PacketHook.cpp : Defines the exported functions for the DLL application.
//
#define _CRT_NON_CONFORMING_SWPRINTFS

#include "PacketHook.h"

CInPacketPtr _CInPacket = (CInPacketPtr)0x00CEF080;
MemoryFreePtr _MemoryFree = (MemoryFreePtr)0x004BDEB0;
Decode1Ptr _Decode1 = (Decode1Ptr)0x0042CC30;
Decode2Ptr _Decode2 = (Decode2Ptr)0x0042CCE0;
Decode4Ptr _Decode4 = (Decode4Ptr)0x0042CD90;
Decode8Ptr _Decode8 = (Decode8Ptr)0x0056F460;
DecodeStrPtr _DecodeStr = (DecodeStrPtr)0x004EBE00;
DecodeBufferPtr _DecodeBuffer = (DecodeBufferPtr)0x0042CE40;

COutPacketPtr _COutPacket = (COutPacketPtr)0x00CEF6E0;
Encode1Ptr _Encode1 = (Encode1Ptr)0x004C6A20;
Encode2Ptr _Encode2 = (Encode2Ptr)0x004C6A70;
Encode4Ptr _Encode4 = (Encode4Ptr)0x004BEDD0;
Encode8Ptr _Encode8 = (Encode8Ptr)0x0056FF80;
EncodeStrPtr _EncodeStr = (EncodeStrPtr)0x005713B0;
EncodeBufferPtr _EncodeBuffer = (EncodeBufferPtr)0x005B65F0;

BOOL logging = FALSE;
list<vector<char>*>* build_in;
list<vector<char>*>* build_out;
int build_out_addr;
vector<list<vector<char>*>*> packets;
map<int, BOOL> blacklist_in, blacklist_out;
HWND window;
mutex packets_mutex;

void SetWindow(HWND in) {
	window = in;
}

vector<char> * toByteArr(int64_t raw, int size) {
	vector<char> *ret = new vector<char>();
	ret->push_back(CODE_INT);
	switch (size) {
	case 1:
		ret->push_back((char)raw);
		break;
	case 2:
		ret->push_back(raw & 0xFF);
		ret->push_back((raw >> 8) & 0xFF);
		break;
	case 4:
		ret->push_back(raw & 0xFF);
		ret->push_back((raw >> 8) & 0xFF);
		ret->push_back((raw >> 16) & 0xFF);
		ret->push_back((raw >> 24) & 0xFF);
		break;
	case 8:
		ret->push_back(raw & 0xFF);
		ret->push_back((raw >> 8) & 0xFF);
		ret->push_back((raw >> 16) & 0xFF);
		ret->push_back((raw >> 24) & 0xFF);
		ret->push_back((raw >> 32) & 0xFF);
		ret->push_back((raw >> 40) & 0xFF);
		ret->push_back((raw >> 48) & 0xFF);
		ret->push_back((raw >> 56) & 0xFF);
		break;
	}
	return ret;
}

BOOL DetourFunction(__in BOOL bState, __inout PVOID* ppPointer, __in PVOID pDetour) {
	if (DetourTransactionBegin() == NO_ERROR)
		if (DetourUpdateThread(GetCurrentThread()) == NO_ERROR)
			if ((bState ? DetourAttach : DetourDetach)(ppPointer, pDetour) == NO_ERROR)
				if (DetourTransactionCommit() == NO_ERROR)
					return TRUE;
	return FALSE;
}

char __fastcall Decode1_Detour(void* addr) {
	char ret = _Decode1(addr);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			build_in->push_back(toByteArr(ret, 1));
		}
	}
	return ret;
}

int __fastcall Decode2_Detour(void* addr) {
	int ret = _Decode2(addr);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			if (build_in->size() != 1 || !blacklist_in.count(ret)) {
				build_in->push_back(toByteArr(ret, 2));
			}
			else {
				delete build_in->front();
				delete build_in;
				build_in = 0;
			}
		}
	}
	return ret;
}

int __fastcall Decode4_Detour(void* addr) {
	int ret = _Decode4(addr);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			build_in->push_back(toByteArr(ret, 4));
		}
	}
	return ret;
}

int64_t __fastcall Decode8_Detour(void* addr) {
	int64_t ret = _Decode8(addr);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			build_in->push_back(toByteArr(ret, 8));
		}
	}
	return ret;
}

DWORD* __fastcall DecodeStr_Detour(void* addr, void* dummy, void* str) {
	char** s = (char**) _DecodeStr(addr, str);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			vector<char> *ret = new vector<char>();
			ret->push_back(CODE_STR);
			size_t len = strlen(*s);
			ret->push_back(len & 0xFF);
			ret->push_back((len >> 8) & 0xFF);
			for (char* i = *s; *i != 0; ++i) {
				ret->push_back(*i);
			}
			build_in->push_back(ret);
		}
	}
	return (DWORD*)s;
}

void __fastcall DecodeBuffer_Detour(void* addr, void* dummy, void* p, int n) {
	_DecodeBuffer(addr, p, n);
	if ((int)addr == 1555556) {
		if (build_in != 0) {
			vector<char> *ret = new vector<char>();
			ret->push_back(CODE_BUFFER);
			for (int i = 0; i < n; ++i) {
				ret->push_back(*((char*)p + i));
			}
			build_in->push_back(ret);
		}
	}
}

void __fastcall CInPacket_Detour(void* addr, void* dummy, void* buf) {	
	if ((int)addr == 1555556) {
		if (build_in == 0 && logging) {
			build_in = new list<vector<char>*>;
			vector<char> *ret = new vector<char>();
			ret->push_back(0);
			build_in->push_back(ret);
		}
	}
	_CInPacket(addr, buf);
}

void __fastcall MemoryFree_Detour(void* addr) {
	if ((int)addr == 1555564) {
		if (build_in != 0 && build_in->size() > 1) {
			lock_guard<mutex> lock(packets_mutex);
			packets.push_back(build_in);
			build_in = 0;
		}
	}
	else if ((int)addr == build_out_addr + 4 || (int)addr == build_out_addr) {
		//MessageBox(NULL, L"OUT PACKET FREE", L"OUT PACKET FREE", MB_ICONERROR);
		if (build_out != 0 && build_out->size() > 1) {
			lock_guard<mutex> lock(packets_mutex);
			packets.push_back(build_out);
			build_out = 0;
		}
	}
	else {
		//wchar_t buff[100];
		//swprintf_s(buff, L"OUT PACK FREE %d", (int)addr);
		//MessageBox(NULL, buff, buff, MB_ICONERROR);
	}
	_MemoryFree(addr);
}

void __fastcall Encode1_Detour(void* addr, void* dummy, char in) {
	_Encode1(addr, in);
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			build_out->push_back(toByteArr(in, 1));
		}
	}
}

void __fastcall Encode2_Detour(void* addr, void* dummy, short in) {
	_Encode2(addr, in);
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			build_out->push_back(toByteArr(in, 2));
		}
	}
}

void __fastcall Encode4_Detour(void* addr, void* dummy, int in) {
	_Encode4(addr, in);
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			build_out->push_back(toByteArr(in, 4));
		}
	}
}

void __fastcall Encode8_Detour(void* addr, void* dummy, int64_t in) {
	_Encode8(addr, in);
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			build_out->push_back(toByteArr(in, 8));
		}
	}
}

void __fastcall EncodeStr_Detour(void* addr, void* dummy, void* s) {
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			vector<char> *ret = new vector<char>();
			ret->push_back(CODE_STR);
			size_t len = strlen((char*)s);
			ret->push_back(len & 0xFF);
			ret->push_back((len >> 8) & 0xFF);
			for (char* i = (char*)s; *i != 0; ++i) {
				ret->push_back(*i);
			}
			build_out->push_back(ret);
		}
	}
	_EncodeStr(addr, s);
}

void __fastcall EncodeBuffer_Detour(void* addr, void* dummy, void* p, int n) {
	if ((int)addr == build_out_addr) {
		if (build_out != 0) {
			vector<char> *ret = new vector<char>();
			ret->push_back(CODE_BUFFER);
			for (int i = 0; i < n; ++i) {
				ret->push_back(*((char*)p + i));
			}
			build_out->push_back(ret);
		}
	}
	_EncodeBuffer(addr, p, n);
}

void __fastcall COutPacket_Detour(void* addr, void* dummy, int opcode) {
	build_out_addr = (int) addr;
	if (build_out == 0 && logging && !blacklist_out.count(opcode)) {
		build_out = new list<vector<char>*>;
		vector<char> *ret = new vector<char>();
		ret->push_back(1);
		build_out->push_back(ret);
	}
	else if (build_out != 0) {
		//wchar_t buff[100];
		//swprintf_s(buff, L"CONCURRENT OUTPACKET DETECTED %d", (int)addr);
		//MessageBox(NULL, buff, buff, MB_ICONERROR);
	}
	_COutPacket(addr, opcode);
}

void ClearPackets() {
	lock_guard<mutex> lock(packets_mutex);
	vector<list<vector<char>*>*>::iterator packets_iter = packets.begin();
	for (; packets_iter != packets.end(); ++packets_iter) {
		list<vector<char>*>* packet = *packets_iter;
		list<vector<char>*>::iterator it = packet->begin();
		for (; it != packet->end(); ++it) {
			delete *it;
		}
		delete packet;
	}
	packets.clear();
}



