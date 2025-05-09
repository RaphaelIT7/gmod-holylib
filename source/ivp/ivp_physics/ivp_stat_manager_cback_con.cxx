// Copyright (C) Ipion Software GmbH 2000. All rights reserved.

// IVP_EXPORT_PROTECTED

/*******************************************************************************
 * INCLUDES
 ******************************************************************************/ 


// IVP physics includes
#include <ivp_physics.hxx>
#include <cstring>
#include <ivp_material.hxx>
#include <ivp_betterstatisticsmanager.hxx>

// IVP graphics includes
#include <ivp_stat_manager_cback_con.hxx>


/*******************************************************************************
 * DEFINES
 ******************************************************************************/ 



/*******************************************************************************
 * INTERNAL CLASSES
 ******************************************************************************/ 




/*******************************************************************************
 * METHODS
 ******************************************************************************/ 

void IVP_Statisticsmanager_Console_Callback::output_request(IVP_BetterStatisticsmanager_Data_Entity *entity) {

    switch ( entity->type ) {
    case INT_VALUE:
    case DOUBLE_VALUE:

	switch ( entity->type ) {
	case INT_VALUE:
	    ivp_message("%s%d\n", entity->text, entity->data.int_value);
	    break;
	case DOUBLE_VALUE:
	    ivp_message("%s%f\n", entity->text, entity->data.double_value);
	    break;
	default:
	    break;
	}

	break;

    case INT_ARRAY:
	{
 	    ivp_message("%s\n", entity->text);

	    int i;
	    for (i=0; i<entity->data.int_array.size; i++) {
		int value     = entity->data.int_array.array[i];
		ivp_message("%d\n", value);
	    }
	}
	break;

    case DOUBLE_ARRAY:
	{
 	    ivp_message("%s\n", entity->text);

	    int i;
	    for (i=0; i<entity->data.int_array.size; i++) {
		IVP_DOUBLE value     = entity->data.double_array.array[i];
		ivp_message("%f\n", value);
	    }
	}
	break;
    case STRING:
	{
 	    ivp_message("%s\n", entity->text);
	}
	break;
    }

    return;
}


void IVP_Statisticsmanager_Console_Callback::enable() {

    return;
}


void IVP_Statisticsmanager_Console_Callback::disable() {
    
    return;
}


IVP_Statisticsmanager_Console_Callback::IVP_Statisticsmanager_Console_Callback() {

    return;
}
