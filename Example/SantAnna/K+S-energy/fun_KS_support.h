/******************************************************************************

	SUPPORT C FUNCTIONS
	-------------------

	Pure C support functions used in the objects in the K+S LSD model are 
	coded below.
 
 ******************************************************************************/

/*======================== GENERAL SUPPORT C FUNCTIONS =======================*/

// calculate the bounded, moving-average growth rate of variable
// if lim is zero, there is no bounding

double mov_avg_bound( object *obj, const char *var, double lim, double per )
{
	double prev, g, sum_g;
	int i;
	
	for ( sum_g = i = 0; i < per; ++i )
	{
		if ( t - i <= 0 )						// just go to t=0
			break;
			
		prev = VLS( obj, var, i + 1 );
		g = ( prev != 0 ) ? VLS( obj, var, i ) / prev - 1 : 0;
		
		if ( lim > 0 )
			g = max( min( g, lim ), - lim );	// apply bounds
		
		sum_g += g;
	}

	return sum_g / i;
}


// append error messages and increment error counter

void check_error( bool cond, const char* errMsg, int errCount, int *errCounter )
{
	if ( ! cond )
		return;
	
	if ( errCount == 0 )
		LOG( " %s", errMsg );
	else
		LOG( " %s(%d)", errMsg, errCount );
	
	++( *errCounter );
}


/*====================== FINANCIAL SUPPORT C FUNCTIONS =======================*/

// comparison function for sort method in equation 'cScores', '_cScores'

bool rank_desc_NWtoS( firmRank e1, firmRank e2 ) 
{ 
	return e1.NWtoS > e2.NWtoS; 
}


// update firm debt in equations '_EIe', '_TaxE'

void update_debtE( object *firm, double desired, double loan )
{
	double newDebt;
	
	if ( desired > 0 )							// ignore loan repayment
	{
		INCRS( firm, "_CDe", desired );			// desired credit
		INCRS( firm, "_CDeC", desired - loan );	// credit constraint
		INCRS( firm, "_CSe", loan );			// supplied credit
	}
	
	if ( loan != 0 )
	{
		newDebt = INCRS( firm, "_DebE", loan );	// increment firm's debt stock
		if ( newDebt < 0.001 )
			WRITES( firm, "_DebE", 0 );			// write-off very small debt
		
		object *bank = HOOKS( firm, BANK );		// firm's bank
		double TCeFree = VS( bank, "_TCeFree" );// available credit at firm's bank
	
		// if credit limit active, adjust bank's available credit
		if ( TCeFree > -0.1 )
			WRITES( bank, "_TCeFree", max( TCeFree - loan, 0 ) );
	}
}


// update firm debt in equation '_Q1', '_Tax1'

void update_debt1( object *firm, double desired, double loan )
{
	double newDebt;
	
	if ( desired > 0 )							// ignore loan repayment
	{
		INCRS( firm, "_CD1", desired );			// desired credit
		INCRS( firm, "_CD1c", desired - loan );	// credit constraint
		INCRS( firm, "_CS1", loan );			// supplied credit
	}
	
	if ( loan != 0 )
	{
		newDebt = INCRS( firm, "_Deb1", loan );	// increment firm's debt stock
		if ( newDebt < 0.001 )
			WRITES( firm, "_Deb1", 0 );			// write-off very small debt

		object *bank = HOOKS( firm, BANK );		// firm's bank
		double TC1free = VS( bank, "_TC1free" );// available credit firm's bank

		// if credit limit active, adjust bank's available credit
		if ( TC1free > -0.1 )
			WRITES( bank, "_TC1free", max( TC1free - loan, 0 ) );
	}
}


// update firm debt in equations '_Q2', '_EI', '_SI', '_Tax2'

void update_debt2( object *firm, double desired, double loan )
{
	double newDebt;
	
	if ( desired > 0 )							// ignore loan repayment
	{
		INCRS( firm, "_CD2", desired );			// desired credit
		INCRS( firm, "_CD2c", desired - loan );	// credit constraint
		INCRS( firm, "_CS2", loan );			// supplied credit
	}
	
	if ( loan != 0 )
	{
		newDebt = INCRS( firm, "_Deb2", loan );	// increment firm's debt stock
		if ( newDebt < 0.001 )
			WRITES( firm, "_Deb2", 0 );			// write-off very small debt
	
		object *bank = HOOKS( firm, BANK );		// firm's bank
		double TC2free = VS( bank, "_TC2free" );// available credit at firm's bank
	
		// if credit limit active, adjust bank's available credit
		if ( TC2free > -0.1 )
			WRITES( bank, "_TC2free", max( TC2free - loan, 0 ) );
	}
}


/*================== CAPITAL MANAGEMENT SUPPORT C FUNCTIONS ==================*/

