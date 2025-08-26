#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#include "../../error_log.h"
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
#include    "../lib/SerializeBuffer_exception/SerializeBuffer_exception.h"
#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")
#include <vector>