/******************************************************************************

	STATISTICS EQUATIONS
	--------------------

	Equations that are not required for the model to run but may produce
	useful country- or sector-level statistics for analysis.
 
 ******************************************************************************/

/*========================= COUNTRY-LEVEL STATS ==============================*/

EQUATION( "CD" )
/*
Total credit demand
*/
RESULT( VS( SECSTAL2, "CD1" ) + VS( SECSTAL2, "CD2" ) )


EQUATION( "CDc" )
/*
Total credit demand constraint
*/
RESULT( VS( SECSTAL2, "CD1c" ) + VS( SECSTAL2, "CD2c" ) )


EQUATION( "CS" )
/*
Total credit supplied
*/
RESULT( VS( SECSTAL2, "CS1" ) + VS( SECSTAL2, "CS2" ) )


EQUATION( "Creal" )
/*
Real aggregated consumption
*/
RESULT( VS( GRANDPARENT, "C" ) / VS( CONSECL2, "CPI" ) )


EQUATION( "DefGDP" )
/*
Government deficit on GDP ratio
*/
v[1] = VS( GRANDPARENT, "GDPnom" );
RESULT( v[1] > 0 ? VS( GRANDPARENT, "Def" ) / v[1] : CURRENT )


EQUATION( "GDI" )
/*
Gross domestic income (nominal terms)
*/
RESULT( VS( CAPSECL2, "W1" ) + VS( CONSECL2, "W2" ) +
		VS( CAPSECL2, "Pi1" ) + VS( CONSECL2, "Pi2" ) + 
		VS( FINSECL2, "PiB" ) + VS( GRANDPARENT, "G" ) - 
		VS( GRANDPARENT, "Tax" ) + 
		VS( CAPSECL2, "PPI" ) * VS( CONSECL2, "SI" ) / VS( CONSECL2, "m2" ) )


EQUATION( "dA" )
/*
Overall labor productivity growth rate
*/
v[1] = VLS( GRANDPARENT, "A", 1 );
RESULT( v[1] > 0 ? VS( GRANDPARENT, "A" ) / v[1] - 1 : 0 )


/*========================= FINANCIAL SECTOR STATS ===========================*/

EQUATION( "BadDeb" )
/*
Total bad debt (defaults) in financial sector
*/
RESULT( SUMS( FINSECL2, "_BadDeb" ) )


EQUATION( "BadDebAcc" )
/*
Accumulated losses from bad debt in financial sector
*/
RESULT( CURRENT + VS( SECSTAL2, "BadDeb" ) )


EQUATION( "Bda" )
/*
Firms financial fragility defined as the ratio between accumulated bad debt (loans
in default) and total bank assets (stock of loans)
*/
RESULT( SUMS( FINSECL2, "_Bda" ) )


EQUATION( "Bfail" )
/*
Rate of failing banks
*/
VS( FINSECL2, "NWb" );							// make sure it is updated
RESULT( COUNT_CNDS( FINSECL2, "Bank", "_Gbail", ">", 0 ) / 
		COUNTS( FINSECL2, "Bank" ) )


EQUATION( "ExRes" )
/*
Excess reserves (free cash) hold by financial sector
*/
RESULT( SUMS( FINSECL2, "_ExRes" ) )


EQUATION( "HHb" )
/*
Normalized Herfindahl-Hirschman index for financial sector
*/
i = COUNTS( FINSECL2, "Bank" );
RESULT( i > 1 ? max( 0, ( WHTAVES( FINSECL2, "_fB", "_fB" ) - 1.0 / i ) / 
						( 1 - 1.0 / i ) ) : 1 )


EQUATION( "HPb" )
/*
Hymer-Pashigian index for financial sector
*/

v[0] = 0;										// index accumulator
CYCLES( FINSECL2, cur, "Bank" )
	v[0] += fabs( VLS( cur, "_fB", 1 ) - VS( cur, "_fB" ) );// sum share changes