// send machine brochure to consumption-good client firm in equation '_NC'

object *send_brochure( int supplID, object *suppl, int clientID, object *client )
{
	object *broch, *cli;
	
	cli = ADDOBJS( suppl, "Cli" );				// add object to new client
	WRITES( cli, "__IDc", clientID );			// update client ID
	WRITES( cli, "__tSel", T );					// update selection time

	broch = ADDOBJS( client, "Broch" );			// add brochure to client
	WRITES( broch, "__IDs", supplID );			// update supplier ID
	WRITE_SHOOKS( broch, cli );					// pointer to supplier client list
	WRITE_SHOOKS( cli, broch );					// pointer to client brochure list
	
	return broch;
}


// send new machine order in equations '_EI', '_SI'

void send_order( object *firm, double nMach )
{
	// find firm entry on supplier client list
	object *cli = SHOOKS( HOOKS( firm, SUPPL ) );
	
	if ( VS( cli, "__tOrd" ) < T )				// if first order in period
	{
		WRITES( cli, "__nOrd", nMach );			// set new order size
		WRITES( cli, "__tOrd", T );				// set order time
		WRITES( cli, "__nCan", 0 );				// no machine canceled yet
	}
	else
		INCRS( cli, "__nOrd", nMach );			// increase existing order size
}


// perform investment according to available funding in equations '_EI', '_SI'

double invest( object *firm, double desired )
{
	double invest, invCost, loan, loanDes;
	
	if ( desired <= 0 )
		return 0;
		
	double m2 = VS( PARENTS( firm ), "m2" );	// machine output per period
	double _CS2a = VS( firm, "_CS2a" );			// available credit supply
	double _NW2 = VS( firm, "_NW2" );			// net worth (cash available)
	double _p1 = VS( PARENTS( SHOOKS( HOOKS( firm, SUPPL ) ) ), "_p1" );

	invCost = _p1 * desired / m2;				// desired investment cost

	if ( invCost <= _NW2 - 1 )					// can invest with own funds?
	{
		invest = desired;						// invest as planned
		_NW2 -= invCost;						// remove machines cost from cash
	}
	else
	{
		if ( invCost <= _NW2 - 1 + _CS2a )		// possible to finance all?
		{
			invest = desired;					// invest as planned
			loan = loanDes = invCost - _NW2 + 1;// finance the difference
			_NW2 = 1;							// keep minimum cash
		}
		else									// credit constrained firm
		{
			// invest as much as the available finance allows, rounded # machines
			invest = max( floor( ( _NW2 - 1 + _CS2a ) / _p1 ) * m2, 0 );
			loanDes = invCost - _NW2 + 1;		// desired credit
			
			if ( invest == 0 )
				loan = 0;						// no finance
			else
			{
				invCost = _p1 * invest / m2;	// reduced investment cost
				
				if ( invCost <= _NW2 - 1 )		// just own funds?
				{
					loan = 0;
					_NW2 -= invCost;			// remove machines cost from cash
				}
				else
				{
					loan = invCost - _NW2 + 1;	// finance the difference
					_NW2 = 1;					// keep minimum cash
				}
			}
		}

		update_debt2( firm, loanDes, loan );	// update debt (desired/granted)
	}

	if ( invest > 0 )
	{
		WRITES( firm, "_NW2", _NW2 );			// update the firm net worth
		send_order( firm, round( invest / m2 ) );// order to machine supplier
	}

	return invest;
}


// add new vintage to the capital stock of a firm in equation 'K' and 'initCountry'

