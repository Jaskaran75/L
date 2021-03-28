
#include <math.h>
#include "fun_head_fast.h"

#define UNKNOWN 3								  // unknown island - yellow
#define KNOWN 2									  // known island - green
#define COLONIZED 1								// colonized island - red
#define SEA 5									    // sea (not an island) - blue
#define EXPLORER 1000							// explorer ship - white
#define IMITATOR 0								// imitator ship - black

MODELBEGIN

EQUATION( "Init" )
//FAST;											// comment to show log/debug messages
PLOG("Init");
double island_counter=0;                                 // v[0]
double known_Island_Counter=VL("known_Island_Counter",1);// v[1]
double island_prob=V( "pi" );                            // v[2]
double init_known_island= VL( "l", 1 );                  // v[3]
double l0radius= V( "l0radius" );                        // v[4]
//double rho= V( "rho" );                               // v[5]
double minSgnPrb= V( "minSgnPrb" );                    // v[6]
double LatticeSize= V( "sizeLattice" );               // v[7]

// check if there are enough islands on the defined radius for setting known islands
if ( init_known_island > l0radius * l0radius && ! ( init_known_island == 1 && l0radius == 0 ) )
{
	//PLOG( "\n\nError: invalid l0=%.0lf for l0radius=%.0lf\n", init_known_island, l0radius);
	ABORT;
	END_EQUATION( 0 );
}

MAKE_UNIQUE("Island");
MAKE_UNIQUE("Agent");


if ( LatticeSize > 0 && ! V( "latticeOpen" ) )
{
	i = min( LatticeSize, 2 * LAST_T ) + 1;
	INIT_SPACE_SINGLE(0,0,i,i);
	INIT_LAT_GIS(SEA);
	WRITES( p->up, "latticeOpen", 1 );			// avoid more than one lattice instance
	WRITE( "seaShown", 1 );						     // signal this Sea instance has the lattice
	k = 1;

}
else
	k = 0;


INIT_NET( "Island", "DISCONNECTED", 1 );//LSD network
SET_GIS_DISTANCE_TYPE('e');//Euclidean distance

cur=SEARCH("Island");//
auto ID=UIDS(cur);
for ( i = 0; i <= LatticeSize ;++i )
	for ( j = 0; j <= LatticeSize; ++j )
			// draw the existence of an island (except at the center)
		if ( RND < island_prob && ! ( i == LatticeSize/2 && j == LatticeSize/2 ) )

	{

				WRITE("addATx",i);
				WRITE("addATy",j);
				V( "addIsland" );
			//	//PLOG( "\nIsland=at x=%d y=%d ",i,j);
	}
//PLOG("know island is %g init know is %g",V("known_Island_Counter"),init_known_island);//keep track of total known island

//add center island and make it knownIsland
//PLOG("adding center island\n")
WRITE("addATx",LatticeSize/2);
WRITE("addATy",LatticeSize/2);
V( "addIsland" );
V("makeKnown");
//WRITE("addATy",LatticeSize/2);
WRITE("known_Island_Counter",++known_Island_Counter);

j=V("N");//number of agents
//add one agent in the center (initially known island)
WRITE("addATx",LatticeSize/2);
WRITE("addATy",LatticeSize/2);
V("addAgentTOSpace");
cur1=SEARCH_POSITION_XY("Agent",50,50);


//make random islands around the center known within the radius
RCYCLE_NEIGHBOURS(cur1, cur, "Island", l0radius ){
	if (known_Island_Counter< init_known_island){
//PLOG("\nMaking island known at x=%g y=%g",POSITION_XS(cur),POSITION_YS(cur));
		if (VS(cur,"_known")==0)//island is not known yet
			{
				WRITE("addATx",POSITION_XS(cur));
				WRITE("addATy",POSITION_YS(cur));
				V("makeKnown");
				WRITES(cur, "_s",abs( POSITION_XS(cur)-50) +abs( POSITION_YS(cur)-50));// island prod. coeff.
				known_Island_Counter++;
				WRITE("known_Island_Counter",known_Island_Counter);
				//PLOG("\n _s is %g",VS(cur,"_s"));
 			}
	}
}

