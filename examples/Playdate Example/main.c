//
//  main.c
//  Librif Playdate example
//
//  Created by Matteo D'Ignazio on 11/03/22.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "librif_pd_lua.h"

#ifdef _WINDLL
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

DllExport int eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg) {
    
    if(event == kEventInitLua){
        RIF_pd = playdate;
        
        librif_pd_lua_register();
    }
	
	return 0;
}