void add_vintage( object *firm, double nMach, bool newInd )
{
	double __AeeVint, __AefVint, __AlpVint, __pVint;
	int __ageVint, __nMach, __nVint;
	object *cap, *cons, *cur, *suppl, *vint;
	
	suppl = PARENTS( SHOOKS( HOOKS( firm, SUPPL ) ) );// current supplier
	__nMach = floor( nMach );					// integer number of machines
	
	// at t=1 firms have a mix of machines: old to new, many suppliers
	if ( newInd )
	{
		cap = SEARCHS( GRANDPARENTS( firm ), "Capital" ); 
		cons = SEARCHS( GRANDPARENTS( firm ), "Consumption" );
		
		__ageVint = VS( cons, "eta" ) + 1;		// age of oldest machine
		__nVint = ceil( nMach / __ageVint );	// machines per vintage
		__AeeVint = INIEEFF;					// initial energy efficiency
		__AefVint = INIEFRI;					// initial envir. friendliness
		__AlpVint = INIPROD;					// initial labor productivity
		__pVint = VLS( cap, "p1avg", 1 );		// initial machine price
	}
	else
	{
		__ageVint = 1 - T;
		__nVint = __nMach;
		__AeeVint = VS( suppl, "_AtauEE" );
		__AefVint = VS( suppl, "_AtauEF" );
		__AlpVint = VS( suppl, "_AtauLP" );
		__pVint = VS( suppl, "_p1" );
	}
	
	while ( __nMach > 0 )
	{
		if ( newInd )
		{
			cur = RNDDRAW_FAIRS( cap, "Firm1" );// draw another supplier
			if ( cur == suppl )					// don't use current supplier
				continue;
				
			vint = ADDOBJLS( firm, "Vint", T - 1 );// recalculate in t=1
		}
		else
		{
			cur = suppl;						// just use current supplier
			vint = ADDOBJS( firm, "Vint" );		// just recalculate in next t
		}
			
		WRITE_SHOOKS( vint, HOOKS( firm, TOPVINT ) );// save previous vintage
		WRITE_HOOKS( firm, TOPVINT, vint );		// save pointer to top vintage		
	
		WRITES( vint, "__IDvint", VNT( T, VS( cur, "_ID1" ) ) );// vintage ID
		WRITES( vint, "__AeeVint", __AeeVint );	// vintage energy efficiency
		WRITES( vint, "__AefVint", __AefVint );	// vintage envir. friendliness
		WRITES( vint, "__AlpVint", __AlpVint );	// vintage labor productivity
		WRITES( vint, "__nVint", __nVint );		// number of machines in vintage
		WRITES( vint, "__pVint", __pVint );		// price of machines in vintage
		WRITES( vint, "__tVint", 1 - __ageVint );// vintage build time
		
		__nMach -= __nVint;
		--__ageVint;
	
		if ( __ageVint > 0 && __nMach % __ageVint == 0 )// exact ratio missing?
			__nVint = __nMach / __ageVint;		// adjust machines per vintage
	}
}


// scrap (remove) vintage from capital stock in equation 'K'
// return -1 if last vintage (not removed but shrank to 1 machine)

double scrap_vintage( variable *var, object *vint )
{
	double RS;
	
	if ( NEXTS( vint ) != NULL )				// don't remove last vintage
	{
		// remove as previous vintage from next vintage
		if ( SHOOKS( NEXTS( vint ) ) == vint )
			WRITE_SHOOKS( NEXTS( vint ), NULL );
		
		RS = abs( VS( vint, "__RSvint" ) );					
		DELETE( vint );							// delete vintage
	}
	else
	{
		RS = -1;								// signal last machine
		WRITES( vint, "__nVint", 1 );			// keep just 1 machine
	}
	
	return RS;
}


/*=================== FIRM ENTRY-EXIT SUPPORT C FUNCTIONS ====================*/

// add and configure entrant energy producer firm object(s) and required hooks 
// in equations 'entryEexit' and 'initCountry'

