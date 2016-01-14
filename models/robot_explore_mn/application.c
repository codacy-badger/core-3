#include <ROOT-Sim.h>
#include <strings.h>
#include "application.h"
#include "utility.c"

#define DEBUG if(0)

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state) {
        enter_t *enter_p;
	exit_t *exit_p;
	destination_t *destination_p;
	complete_t *complete_p;
        
	enter_t enter;
	exit_t exit;
	destination_t destination;
	complete_t complete;

        simtime_t timestamp;

	lp_agent_t *agent;
	lp_region_t *region;

	unsigned char *old_agent;
	unsigned int i,j;
	
        switch(event) {

                case INIT: // must be ALWAYS implemented
			
                        if(is_agent(me)){
				agent = (lp_agent_t *)malloc(sizeof(lp_agent_t));
				printf("AGENT ADD:%p\n",agent);	
				agent->complete = false;

			//	agent->region = random_region();
				agent->region = 0;
				agent->map = ALLOCATE_BITMAP(get_tot_regions());
				BITMAP_BZERO(agent->map,get_tot_regions());

				agent->count = 0;

				SetState(agent);
			}
			else{
				region = (lp_region_t *)malloc(sizeof(lp_region_t));
				region->guests = calloc(get_tot_agents(),sizeof(unsigned char *));

        			region->count = 0;     
        			region->obstacles = get_obstacles();

				SetState(region);	
			}
			
                        timestamp = (simtime_t)(20 * Random());

			if(is_agent(me)){
				BITMAP_SET_BIT(agent->map,agent->region);
				agent->count++;
				
				enter.agent = me;
                        	enter.map = agent->map;
				
				DEBUG printf("%d send ENTER to %d\n",me,agent->region);
				ScheduleNewEvent(agent->region, timestamp, ENTER, &enter, sizeof(enter));
				
				exit.agent = me;		
				exit.map = agent->map;
				
				timestamp += Expent(DELAY);
				DEBUG printf("%d send EXIT to %d\n",me,agent->region);
				ScheduleNewEvent(agent->region, timestamp, EXIT, &exit, sizeof(exit));
			}
			else{
				ScheduleNewEvent(me, timestamp, PING, NULL, 0);	
			}

                        break;

                case PING:
			DEBUG printf("Send PING\n");
                        ScheduleNewEvent(me, now + Expent(DELAY), PING, NULL, 0);
			break;

		case ENTER:
			enter_p = (enter_t *) content;
			region = (lp_region_t *) state;

			printf("Region%d process ENTER of %d\n",me,enter_p->agent);
		
			region->guests[region->count] = enter_p->map;
			
			for(i=0; i<region->count; i++){
				old_agent = region->guests[i];

				for(j=0; j<get_tot_regions(); j++){
					if(!BITMAP_CHECK_BIT(enter_p->map,j) && BITMAP_CHECK_BIT(old_agent,j))
						BITMAP_SET_BIT(enter_p->map,j);
					else if(BITMAP_CHECK_BIT(enter_p->map,j) && !BITMAP_CHECK_BIT(old_agent,j))
						BITMAP_SET_BIT(old_agent,j);
				}
			}

			region->count++;	
			DEBUG	printf("End enter Region:%d\n",me);
			
			break;

		case EXIT: 
			exit_p = (exit_t *) content;
			region = (lp_region_t *) state;
			
			destination.region = get_region(me,region->obstacles,exit_p->agent);
			
			for(i=0;i<region->count; i++){
				if(region->guests[i] == exit_p->map){
					if(i!=(region->count-1) && region->count >= 1)
						region->guests[i] = region->guests[region->count-1];
					
					region->count--;	
					break;
				}
			}
			
			DEBUG 	printf("%d send DESTINATION to %d\n",me,exit_p->agent);
			ScheduleNewEvent(exit_p->agent, now + Expent(DELAY), DESTINATION, &destination, sizeof(destination));
			
			break;

		case DESTINATION: 
			destination_p = (destination_t *) content;
			agent = (lp_agent_t *) state;
			
			agent->region = destination_p->region;
			BITMAP_SET_BIT(agent->map,destination_p->region);

                        agent->count = 0;
			for(i=0; i<get_tot_regions(); i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					agent->count++;
			}
				
			if(check_termination(agent)){
				/*
				agent->complete = true;
	                        
				complete.agent = me;
				
				if(me + 1 == n_prc_tot){
					printf("%d send COMPLETE to %d add:%p \n",me,get_tot_regions(),agent);
			 		ScheduleNewEvent(get_tot_regions(), now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				}
				else{	
					printf("%d send COMPLETE to add:%p %d\n",me,me+1,agent);
			 		ScheduleNewEvent(me + 1, now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
				}
				
				//Unnote break command to stop exploration if the termination condiction is true
				*/
				printf("%d send COMPLETE to %d add:%p \n",me,get_tot_regions(),agent);
				break;
			}
			
			timestamp = now + Expent(DELAY);			
			
			enter.agent = me;
			enter.map = agent->map;

			DEBUG printf("%d send ENTER to %d\n",me,destination_p->region);
			ScheduleNewEvent(destination_p->region, timestamp, ENTER, &enter, sizeof(enter));

			exit.agent = me;
                        exit.map = agent->map;
			
			DEBUG printf("%d send EXIT to %d\n",me,destination_p->region);
			ScheduleNewEvent(destination_p->region, timestamp + Expent(DELAY), EXIT, &exit, sizeof(exit));
			break;

		case COMPLETE:
			complete_p = (complete_t *) content;
			if(is_agent(me)){
				agent = (lp_agent_t *) state;
				agent->complete = true;
				agent->count = get_tot_regions();
			}
		
			if(complete_p->agent == me) break;

                        complete.agent = complete_p->agent;

                        if(me + 1 == n_prc_tot){
				printf("%d send COMPLETE to %d\n",me,get_tot_regions());
				ScheduleNewEvent(get_tot_regions(),  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
			}
			else{
				printf("%d send COMPLETE to %d\n",me,me+1);
				ScheduleNewEvent(me + 1,  now + Expent(DELAY), COMPLETE, &complete, sizeof(complete));
			}

			break;
		
        }
}

bool OnGVT(unsigned int me, void *snapshot) {
	lp_agent_t *agent;
	
	if(is_agent(me)){
		agent = (lp_agent_t *) snapshot;	
		
//		DEBUG{	
			unsigned int i;
			printf("Agent[%d]\t",me);
			printf("ADD:%p \t", agent);
			printf("C:%s \t", agent->complete ? "true" : "false");
			printf("VC:%d \t{",agent->count);
			for(i=0;i<get_tot_regions();i++){
				if(BITMAP_CHECK_BIT(agent->map,i))
					printf("1 ");
				else
					printf("0 ");
			}
			printf("}\n");
//		}
		
        	if(me == get_tot_regions())
                	printf("Completed work: %f%%\n", percentage(agent));
        	
		
			
		if(!check_termination(agent)){
			DEBUG	printf("[ME:%d] Complete:%f flag:%d\n",me,percentage(agent),agent->complete);
			return false;
		}
	
		DEBUG printf("%d complete execution  C:%f F:%d\n",me,percentage(agent),agent->complete);
	}
	
	return true;
}
