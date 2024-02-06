//
//  main.c
//  Librif Playdate example
//
//  Created by Matteo D'Ignazio on 11/03/22.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "librif_luaglue.h"

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg) {
    
    if(event == kEventInitLua){
        librif_init(playdate);
        
        librif_register_lua();
    }
	
	return 0;
}