double entry_firmE( variable *var, object *sector, int n, bool newInd )
{
	double AtauDE, AtauDEmax, AtauDEmin, ICtauGE, ICtauGEmax, ICtauGEmin, NWe, 
		   NWe0, emTauDE, emTauDEavg, fE, fKge, pE, mult, equity = 0;
	int IDe, IDb, tEent;
	object *firm, *bank, *cli, *plant,
		   *fin = SEARCHS( PARENTS( sector ), "Financial" ),
		   *lab = SEARCHS( PARENTS( sector ), "Labor" );
	
	double DebE0ratio = VS( sector, "DebE0ratio" );// bank fin. to equity ratio
	double Phi5 = VS( sector, "Phi5" );			// lower support for wealth share
	double Phi6 = VS( sector, "Phi6" );			// upper support for wealth share
	double alpha3 = VS( sector, "alpha3" );		// lower support for imitation
	double beta3 = VS( sector, "beta3" );		// upper support for imitation
	double bE = VS( sector, "bE" );				// required payback period
	double muE0 = VS( sector, "muE0" );			// initial mark-up in energy s.
	double pF = VS( sector, "pF" );				// fossil fuel price
	double x6 = VS( sector, "x6" );				// entrant upper advantage
	double w = VS( lab, "w" );					// current wage
	
	if ( newInd )
	{
		AtauDE = VS( sector, "Ade0" );			// ini. efficiency dirty plant 
		ICtauGE = bE * pF / AtauDE;				// initial green plant cost
		NWe0 = VS( sector, "NWe0" ); 			// initial wealth in energy sec.
		emTauDE = VS( sector, "emDE0" );		// initial emissions dirty plant
		fE = 1.0 / n;							// fair share
		fKge = VS( sector, "fGE0" ); 			// initial share of green plants
		pE = muE0 * w + ( fKge == 1 ? 0 : pF / AtauDE );// initial price
		tEent = 0;								// entered before t=1
	}
	else
	{
		AtauDEmax = MAXS( sector, "_AtauDE" );	// max dirty energy efficiency
		AtauDEmin = MINS( sector, "_AtauDE" );	// min dirty energy efficiency
		ICtauGEmax = MAXS( sector, "_ICtauGE" );// max green plant cost
		ICtauGEmin = MINS( sector, "_ICtauGE" );// min green plant cost
		NWe0 = max( WHTAVES( sector, "_NWe", "_fE" ), VS( sector, "NWe0" ) );
		emTauDE = AVES( sector, "_emTauDE" );	// average dirty energy emissions
		fE = 0;									// no market share
		fKge = WHTAVES( sector, "_fKge", "_fE" );// average share of green plants
		pE = WHTAVES( sector, "_pE", "_fE" );	// average power price
		tEent = T;								// entered now
	}
	
	// add entrant firms (end of period, don't try to sell)
	for ( ; n > 0; --n )
	{
		IDe = INCRS( sector, "lastIDe", 1 );	// new firm ID
		
		// create object, always recalculate in t
		firm = ADDOBJLS( sector, "FirmE", T - 1 );
		ADDHOOKS( firm, FIRMEHK );				// add object hooks
		
		// select associated bank and create hooks to/from it
		IDb = VS( fin, "pickBank" );			// draw bank
		bank = V_EXTS( PARENTS( sector ), country, bankPtr [ IDb - 1 ] );// bank object
		WRITES( firm, "_bankE", IDb );
		WRITE_HOOKS( firm, BANK, bank );		
		cli = ADDOBJS( bank, "CliE" );			// add to bank client list
		WRITES( cli, "__IDe", IDe );
		WRITE_SHOOKS( cli, firm );				// pointer back to client
		WRITE_HOOKS( firm, BCLIENT, cli );		// pointer to bank client list
		
		// remove empty plant instances
		plant = SEARCHS( firm, "Dirty" );
		DELETE( plant );
		plant = SEARCHS( firm, "Green" );
		DELETE( plant );

		if ( ! newInd )
		{
			// draw initial dirty efficiency/green cost (imitation from best firm)
			AtauDE = AtauDEmin + ( AtauDEmax - AtauDEmin ) * 
								 ( 1 + x6 ) * beta( alpha3, beta3 );
			ICtauGE = ICtauGEmax - ( ICtauGEmax - ICtauGEmin ) *
								 ( 1 + x6 ) * beta( alpha3, beta3 );
		}
		
		// initial cost, price and net wealth
		if ( VS( PARENTS( sector ), "flagEnClim" ) != 0 )
		{
			mult = newInd ? 1 : uniform( Phi5, Phi6 );// NW multiple
			NWe = mult * NWe0;					// initial free cash
			equity += NWe * ( 1 - DebE0ratio );	// accumulated equity (all firms)
		}
		else
			NWe = 0;

		// initialize variables
		WRITES( firm, "_IDe", IDe );
		WRITES( firm, "_tEent", tEent );
		WRITELLS( firm, "_AtauDE", AtauDE, tEent, 1 );
		WRITELLS( firm, "_ICtauGE", ICtauGE, tEent, 1 );
		WRITELLS( firm, "_DebE", NWe * DebE0ratio, tEent, 1 );
		WRITELLS( firm, "_NWe", NWe, tEent, 1 );
		WRITELLS( firm, "_emTauDE", emTauDE, tEent, 1 );
		WRITELLS( firm, "_fE", fE, tEent, 1 );
		WRITELLS( firm, "_fKge", fKge, tEent, 1 );
		WRITELLS( firm, "_muE", muE0 * w, tEent, 1 );
		WRITELLS( firm, "_pE", pE, tEent, 1 );
		WRITELLS( firm, "_qcE", 4, tEent, 1 );
	}
	
	return equity;								// equity cost of entry(ies)
}


// add and configure entrant capital-good firm object(s) and required hooks 
// in equations 'entry1exit' and 'initCountry'

