#pragma once
#include <set>

struct st_Sector_Pos
{
    st_Sector_Pos() = default;

    st_Sector_Pos(int iX, int iY)
        : _iX(iX), _iY(iY)
    {

    }
    bool operator<(const st_Sector_Pos &other) const
    {
        if (this->_iX != other._iX)
            return this->_iX < other._iX;
        return this->_iY < other._iY;
    }
    bool operator==(const st_Sector_Pos &other) const
    {
        return this->_iX == other._iX && this->_iY == other._iY;
    }
    int _iX, _iY;
};
struct st_Sector_Around
{
    st_Sector_Around()
    {
        // Around.reserve(6);
    }
    std::set<st_Sector_Pos> Around;
};