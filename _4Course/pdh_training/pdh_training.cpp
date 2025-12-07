

#include <Pdh.h>
#include <stdio.h>

#pragma comment(lib, "Pdh.lib")

#include "CCpuUsage.h"

int main(void)
{
    // PDH 쿼리 핸들 생성
    PDH_HQUERY cpuQuery;
    PdhOpenQuery(NULL, NULL, &cpuQuery);

    PDH_HCOUNTER Process_PrivateByte;
    PdhAddCounter(cpuQuery, L"\\Process(AggregatorHost)\\Private Bytes", NULL, &Process_PrivateByte);

    PDH_HCOUNTER Process_NonpagedByte;
    PdhAddCounter(cpuQuery, L"\\Process(AggregatorHost)\\Pool Nonpaged Bytes", NULL, &Process_NonpagedByte);

    PDH_HCOUNTER Available_Byte;
    PdhAddCounter(cpuQuery, L"\\Memory\\Available MBytes", NULL, &Available_Byte);

    PDH_HCOUNTER Nonpaged_Byte;
    PdhAddCounter(cpuQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &Nonpaged_Byte);

    PdhCollectQueryData(cpuQuery);
    CCpuUsage CPUTime;

    {
        PDH_FMT_COUNTERVALUE Process_PrivateByteVal;
        PDH_FMT_COUNTERVALUE Process_Nonpaged_ByteVal;
        PDH_FMT_COUNTERVALUE Available_Byte_ByteVal;
        PDH_FMT_COUNTERVALUE Nonpaged_Byte_ByteVal;

        while (true)
        {
            // 1초마다 갱신
            PdhCollectQueryData(cpuQuery);

            // 갱신 데이터 얻음
            PDH_FMT_COUNTERVALUE counterVal;
            PdhGetFormattedCounterValue(Process_PrivateByte, PDH_FMT_DOUBLE, NULL, &Process_PrivateByteVal);
            wprintf(L"Process_PrivateByte : %lf Byte\n", Process_PrivateByteVal.doubleValue);

 
            PdhGetFormattedCounterValue(Process_NonpagedByte, PDH_FMT_DOUBLE, NULL, &Process_Nonpaged_ByteVal);
            wprintf(L"Process_Nonpaged_Byte : %lf Byte\n", Process_Nonpaged_ByteVal.doubleValue);

            PdhGetFormattedCounterValue(Available_Byte, PDH_FMT_DOUBLE, NULL, &Available_Byte_ByteVal);
            wprintf(L"Available_Byte : %lf Byte\n", Available_Byte_ByteVal.doubleValue);

            PdhGetFormattedCounterValue(Nonpaged_Byte, PDH_FMT_DOUBLE, NULL, &Nonpaged_Byte_ByteVal);
            wprintf(L"Nonpaged_Byte_ByteVal : %lf Byte\n", Nonpaged_Byte_ByteVal.doubleValue);

            // 얻은 데이터 사용

 /*           CPUTime.UpdateCpuTime();
            wprintf(L"Processor:%f / Process:%f \n", CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
            wprintf(L"ProcessorKernel:%f / ProcessKernel:%f \n", CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
            wprintf(L"ProcessorUser:%f / ProcessUser:%f \n", CPUTime.ProcessorUser(), CPUTime.ProcessUser());
            wprintf(L"==================================\n");*/

            Sleep(1000);
        }
    }

    return 0;
}
