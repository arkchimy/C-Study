#pragma once

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <thread>
#include <vector>
#include <iostream>

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"

#include "../../_3Course/lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
#include "../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../../_3Course/lib/CNetwork_lib/CNetwork_lib.h"
#include "../../_3Course/lib/Profiler_MultiThread/Profiler_MultiThread.h"
#include "../../_3Course/lib/CNetwork_lib/Proxy.h"