RESULT( v[0] )	


EQUATION( "TC" )
/*
Total credit supply provided by financial sector
Negative value (-1) means unlimited credit
*/

if ( VS( GRANDPARENT, "flagCreditRule" ) < 1 )
	END_EQUATION( -1 );

RESULT( SUMS( FINSECL2, "_TC" ) )


/*======================= CAPITAL-GOOD SECTOR STATS ==========================*/

EQUATION( "AtauAvg" )
/*
Average labor productivity of machines supplied by capital-good sector
*/
RESULT( AVES( CAPSECL2, "_Atau" ) )


EQUATION( "BtauAvg" )
/*
Average labor productivity of machines produced by capital-good sector
*/
RESULT( AVES( CAPSECL2, "_Btau" ) )


EQUATION( "CD1" )
/*
Total credit demand of firms in capital-good sector
*/
RESULT( SUMS( CAPSECL2, "_CD1" ) )


EQUATION( "CD1c" )
/*
Total credit demand constraint of firms in capital-good sector
*/
RESULT( SUMS( CAPSECL2, "_CD1c" ) )


EQUATION( "CS1" )
/*
Total credit supplied to firms in capital-good sector
*/
RESULT( SUMS( CAPSECL2, "_CS1" ) )


EQUATION( "HCavg" )
/*
Number of historical clients of capital-good firms
*/
RESULT( AVES( CAPSECL2, "_HC" ) )


EQUATION( "HH1" )
/*
Normalized Herfindahl-Hirschman index for capital-good sector
*/
i = COUNTS( CAPSECL2, "Firm1" );
RESULT( i > 1 ? max( 0, ( WHTAVES( CAPSECL2, "_f1", "_f1" ) - 1.0 / i ) / 
						( 1 - 1.0 / i ) ) : 1 )


EQUATION( "HP1" )
/*
Hymer-Pashigian index for capital-good sector
*/

v[0] = 0;										// index accumulator
CYCLES( CAPSECL2, cur, "Firm1" )
	v[0] += fabs( VLS( cur, "_f1", 1 ) - VS( cur, "_f1" ) );// sum share changes

RESULT( v[0] )	


EQUATION( "NCavg" )
/*
Number of new clients of capital-good firms
*/
RESULT( AVES( CAPSECL2, "_NC" ) )


EQUATION( "RD" )
/*
R&D expenditure of capital-good sector
*/
RESULT( SUMS( CAPSECL2, "_RD" ) )


EQUATION( "age1avg" )
/*
Average age of firms in capital-good sector
*/
RESULT( T - AVES( CAPSECL2, "_t1ent" ) )


/*======================= CONSUMER-GOOD SECTOR STATS =========================*/

EQUATION( "A2sd" )
/*
Standard deviation of machine-level log labor productivity of firms in 
consumption-good sector
*/

v[1] = VS( CONSECL2, "A2" );					// average productivity
if ( v[1] <= 0 )
	END_EQUATION( 0 );							// probably no production yet

v[1] = log( v[1] + 1 );							// average log productivity

i = 0;											// valid cases count
v[0] = 0;										// square difference accumulator
CYCLES( CONSECL2, cur, "Firm2" )
{
	v[2] = VS( cur, "_A2" );
	if ( is_finite( v[2] ) && v[2] > 0 )
	{
		v[0] += pow( log( v[2] + 1 ) - v[1], 2 );
		++i;
	}
}

RESULT( i > 0 ? sqrt( v[0] / i ) : 0 )


EQUATION( "CD2" )
/*
Total credit demand of firms in consumer-good sector
*/
RESULT( SUMS( CONSECL2, "_CD2" ) )


EQUATION( "CD2c" )
/*
Total credit demand constraint of firms in consumer-good sector
*/
RESULT( SUMS( CONSECL2, "_CD2c" ) )


