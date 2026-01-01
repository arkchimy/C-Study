#include "SectorManager.h"

#include <algorithm>

void stSectorManager::SetSectorAround(int iX, int iY, st_Sector_Around *pSectorAround)
{
    for (int row = -1; row <= 1; row++)
    {
        for (int column = -1; column <= 1; column++)
        {
            int rx = std::clamp(iX + row, 0, _MaxX - 1);
            int ry = std::clamp(iY + column, 0, _MaxY - 1);
 
            st_Sector_Pos temp(rx,ry);
            pSectorAround->Around.insert(temp);
        }
    }
}