double entry_firm1( variable *var, object *sector, int n, bool newInd )
{
	double AtauEE, AtauEF, AtauLP, AtauLPmax, BtauEE, BtauEF, BtauLP, BtauLPmax, 
		   D10, NW1, NW10, RD0, c1, cTau, f1, p1, mult, equity = 0;
	int ID1, IDb, t1ent;
	object *firm, *bank, *cli, 
		   *cons = SEARCHS( PARENTS( sector ), "Consumption" ), 
		   *ene = SEARCHS( PARENTS( sector ), "Energy" ),
		   *fin = SEARCHS( PARENTS( sector ), "Financial" ),
		   *lab = SEARCHS( PARENTS( sector ), "Labor" );
	
	double Deb10ratio = VS( sector, "Deb10ratio" );// bank fin. to equity ratio
	double Phi3 = VS( sector, "Phi3" );			// lower support for wealth share
	double Phi4 = VS( sector, "Phi4" );			// upper support for wealth share
	double alpha2 = VS( sector, "alpha2" );		// lower support for imitation
	double beta2 = VS( sector, "beta2" );		// upper support for imitation
	double mu1 = VS( sector, "mu1" );			// mark-up in sector 1
	double m1 = VS( sector, "m1" );				// worker production scale
	double nu = VS( sector, "nu" );				// share of R&D expenses
	double pE = VS( ene, "pE" );				// energy price
	double trCO2 = VS( PARENTS( sector ), "trCO2" );// carbon tax rate
	double x5 = VS( sector, "x5" );				// entrant upper advantage
	double w = VS( lab, "w" );					// current wage
	
	if ( newInd )
	{
		double F20 = VS( cons, "F20" );
		double m2 = VS( cons, "m2" );			// machine output per period

		AtauEE = BtauEE = INIEEFF;				// initial products.
		AtauEF = BtauEF = INIEFRI;
		AtauLP = AtauLPmax = INIPROD;
		BtauLP = BtauLPmax = ( 1 + mu1 ) * AtauLP / 
							 ( m1 * m2 * VS( cons, "b" ) );

		NW10 = VS( sector, "NW10" ); 			// initial wealth in sector 1
		f1 = 1.0 / n;							// fair share
		t1ent = 0;								// entered before t=1
		
		// initial demand expectation, assuming all sector 2 firms, 1/eta 
		// replacement factor and fair share in sector 1 and full employment
		double p20 = VLS( cons, "CPI", 1 );
		double K0 = ceil( VS( lab, "Ls0" ) * INIWAGE / p20 / F20 / m2 ) * m2;
		
		D10 = F20 * K0 / m2 / VS( cons, "eta" ) / n;
	}
	else
	{
		AtauEE = AVES( sector, "_AtauEE" );		// avg. machine energy efficiency
		AtauEF = AVES( sector, "_AtauEF" );		// avg. machine envir. friendl.
		AtauLPmax = MAXS( sector, "_AtauLP" );	// best machine lab. productivity
		BtauEE = AVES( sector, "_BtauEE" );		// avg. energy effic. in sector 1
		BtauEF = AVES( sector, "_BtauEF" );		// avg. env. friend. in sector 1
		BtauLPmax = MAXS( sector, "_BtauLP" );	// best productivity in sector 1
		NW10 = max( WHTAVES( sector, "_NW1", "_f1" ), VS( sector, "NW10" ) * 
					VS( sector, "PPI" ) / VS( sector, "PPI0" ) );
		f1 = 0;									// no market share
		t1ent = T;								// entered now
		
		// initial demand equal to 1 machine per client under fair share entry
		D10 = VS( cons, "F2" ) / VS( sector, "F1" );
	}
	
	// add entrant firms (end of period, don't try to sell)
	for ( ; n > 0; --n )
	{
		ID1 = INCRS( sector, "lastID1", 1 );	// new firm ID
		
		// create object, only recalculate in t if new industry
		if ( newInd )
			firm = ADDOBJLS( sector, "Firm1", T - 1 );
		else
			firm = ADDOBJS( sector, "Firm1" );
		
		ADDHOOKS( firm, FIRM1HK );				// add object hooks
		
		// select associated bank and create hooks to/from it
		IDb = VS( fin, "pickBank" );			// draw bank
		bank = V_EXTS( PARENTS( sector ), country, bankPtr [ IDb - 1 ] );// bank object
		WRITES( firm, "_bank1", IDb );
		WRITE_HOOKS( firm, BANK, bank );		
		cli = ADDOBJS( bank, "Cli1" );			// add to bank client list
		WRITES( cli, "__ID1", ID1 );
		WRITE_SHOOKS( cli, firm );				// pointer back to client
		WRITE_HOOKS( firm, BCLIENT, cli );		// pointer to bank client list
		
		// remove empty client instance
		cli = SEARCHS( firm, "Cli" );
		DELETE( cli );

		if ( ! newInd )
		{
			// initial labor productivity (imitation from best firm)
			AtauLP = beta( alpha2, beta2 );		// draw A from Beta(alpha,beta)
			AtauLP *= AtauLPmax * ( 1 + x5 );	// fraction of top firm
			BtauLP = beta( alpha2, beta2 );		// draw B from Beta(alpha,beta)
			BtauLP *= BtauLPmax * ( 1 + x5 );	// fraction of top firm
		}
		
		// initial cost, price and net wealth
		mult = newInd ? 1 : uniform( Phi3, Phi4 );// NW multiple
		c1 = ( w / BtauLP + ( pE + trCO2 * BtauEF ) / BtauEE ) / m1;// unit cost
		cTau = w / AtauLP + ( pE + trCO2 * AtauEF ) / AtauEE;// u. cost clients
		p1 = ( 1 + mu1 ) * c1;					// unit price
		RD0 = nu * D10 * p1;					// R&D expense
		NW1 = mult * NW10;						// initial free cash
		equity += NW1 * ( 1 - Deb10ratio );		// accumulated equity (all firms)

		// initialize variables
		WRITES( firm, "_ID1", ID1 );
		WRITES( firm, "_t1ent", t1ent );
		WRITELLS( firm, "_AtauEE", AtauEE, t1ent, 1 );
		WRITELLS( firm, "_AtauEF", AtauEF, t1ent, 1 );
		WRITELLS( firm, "_AtauLP", AtauLP, t1ent, 1 );
		WRITELLS( firm, "_BtauEE", BtauEE, t1ent, 1 );
		WRITELLS( firm, "_BtauEF", BtauEF, t1ent, 1 );
		WRITELLS( firm, "_BtauLP", BtauLP, t1ent, 1 );
		WRITELLS( firm, "_Deb1", NW1 * Deb10ratio, t1ent, 1 );
		WRITELLS( firm, "_L1rd", RD0 / w, t1ent, 1 );
		WRITELLS( firm, "_NW1", NW1, t1ent, 1 );
		WRITELLS( firm, "_RD", RD0, t1ent, 1 );
		WRITELLS( firm, "_cTau", cTau, t1ent, 1 );
		WRITELLS( firm, "_f1", f1, t1ent, 1 );
		WRITELLS( firm, "_p1", p1, t1ent, 1 );
		WRITELLS( firm, "_qc1", 4, t1ent, 1 );
		
		if ( ! newInd )
		{
			WRITES( firm, "_AtauEE", AtauEE );
			WRITES( firm, "_AtauEF", AtauEF );
			WRITES( firm, "_AtauLP", AtauLP );
			WRITES( firm, "_BtauEE", BtauEE );
			WRITES( firm, "_BtauEF", BtauEF );
			WRITES( firm, "_BtauLP", BtauLP );
			WRITES( firm, "_Deb1", NW1 * Deb10ratio );
			WRITES( firm, "_L1rd", RD0 / w );
			WRITES( firm, "_NW1", NW1 );
			WRITES( firm, "_RD", RD0 );
			WRITES( firm, "_c1", c1 );
			WRITES( firm, "_cTau", cTau );
			WRITES( firm, "_p1", p1 );
			
			// compute variables requiring calculation in t 
			RECALCS( firm, "_Deb1max" );		// prudential credit limit
			RECALCS( firm, "_NC" );				// set initial clients
			VS( firm, "_CS1a" );				// update credit supply
		}
	}
	
	return equity;								// equity cost of entry(ies)
}


