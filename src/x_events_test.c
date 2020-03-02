//
// Copyright(C) 2020 Dave Voutila
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Event logging framework and utils
//

#include <stdio.h>
#include "p_mobj.h"
#include "x_events.h"

int main()
{
    mobj_t m1, m2;

    if (X_InitLog(69, 69) != 0) {
        printf("failed to init log\n");
        return -1;
    }
    X_LogStart(69, 69, 1);
    X_LogArmorPickup(69);
    X_LogWeaponPickup(wp_shotgun);
    X_CloseLog();
    return 0;
}