i=1;
while (i<j){// generate agents
 RCYCLE_NEIGHBOUR_CNDS( cur1, cur, "Island", l0radius, "_known","!=", 1 ){
	double x=VS(cur,"_xIsland");double y=VS(cur,"_yIsland");
	WRITE("addATx",x);
	WRITE("addATy",y);
	V("addAgentTOSpace");
 //PLOG("\n AGENT AT  at x=%g y=%g %d and %d ",POSITION_XS(cur),POSITION_YS(cur),i,j);
 i++;
 break;
	}
}

object *todel=SEARCH("Agent");
DELETE(todel); //delete one extra agent

PLOG("=========== init endss");

PARAMETER;						// turn variable into parameter
RESULT( 0)

EQUATION( "addIsland" )

i=V("addATx");
j=V("addATy");

cur = ADDOBJ( "Island" );			// add new object instance
ADD_TO_SPACE_XYS_WHERE(cur,i, j,c);
// INCR("Total_island_count",1);
WRITES( cur, "_xIsland", i );				// save island x coordinate
WRITES( cur, "_yIsland", j );
auto ID=UIDS(cur);
WRITES( cur, "_idIsland", ID);
WRITE_LAT_GIS_XY(i,j,UNKNOWN);
////LOG( "\nIsland=%.0lf at x=%g y=%g with ID=%g",V("Total_island_count"), POSITION_XS(cur), POSITION_YS(cur),ID );

RESULT(0)

EQUATION( "makeKnown" )

i=V("addATx");
j=V("addATy");
cur= SEARCH_POSITION_XY_WHERE("Island",i,j,c);
if (cur == NULL){
	//PLOG("not found");
	END_EQUATION( 0 );
}

//PLOG("\n found to make known");

WRITES(cur,"_known",1);
WRITE_LAT_GIS_XY(i,j,KNOWN);
auto ID=UIDS(cur);
WRITES(cur,"_idKnown",ID+10000);
INCR("known_Island_Counter",1);
V( "makeLink");
//PLOG("\nmake known at %g %g known=%g %g %g with _s\n",POSITION_XS(cur),POSITION_YS(cur),
     //VS(cur,"_idKnown"),VS(cur,"_xIsland"),VS(cur,"_yIsland"));

RESULT(0)

EQUATION( "addAgentTOSpace" )

i=V("addATx");
j=V("addATy");

cur = ADDOBJ( "Agent" );			// add new object instance
INCR("AgentCount",1);
cur1= SEARCH_POSITION_XY_WHERE("Island",i,j,c);
if (cur1 == NULL){
	//PLOG("not found");
	END_EQUATION( 0 );
}
//PLOG("\n found to add agent");
ADD_TO_SPACE_XYS_WHERE(cur,i, j,c);
auto IDisland=UIDS(cur1);
WRITES(cur,"_OnIslandID",IDisland);
auto ID=UIDS(cur);
WRITES( cur, "_idAgent", ID );
WRITES( cur, "_xAgent",VS(cur1,"_xIsland"));
WRITES( cur, "_yAgent",VS(cur1,"_yIsland"));
WRITELS( cur, "_a", 1, -1 );//Starts with being a Miner
WRITES( cur, "_activeMiner", 1 );
//	WRITES(cur1,"colonyCount",COUNT_POSITION_XY ("Agent", POSITION_XS(cur), POSITION_YS(cur)));

WRITE_LAT_GIS_XY(VS( cur, "_xAgent" ),VS( cur, "_yAgent" ),COLONIZED);
//PLOG("\n agent added at %g %g with Mining %g on island %g wth colony count %g",POSITION_XS(cur),POSITION_YS(cur),
		//	VS(cur,"_activeMiner"),VS(cur,"_OnIslandID"),COUNT_POSITIONS(cur1,"Agent"));

RESULT(0)

EQUATION( "Step" )
/*
Technical variable to force the calculation of any Variable
that has to be calculated early in the time step.
Must be the second variable in the list, after 'Init'.
*/
v[1] = V( "simSpeed" );
if ( v[1] > 0 )									// avoid calling with 0 argument (win32)
	SLEEP( v[1] );								// slow down the simulation, if needed

CYCLE( cur, "Agent" )							// make sure the agents decisions are
	VS( cur, "_a" );							// taken first during the time step

RESULT( 1 )

EQUATION( "g" )
/*
Total output (GDP) growth rate.
*/