// add and configure entrant consumer-good firm object(s) and required hooks 
// in equations 'entry2exit' and 'initCountry'

double entry_firm2( variable *var, object *sector, int n, bool newInd )
{
	double A2, D20, D2e, Eavg, K, Kd, N, NW2, NW2f, NW20, Q2u, c2, f2, f2posChg, 
		   life2cycle, p2, mult, equity = 0;
	int ID2, IDb, t2ent;
	object *firm, *bank, *cli, *suppl, *broch, *vint,
		   *cap = SEARCHS( PARENTS( sector ), "Capital" ), 
		   *ene = SEARCHS( PARENTS( sector ), "Energy" ),
		   *fin = SEARCHS( PARENTS( sector ), "Financial" ),
		   *lab = SEARCHS( PARENTS( sector ), "Labor" );

	double Deb20ratio = VS( sector, "Deb20ratio" );// bank fin. to equity ratio
	double Phi1 = VS( sector, "Phi1" );			// lower support for K share
	double Phi2 = VS( sector, "Phi2" );			// upper support for K share
	double iota = VS( sector, "iota" );			// desired inventories factor
	double mu20 = VS( sector, "mu20" );			// initial mark-up in sector 2
	double m2 = VS( sector, "m2" );				// machine output per period
	double p10 = VLS( cap, "p1avg", 1 );		// initial machine price
	double pE = VS( ene, "pE" );				// energy price
	double trCO2 = VS( PARENTS( sector ), "trCO2" );// carbon tax rate
	double u = VS( sector, "u" );				// desired capital utilization
	double w = VS( lab, "w" );					// current wage

	if ( newInd )
	{
		double phi = VS( lab, "phi" );			// unemployment benefit rate
		double c10 = p10 / ( 1 + VS( cap, "mu1" ) );// initial unit cost sec. 1
		double c20 = INIWAGE / INIPROD + 		// initial unit cost sec. 2
					 ( pE + trCO2 * INIEFRI ) / INIEEFF;
		double p20 = ( 1 + mu20 ) * c20;		// initial consumer-good price
		double trW = VS( PARENTS( sector ), "flagTax" ) > 0 ? 
					 VS( PARENTS( sector ), "tr" ) : 0;// tax rate on wages
		double K0 = ceil( VS( lab, "Ls0" ) * INIWAGE / 
						  p20 / n / m2 ) * m2;	// full employment K required
		double SIr0 = n * K0 / m2 / VS( sector, "eta" );// substit. real invest.
		double RD0 = VS( cap, "nu" ) * SIr0 * p10;// initial R&D expense
		
		// initial steady state demand under fair share
		D20 = ( ( SIr0 * c10 + RD0 ) * ( 1 - phi - trW ) + 
				VS( lab, "Ls0" ) * INIWAGE * phi ) / 
			  ( mu20 + phi + trW ) * c20 / n;
		Eavg = VLS( sector, "Eavg", 1 );		// initial competitiveness
		K = K0;									// initial capital in sector 2
		N = iota * D20;							// initial inventories
		NW20 = VS( sector, "NW20" );			// initial wealth in sector 2
		Q2u = 1;								// initial capacity utilization
		f2 = 1.0 / n;							// fair share
		f2posChg = 0;							// m.s. of post-change firms
		life2cycle = 1;							// start as incumbent
		t2ent = 0;								// entered before t=1
	}
	else
	{
		D20 = 0;
		Eavg = VS( sector, "Eavg" );			// average competitiveness
		K = WHTAVES( sector, "_K", "_f2" );		// w. avg. capital in sector 2
		N = 0;									// inventories
		NW20 = WHTAVES( sector, "_NW2", "_f2" );// average wealth in sector 2
		Q2u = VS( sector, "Q2u" );				// capacity utilization
		f2 = 0;									// no market share
		life2cycle = 0;							// start as pre-operat. entrant
		t2ent = T;								// entered now
	}

	// add entrant firms (end of period, don't try to sell)
	for ( ; n > 0; --n )
	{
		ID2 = INCRS( sector, "lastID2", 1 );	// new firm ID
		
		// create object, only recalculate in t if new industry
		if ( newInd )
			firm = ADDOBJLS( sector, "Firm2", T - 1 );
		else
			firm = ADDOBJS( sector, "Firm2" );
		
		ADDHOOKS( firm, FIRM2HK );				// add object hooks
		vint = SEARCHS( firm, "Vint" );			// remove empty vintage instance		
		DELETE( vint );
		
		// select associated bank and create hooks to/from it
		IDb = VS( fin, "pickBank" );			// draw bank
		bank = V_EXTS( PARENTS( sector ), country, bankPtr[ IDb - 1 ] );// bank object
		WRITES( firm, "_bank2", IDb );
		WRITE_HOOKS( firm, BANK, bank );
		cli = ADDOBJS( bank, "Cli2" );			// add to bank client list
		WRITES( cli, "__ID2", ID2 );			// update object
		WRITE_SHOOKS( cli, firm );				// pointer back to client
		WRITE_HOOKS( firm, BCLIENT, cli );		// pointer to bank client list
		
		// draw current machine supplier and create hooks to/from it
		suppl = RNDDRAWS( cap, "Firm1", "_AtauLP" );// try to draw good supplier
		INCRS( suppl, "_NC", 1 );
		cli = ADDOBJS( suppl, "Cli" );			// add to supplier client list
		WRITES( cli, "__IDc", ID2 );			// update object
		WRITES( cli, "__tSel", T );
		broch = SEARCHS( firm, "Broch" );		// add to firm brochure list
		WRITES( broch, "__IDs", VS( suppl, "_ID1" ) );// update object
		WRITE_SHOOKS( broch, cli );				// pointer to supplier cli. list
		WRITE_SHOOKS( cli, broch );				// pointer to client broch. list
		WRITE_HOOKS( firm, SUPPL, broch );		// pointer to current supplier
			
		// initial desired capital/expected demand, rounded to # of machines
		mult = newInd ? 1 : uniform( Phi1, Phi2 );// capital multiple
		Kd = ceil( max( mult * K / m2, 1 ) ) * m2; 
		D2e = newInd ? D20 : u * Kd;
		
		// define entrant initial free cash (1 period wages or default minimum)
		mult = newInd ? 1 : uniform( Phi1, Phi2 );// NW multiple
		A2 = VS( suppl, "_AtauLP" );			// initial labor productivity
		c2 = VS( suppl, "_cTau" ); 				// initial unit cost
		p2 = ( 1 + mu20 ) * c2;					// initial price
		NW2f = ( 1 + iota ) * D2e * c2;			// initial free cash
		NW2f = max( NW2f, mult * NW20 );
		
		// initial equity must pay initial capital and wages
		NW2 = newInd ? NW2f : VS( suppl, "_p1" ) * Kd / m2 + NW2f;
		equity += NW2 * ( 1 - Deb20ratio );		// accumulated equity (all firms)

		// initialize variables
		WRITES( firm, "_ID2", ID2 );
		WRITES( firm, "_t2ent", t2ent );
		WRITES( firm, "_life2cycle", life2cycle );		
		WRITELLS( firm, "_A2", A2, t2ent, 1 );
		WRITELLS( firm, "_Deb2", NW2 * Deb20ratio, t2ent, 1 );
		WRITELLS( firm, "_f2", f2, t2ent, 1 );
		WRITELLS( firm, "_f2", f2, t2ent, 2 );
		WRITELLS( firm, "_mu2", mu20, t2ent, 1 );
		WRITELLS( firm, "_p2", p2, t2ent, 1 );
		WRITELLS( firm, "_qc2", 4, t2ent, 1 );
		
		for ( int i = 1; i <= 4; ++i )
		{
			WRITELLS( firm, "_D2", D2e, t2ent, i );
			WRITELLS( firm, "_D2d", D2e, t2ent, i );
		}

		if ( newInd )
		{
			WRITELLS( firm, "_K", Kd, t2ent, 1 );
			WRITELLS( firm, "_N", N, t2ent, 1 );
			WRITELLS( firm, "_NW2", NW2, t2ent, 1 );
			
			add_vintage( firm, Kd / m2, newInd );// first machine vintages
		}
		else
		{
			WRITES( firm, "_A2", A2 );
			WRITES( firm, "_D2e", D2e ); 
			WRITES( firm, "_Deb2", NW2 * Deb20ratio );
			WRITES( firm, "_E", Eavg );
			WRITES( firm, "_Kd", Kd );
			WRITES( firm, "_NW2", NW2 );
			WRITES( firm, "_Q2u", Q2u );
			WRITES( firm, "_c2", c2 );
			WRITES( firm, "_c2e", c2 );
			WRITES( firm, "_mu2", mu20 );
			WRITES( firm, "_p2", p2 );

			// compute variables requiring calculation in t 
			RECALCS( firm, "_Deb2max" );		// prudential credit limit
			VS( firm, "_CS2a" );				// update credit supply
		}
	}

	return equity;								// equity cost of entry(ies)
}


