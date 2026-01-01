#pragma once

#include "SectorTypes.h"
#include <vector>

struct stSectorManager
{
    stSectorManager(int MaxX,int MaxY,int sectorSize)
    {
        // 6400 x 1280 
        // _MaxX = 6400 / sectorSize
        // _MaxY = 1280 / sectorSize
        _MaxX = MaxX / sectorSize;
        _MaxY = MaxY / sectorSize;
    }
    void Initalize()
    {
        cacheSectorAround.resize(_MaxX * _MaxY);

        for (int iX = 0; iX < _MaxX; iX++)
        {
            for (int iY = 0; iY < _MaxY; iY++)
                SetSectorAround(iX, iY, &cacheSectorAround[iX + _MaxX * iY]);
        }
    }
    void SetSectorAround(int iX, int iY,
                         st_Sector_Around *pSectorAround);

    void GetSectorAround(int iX, int iY,
                         st_Sector_Around &pSectorAround)
    {
        int idx = iX + _MaxX * iY;
        pSectorAround = cacheSectorAround[idx];
    }

    std::vector<st_Sector_Around> cacheSectorAround;
    int _MaxX,_MaxY;
};