EQUATION( "CS2" )
/*
Total credit supplied to firms in consumer-good sector
*/
RESULT( SUMS( CONSECL2, "_CS2" ) )


EQUATION( "HH2" )
/*
Normalized Herfindahl-Hirschman index for consumption-good sector
*/
i = COUNTS( CONSECL2, "Firm2" );
RESULT( i > 1 ? max( 0, ( WHTAVES( CONSECL2, "_f2", "_f2" ) - 1.0 / i ) / 
						( 1 - 1.0 / i ) ) : 1 )


EQUATION( "HP2" )
/*
Hymer-Pashigian index for consumption-good sector
*/

v[0] = 0;										// index accumulator
CYCLES( CONSECL2, cur, "Firm2" )
	v[0] += fabs( VLS( cur, "_f2", 1 ) - VS( cur, "_f2" ) );// sum share changes

RESULT( v[0] )	


EQUATION( "L2larg" )
/*
Number of workers of largest firm in consumption-good sector
*/
RESULT( MAXS( CONSECL2, "_L2" ) )


EQUATION( "RS2" )
/*
Machine (planned) scrapping rate in consumption-good sector
*/
v[1] = VLS( CONSECL2, "K", 1 );
RESULT( T > 1 && v[1] > 0 ? SUMS( CONSECL2, "_RS2" ) / 
		( v[1] / VS( CONSECL2, "m2" ) ) : 0 )


EQUATION( "age2avg" )
/*
Average age of firms in consumption-good sector
*/
RESULT( T - AVES( CONSECL2, "_t2ent" ) )


EQUATION( "dN" )
/*
Change in total inventories (real terms)
*/
RESULT( VS( CONSECL2, "N" ) - VLS( CONSECL2, "N", 1 ) )


EQUATION( "mu2avg" )
/*
Weighted average mark-up of consumption-good sector
*/
RESULT( WHTAVES( CONSECL2, "_mu2", "_f2" ) )


EQUATION( "nBrochAvg" )
/*
Average number of machine brochures available to firms in consumer-good sector
*/

v[0] = 0;
CYCLES( CONSECL2, cur, "Firm2" )
	v[0] += COUNTS( cur, "Broch");

RESULT( v[0] / VS( CONSECL2, "F2" ) )


/*============================= LABOR STATS ==================================*/

EQUATION( "V" )
/*
Effective vacancy rate (unfilled positions over total labor supply)
*/
RESULT( T > 1 ? min( ( VS( CAPSECL2, "JO1" ) + VS( CONSECL2, "JO2" ) ) / 
					   VS( LABSUPL2, "Ls" ), 1 ) : 0 )


EQUATION( "dw" )
/*
Nominal average wage growth rate
*/
RESULT( log( VS( LABSUPL2, "w" ) ) - log( VLS( LABSUPL2, "w", 1 ) ) )


EQUATION( "wReal" )
/*
Centralized real wage imposed to all workers
*/
RESULT( VS( LABSUPL2, "w" )  / VS( CONSECL2, "CPI" ) )


/*============================ AGENT-LEVEL STATS =============================*/

EQUATION( "_A2e" )
/*
Machine-level effective productivity of firm in consumption-good sector
*/
i = V( "_L2" );
RESULT( i > 0 ? V( "_Q2e" ) / i : CURRENT )


EQUATION( "_RS2" )
/*
Number of machines to scrap of firm in consumption-good sector
*/

v[0] = 0;
CYCLE( cur, "Vint" )
	v[0] += abs( VS( cur, "_RSvint" ) );
	
RESULT( v[0] )


/*============================= DUMMY EQUATIONS ==============================*/

EQUATION_DUMMY( "exit1fail", "" )
/*
Rate of exiting bankrupt firms in capital-good sector
Updated in 'entry1exit'
*/

EQUATION_DUMMY( "exit2fail", "" )
/*
Rate of exiting bankrupt firms in consumption-good sector
Updated in 'entry2exit'
*/