// remove energy producer firm object and exiting hooks in equation 'entryEexit'

double exit_firmE( variable *var, object *firm )
{
	double liqVal = VS( firm, "_NWe" ) - VS( firm, "_DebE" );
	object *bank;
	
	if ( liqVal < 0 )							// account bank losses, if any
	{
		bank = HOOKS( firm, BANK );				// exiting firm bank
		VS( bank, "_BadDeb" );					// ensure reset in t
		INCRS( bank, "_BadDeb", - liqVal );		// accumulate bank losses
	}
	
	DELETE( HOOKS( firm, BCLIENT ) );			// leave client list of bank
	
	DELETE( firm );
	
	return max( liqVal, 0 );					// liquidation credit, if any
}


// remove capital-good firm object and exiting hooks in equation 'entry1exit'

double exit_firm1( variable *var, object *firm )
{
	double liqVal = VS( firm, "_NW1" ) - VS( firm, "_Deb1" );
	object *bank, *firm2;
	
	if ( liqVal < 0 )							// account bank losses, if any
	{
		bank = HOOKS( firm, BANK );				// exiting firm bank
		VS( bank, "_BadDeb" );					// ensure reset in t
		INCRS( bank, "_BadDeb", - liqVal );		// accumulate bank losses
	}
	
	DELETE( HOOKS( firm, BCLIENT ) );			// leave client list of bank
	
	CYCLES( firm, firm2, "Cli" )				// leave supplier lists of s. 2
		DELETE( SHOOKS( firm2 ) );				// delete from firm broch. list
		
	DELETE( firm );
	
	return max( liqVal, 0 );					// liquidation credit, if any
}


// remove consumer-good firm object and exiting hooks in equation 'entry2exit'

double exit_firm2( variable *var, object *firm )
{
	double fires, liqVal = VS( firm, "_NW2" ) - VS( firm, "_Deb2" );
	object *bank, *firm1;

	
	if ( liqVal < 0 )							// account bank losses, if any
	{
		bank = HOOKS( firm, BANK );				// exiting firm bank
		VS( bank, "_BadDeb" );					// ensure reset in t
		INCRS( bank, "_BadDeb", - liqVal );		// accumulate bank losses
	}
	
	DELETE( HOOKS( firm, BCLIENT ) );			// leave client list of bank
	
	CYCLES( firm, firm1, "Broch" )				// leave client lists of s. 1
		DELETE( SHOOKS( firm1 ) );				// delete from firm client list
	
	// update firm map before removing LSD object
	EXEC_EXTS( GRANDPARENTS( firm ), country, firm2map, erase, ( int ) VS( firm, "_ID2" ) );
		
	DELETE( firm );

	return max( liqVal, 0 );					// liquidation credit, if any
}