v[1] = VL( "Q", 1 );							// past period GDP
v[2] = V( "Q" );								// current GDP

if ( v[1] > 0 && v[2] > 0 )						// don't compute if Q is zero
	v[0] = log( v[2] ) - log( v[1] );
else
	v[0] = 0;

RESULT( v[0] )

EQUATION( "l" )
/*
Number of known islands (technologies).
*/
RESULT( V("known_Island_Counter") )

EQUATION( "m" )
/*
Number of agents currently mining on all islands.
*/
double sum=SUM("_activeMiner");
// PLOG("\n number of agents on all island is  %g",sum);

RESULT( sum )


EQUATION( "Q" )
/*
Total output (GDP).
*/
// PLOG("\n TOTAL GDP is is %g",SUM("_Qisland" ) );
RESULT( SUM("_Qisland" ) )



EQUATION( "_Qisland" )
/*
Total output of an island.
*/
double totalOutput=0;
double count=0;
if (V("_known")==1){
		cur1=SEARCH_POSITION_XY("Island",POSITION_X,POSITION_Y);
		FCYCLE_NEIGHBOUR(cur,"Agent",0.1){//all agents on that island
			totalOutput+=VS(cur,"_Qminer");
			count++;
		}
//PLOG("number of count is %g with %g agent count and totalOutput %g",count,COUNT_POSITIONS(cur1,"Agent"),totalOutput);
}
RESULT( totalOutput)

EQUATION( "_c" )
/*
The productivity of the island.
*/
double x=V("_xIsland");double y=V("_yIsland");
//double k=V("_known");
double Productivity=0;

if (V("_known")==1){
		cur1=SEARCH_POSITION_XY("Island",x,y);
    v[1] = COUNT_POSITION_WHERE("Agent",cur1);
	}
else
END_EQUATION(0);

if (v[1]==0)	// avoid division by zero
	v[0]=0;
else
	v[0] = V( "_Qisland" ) / v[1];
//PLOG("\n Productiity of the island %g %g is %g",POSITION_X,POSITION_Y,v[0]);
RESULT( v[0] )

///////////////////////////// MINER object equations ///////////////////////////

EQUATION( "_Qminer" )
/*
Output of miner.
*/
double indiOP=0;
cur=SEARCH_POSITION_XY("Island",POSITION_X,POSITION_Y);
if (cur!=NULL){
	 indiOP=VS(cur, "_s" ) * pow( COUNT_POSITION_WHERE("Agent", cur ), V ("alpha" ) - 1 ) * V( "_activeMiner" );
//PLOG("\nin _Qminer %g %g on ID %g %g",POSITION_X,POSITION_Y,V("_OnIslandID"),indiOP);
}

RESULT( indiOP )


//current status of the miner:1==MINER,2==EXPLORER,3==IMITATOR
EQUATION( "_a" )
//PLOG("fsdfa");
double agentID = V( "_idAgent" );							// id of current agent
i = V( "_xAgent" );								// agent coordinates in t-1
j = V( "_yAgent" );
double search_radius=0;
int directions =0;
if ( CURRENT > 1 )
{
//PLOG("\n current >1 %g %g",POSITION_X,POSITION_Y);
	WRITE( "_OnIslandID", -1);//Agent is on the sea
	WRITE("_activeMiner",0);//not miner
	cur=SEARCH_POSITION_XY("Island",i,j);
if (cur!=NULL){
	if ( VS(cur,"colonyCount")==0 && VS(cur,"_known")==1)
		WRITE_LAT_GIS_XY(i,j,KNOWN);
	else
		WRITE_LAT_GIS_XY(i,j,COLONIZED);
}
else
	WRITE_LAT_GIS_XY(i,j,SEA);

if (CURRENT==2)
	{
	  //PLOG("\ncurrent state is Explorer");
		//PLOG("\nAgent with ID=%g is explorer ",agentID);
		if ( V( "pi" ) == 0 )					// no exploration
		{
			 search_radius = V( "l0radius" );				// radius to constrain search
			 directions = 2;							// north-south move only
		}
		else
		{
			search_radius = LAST_T;						// can move anywhere
			directions = 4;							// move in four directions
		}
		h = uniform_int( 1, directions );				// decide direction to move
		switch( h )
		{
			case 1:								// north
				j = ( j < search_radius && j!= V( "sizeLattice" ) ) ? j + 1 : j;
				break;
			case 2:								// south
				v[0] = ( directions == 2 ) ? 1 : - search_radius; // limit if no exploration
				j = ( j > v[0] && j!= 0) ? j - 1 : j;
				break;
			case 3:								// east
				i = ( i < search_radius && i!= V( "sizeLattice" ) ) ? i + 1 : i;
				break;
			case 4:								// west
				i = ( i > - search_radius && i!= 0 ) ? i - 1 : i;
				break;
		}

		//LOG( "\n Agent=%.0lf explorer at x=%d y=%d to %s", agentID, i, j,
		//	 h == 1 ? "north" : h == 2 ? "south" : h == 3 ? "east" : "west" );
cur=SEARCH_POSITION_XY("Island",i,j);
	}
////////////////////////////////////////////////////////////////////////////////
if ( CURRENT == 3 )							// it is an imitator?
{
	h = V( "_xTarget" );					// target imitated island
	k = V( "_yTarget" );

	if ( abs( h - i ) > abs( k - j ) )		// distance on x larger?
		i += copysign( 1, h - i );			// get closer by the x direction
	else
		j += copysign( 1, k - j );			// get closer by the y direction

		if ( i == h && j == k )
			cur = SEARCH_POSITION_XY_WHERE("Island", i,j, c);
		else
			cur = NULL;

	//LOG( "\n Agent=%.0lf imitator at x=%d y=%d to x=%d y=%d", agentID, i, j, h, k );
}
////////////current 3 ends
WRITE( "_xAgent", i );						// update agent position
WRITE( "_yAgent", j );
TELEPORT_XY( i, j );
if ( cur != NULL )							// found an island?
{
	//PLOG(" \n AN ISLAND at %d %d",i,j);
	if ( ! VS( cur, "_known" ) )			// discovered a new island?
	{
			//PLOG("\nisland is unknown");
// compute the productivity coefficient of the discovered island
			double productivityCoeff= ( 1 + poisson( V( "lambda" ) ) ) *
					 ( abs( i ) + abs( j ) + V( "phi" ) * V( "_Qlast" ) +
						 uniform( -sqrt( 3 ), sqrt( 3 ) ) );
			WRITES(p->up,"addATx",i);
			WRITES(p->up,"addATy",j);
			V("makeKnown");//make island known
			WRITES( cur, "_s", productivityCoeff);

	}

WRITE("_activeMiner",1);//we will mine there
WRITE( "_xTarget", 0 );					// clear target coordinates
WRITE( "_yTarget", 0 );
//LOG( "\n Agent=%.0lf mining at x=%d y=%d",	 agentID, i, j );
WRITE_LAT_GIS_XY( V( "_xAgent" ), V( "_yAgent" ),COLONIZED);
WRITE( "_OnIslandID", UIDS(cur));//ID of the island Agent is staying

//PLOG("\n becomes miner again");
END_EQUATION( 1 );						// becomes a miner again
}
else
{
	if ( CURRENT == 2 )
		//set_marker( V( "seaShown" ), V( "_xAgent" ), V( "_yAgent" ),EXPLORER, v[1] );
		WRITE_LAT_GIS_XY( V( "_xAgent" ), V( "_yAgent" ),EXPLORER);
	else
		//set_marker( V( "seaShown" ), V( "_xAgent" ), V( "_yAgent" ),IMITATOR, v[1] );
		WRITE_LAT_GIS_XY( V( "_xAgent" ), V( "_yAgent" ),IMITATOR);
	END_EQUATION( CURRENT );

}
}

if ( RND < V( "epsilon" ) )
{
	cur =SEARCH_POSITION_XY("Island",i,j);
	//LOG( "\n Agent=%.0lf EXploring from x=%d y=%d with colony count %g", agentID, i, j ,VS(cur,"colonyCount"));

	WRITE( "_Qlast", VL( "_Qminer", 1 ) );	// save last output
	END_EQUATION( 2 );							// become explorer
}
	//PLOG("\n a miner evaluates becoming an imitator mining at %g %g with best c %g ",POSITION_X,POSITION_Y,V("_cBest"));
	if ( (V("_xBest" )>0.0) &&  (V("_yBest" )>0.0) &&  (V(  "_cBest" ) > VL( "_c", 1 )) )
{
	WRITE("_activeMiner",0);
	//LOG( "\n Agent=%.0lf IMITATING from x=%d y=%d to x=%.0lf y=%.0lf",			 agentID, i, j, V("_xBest" ), V("_yBest" ) );

	WRITE( "_xTarget", V( "_xBest" ) );// coordinates of new target island
	WRITE( "_yTarget", V( "_yBest" ) );
//PLOG("%%-------------------------becomes imitator -------------------------");
		END_EQUATION( 3 );						// become imitator
}
///////////////////////////////////////////////////////////////////////////////
RESULT(1)// keep mining

EQUATION( "_cBest" )
/*
The productivity of the best known island with a signal received.
*/

double total_miner= VL( "m", 1 );							// total number of miners

if ( ! V( "_activeMiner" ) || total_miner == 0 )			// only for active miners and
{
		END_EQUATION( 0 );							// avoid division by zero
	}
	double bestProd = 0;										// best productivity so far
	i = j = 0;										// best island coordinates

	double receivedPrb=0;
	double productivity=0;
	//double distance=0.0;
//PLOG("\n in cBest caller position %g %g",POSITION_X,POSITION_Y);

cur1=SEARCH_POSITION_XY("Island",POSITION_X,POSITION_Y);

double best_x=0,best_y=0;
if((V_NODEIDS( cur1 )!=NULL) && (STAT_NODES(cur1)!=0))
 {

	// PLOG("\nIsland node to cycle around in cycle is %g %g ",POSITION_XS(cur1),POSITION_YS(cur1));
CYCLE_LINKS(cur1,curl){

	//PLOG("\n cycling links");
		cur = LINKTO( curl );
		//PLOG("\n x and y is %g %g with agents %g",POSITION_XS(cur),POSITION_YS(cur),COUNT_POSITIONS(cur,"Agent"));				// object connected
  	receivedPrb=(COUNT_POSITION_WHERE("Agent",cur)/total_miner)*V_LINK(curl);
		//PLOG("\n recieved prb %g with value of link  %g with total miner%g ",receivedPrb,V_LINK(curl),total_miner);
		if (RND <receivedPrb){
			productivity=VLS(cur,"_c",1);
			if (productivity>bestProd){
				bestProd=productivity;
				best_x=VS(cur,"_xIsland");
				best_y=VS(cur,"_yIsland");
			//	PLOG("\n %g %g  best island at the end",best_x,best_y);
			}
		}
}
}

WRITE("_xBest", best_x );							// save best island coordinates
WRITE("_yBest",best_y);
//PLOG("\n   %g %g are best island at the end",best_x ,best_y);

RESULT( bestProd )

//////////////////////// KNOWNISLAND object equations //////////////////////

EQUATION( "makeLink")//number of miner on an island


i=V("addATx");
j=V("addATy");
double rho=V("rho");
double minSgnPrb=V("minSgnPrb");
cur1=SEARCH_POSITION_XY("Island",i,j);
if (cur1 == NULL){
	PLOG("not found TO MAKE LINKS");
	END_EQUATION( 0 );
}
double maxDis=log(minSgnPrb)/rho*-1;

DCYCLE_NEIGHBOURS( cur1,cur, "Island", maxDis ){

if (VS(cur,"_known")!=0 && SEARCH_LINKS( cur1, V_NODEIDS( cur ) ) == NULL)
{
	double dis=DISTANCE_BETWEEN(cur1,cur);
	if (dis==0)
			continue;

	double maxSgnPrb = exp( - rho * DISTANCE_BETWEEN(cur1,cur) );
	if (maxSgnPrb<minSgnPrb)
			continue;

		ADDLINKWS( cur1, cur, maxSgnPrb );
		ADDLINKWS( cur, cur1, maxSgnPrb );
	//PLOG("\nmake LINK between %g %g  and %g %g within %g and strength %g nad dis is %g",
	          //	POSITION_XS(cur1),POSITION_YS(cur1),POSITION_XS(cur),POSITION_YS(cur),
	                 //maxDis,maxSgnPrb,DISTANCE2(cur1,cur));
}

}
RESULT( 0)

MODELEND

// do not add Equations in this area

void close_sim( void )
{
	// close simulation special commands go here
}
