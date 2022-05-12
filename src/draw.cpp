/*************************************************************

	LSD 8.0 - May 2022
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente and Marcelo Pereira
	LSD is distributed under the GNU General Public License

	See Readme.txt for copyright information of
	third parties' code used in LSD

 *************************************************************/

/*************************************************************
DRAW.CPP
Draws the graphical representation of the model. It is activated by
INTERF.CPP only in case a model is loaded.

The main functions contained in this file are:

- void show_graph( object *t )
initialize the canvas and calls show_obj for the root

- void draw_obj( object *top, object *sel, int level, int center, int from )
recursive function that according to the level of the object type sets the
distances among the objects. Rather rigid, but it should work nicely
for most of the model structures. It assigns all the labels (above and below the
symbols) and the writing bound to the mouse.

- void put_node( int x1, int y1, int x2, int y2, const char *str, bool sel )
Draw the circle.

- void put_line( int x1, int y1, int x2, int y2 )
draw the line

- void put_text( const char *str, const char *num, int x, int y, const char *str2 );
Draw the different texts and sets the bindings
*************************************************************/

#include "decl.h"

float level_factor[ MAX_LEVEL ];
int range;
int range_type;

/****************************************************
SHOW_GRAPH
****************************************************/
void show_graph( object *t )
{
	object *top;
	static object *last_t = NULL;

	if ( ! struct_loaded || ! strWindowOn )		// model structure window is deactivated?
	{
		cmd( "destroytop .str" );
		return;
	}

	if ( t == NULL )
		if ( last_t == NULL )
			t = root;
		else
			t = last_t;
	else
		last_t = t;

	for ( top = t; top->up != NULL; top = top->up );

	if ( ! exists_window( ".str" ) )			// build window only if needed
	{
		cmd( "newtop .str \"\" { set strWindowOn 0; set choice 23 } \"\"" );
		cmd( "wm transient .str ." );
		cmd( "sizetop .str" );
	}
	else
		cmd( "destroy .str.f" );										// or just recreate canvas

	cmd( "wm title .str \"%s%s - LSD Model Structure\"", unsaved_change() ? "*" : " ", strlen( simul_name ) > 0 ? simul_name : NO_CONF_NAME );

	cmd( "ttk::frame .str.f" );
	cmd( "ttk::canvas .str.f.c -xscrollincrement 1 -entry 0 -dark $darkTheme" );
	cmd( "pack .str.f.c -expand yes -fill both" );
	cmd( "pack .str.f -expand yes -fill both" );

	cmd( "showtop .str current yes yes no 0 0 b" );

	draw_obj( top, t );

	cmd( "set hrsizeM [ winfo width .str ]" );
	cmd( "set vrsizeM [ winfo height .str ]" );
	cmd( ".str.f.c xview scroll [ expr { - int ( $hrsizeM / 2 ) } ] units" );

	draw_buttons( );

	cmd( "bind .str.f.c <Configure> { \
			if { $hrsizeM != [ winfo width .str ] || $vrsizeM != [ winfo height .str ] } { \
				set choice_g 23 \
			} \
		}" );
	cmd( "bind .str.f.c <Button-1> { \
			if { [ info exists res_g ] } { \
				set choice_g 24 \
			} \
		}" );
	cmd( "bind .str.f.c <Double-Button-1> { \
			if { [ info exists res_g ] && [ winfo exists .m ] } { \
				destroy .list; \
				set choice 6 \
			} \
		}" );
	cmd( "bind .str.f.c <Button-2> { \
			if { [ info exists res_g ] && [ winfo exists .m ] } { \
				destroy .list; \
				set vname $res_g; \
				set useCurrObj no; \
				set nocomp [ expr { ! [ get_obj_conf $vname comp ] } ]; \
				tk_popup .str.f.c.v %%X %%Y \
			} \
		}" );
	cmd( "bind .str.f.c <Button-3> { event generate .str.f.c <Button-2> -x %%x -y %%y }" );

	cmd( "ttk::menu .str.f.c.v -tearoff 0" );
	cmd( ".str.f.c.v add command -label \"Make Current\" -command { set choice 4 }" );
	cmd( ".str.f.c.v add separator" );
	cmd( ".str.f.c.v add command -label Change -command { set choice 6 }" );
	cmd( ".str.f.c.v add command -label Rename -command { set choice 83 }" );
	cmd( ".str.f.c.v add command -label Number -command { set choice 33 }" );
	cmd( ".str.f.c.v add command -label Move -command { set choice 32 }" );
	cmd( ".str.f.c.v add command -label Delete -command { set choice 74 }" );
	cmd( ".str.f.c.v add separator" );
	cmd( ".str.f.c.v add cascade -label Add -menu .str.f.c.v.a" );
	cmd( ".str.f.c.v add separator" );
	cmd( ".str.f.c.v add checkbutton -label \"Not Compute (-)\" -variable nocomp -command { set ctxMenuCmd \"set_obj_conf $vname comp [ expr { ! $nocomp } ]\"; set choice 95 }" );	// entryconfig 14
	cmd( ".str.f.c.v add separator" );
	cmd( ".str.f.c.v add command -label \"Initial Values\" -command { set choice 21 }" );
	cmd( ".str.f.c.v add command -label \"Browse Data\" -command { set choice 34 }" );
	cmd( "ttk::menu .str.f.c.v.a -tearoff 0" );
	cmd( ".str.f.c.v.a add command -label Variable -command { set choice 2; set param 0 }" );
	cmd( ".str.f.c.v.a add command -label Parameter -command { set choice 2; set param 1 }" );
	cmd( ".str.f.c.v.a add command -label Function -command { set choice 2; set param 2 }" );
	cmd( ".str.f.c.v.a add command -label Object -command { set choice 3 }" );

	cmd( "bind .str <F1> { LsdHelp graphrep.html }" );
	set_shortcuts( ".str" );

	cmd( "if { [ winfo exists .plt ] } { lower .str .plt }" );

	cmd( "update" );
}


/****************************************************
DRAW_BUTTONS
****************************************************/
void draw_buttons( void )
{
	cmd( "set n [ scan [ .str.f.c bbox all ] \"%%d %%d %%d %%d\" x1 y1 x2 y2 ]" );
	cmd( "set cx1 [ .str.f.c canvasx 0 ]" );
	cmd( "set cy1 [ .str.f.c canvasy 0 ]" );
	cmd( "set cx2 [ .str.f.c canvasx [ winfo width .str ] ]" );
	cmd( "set cy2 [ .str.f.c canvasy [ winfo height .str ] ]" );

	cmd( "if { $n == 4 } { \
			set hratioM [ expr { ( $cx2 - $cx1 ) / ( $x2 - $x1 + 2 * $borderM ) } ]; \
			set vratioM [ expr { ( $cy2 - $cy1 ) / ( $y2 - $y1 + 2 * $borderM ) } ] \
		} { \
			set hratioM 1; \
			set vratioM 1 \
		}" );

	cmd( "ttk::button .str.f.c.hplus -text \"\u25B6\" -width 2 -style bold.Toolbutton -command { \
			set hfactM [ round_N [ expr { $hfactM + $rstepM } ] 2 ]; \
			set choice_g 23 \
		}" );
	cmd( "ttk::button .str.f.c.hminus -text \"\u25C0\" -width 2 -style bold.Toolbutton -command { \
			set hfactM [ round_N [ expr { max( $hfactM - $rstepM, $hfactMmin ) } ] 2 ]; \
			set choice_g 23 \
		}" );
	cmd( "ttk::button .str.f.c.vplus -text \"\u25BC\" -width 2 -style Toolbutton -command { \
			set vfactM [ round_N [ expr { $vfactM + $rstepM } ] 2 ]; \
			set choice_g 23 \
		}" );
	cmd( "ttk::button .str.f.c.vminus -text \"\u25B2\" -width 2 -style Toolbutton -command { \
			set vfactM [ round_N [ expr { max( $vfactM - $rstepM, $vfactMmin ) } ] 2 ]; \
			set choice_g 23 \
		}" );
	cmd( "ttk::button .str.f.c.auto -text \"A\" -width 2 -style bold.Toolbutton -command { \
			set hfactM [ round_N [ expr { $hfactM * $hratioM } ] 2 ]; \
			set vfactM [ round_N [ expr { $vfactM * $vratioM } ] 2 ]; \
			set choice_g 23 \
		}" );

	cmd( "bind .str <Control-plus> { invoke .str.f.c.hplus }" );
	cmd( "bind .str <Control-minus> { invoke .str.f.c.hminus }" );
	cmd( "bind .str <Alt-plus> { invoke .str.f.c.vplus }" );
	cmd( "bind .str <Alt-minus> { invoke .str.f.c.vminus }" );
	cmd( "bind .str <Control-a> { invoke .str.f.c.auto }; bind .str <Control-A> { invoke .str.f.c.auto }" );
	cmd( "bind .str <Alt-a> { invoke .str.f.c.auto }; bind .str <Alt-A> { invoke .str.f.c.auto }" );

	cmd( "set colM [ expr { $cx2 - $borderM - $borderMadj } ]" );
	cmd( "set rowM [ expr { $cy2 - $borderM } ]" );

	cmd( "set a [ .str.f.c create window $colM $rowM -window .str.f.c.auto -tags tooltip ]" );
	cmd( "set b [ .str.f.c create window [ expr { $colM - $bhstepM } ] $rowM -window .str.f.c.hplus -tags tooltip ]" );
	cmd( "set c [ .str.f.c create window [ expr { $colM - 2 * $bhstepM } ] $rowM -window .str.f.c.hminus -tags tooltip ]" );
	cmd( "set d [ .str.f.c create window $colM [ expr { $rowM - $bvstepM } ] -window .str.f.c.vplus -tags tooltip ]" );
	cmd( "set e [ .str.f.c create window $colM [ expr { $rowM - 2 * $bvstepM } ] -window .str.f.c.vminus -tags tooltip ]" );

	cmd( "tooltip::tooltip .str.f.c -item $a \"Automatic adjustment\"" );
	cmd( "tooltip::tooltip .str.f.c -item $b \"Increase horizontal spacing\"" );
	cmd( "tooltip::tooltip .str.f.c -item $c \"Decrease horizontal spacing\"" );
	cmd( "tooltip::tooltip .str.f.c -item $d \"Increase vertical spacing\"" );
	cmd( "tooltip::tooltip .str.f.c -item $e \"Decrease vertical spacing\"" );
}


/****************************************************
CREATE_FLOAT_LIST
****************************************************/
void create_float_list( object *t )
{
	bool sp_upd;
	variable *cv;

	// element lists used to build floating elements window
	cmd( "set tlist_%s [ list ]", t->label );
	cmd( "set slist_%s [ list ]", t->label );

	if ( t->v != NULL )
		for ( cv = t->v; cv != NULL; cv = cv->next )
		{
			// special updating scheme?
			if ( cv->param == 0 && ( cv->delay > 0 || cv->delay_range > 0 || cv->period > 1 || cv->period_range > 0 ) )
				sp_upd = true;
			else
				sp_upd = false;

			// set flags string
			cmd( "set varFlags \"%s%s%s%s%s\"", ( cv->save || cv->savei ) ? "+" : "", cv->plot ? "*" : "", cv->debug == 'd' ? "!" : "", cv->parallel ? "&" : "", sp_upd ? "\u00A7" : "" );

			if ( cv->param == 0 )
			{
				if ( cv->num_lag == 0 )
				{
					cmd( "lappend tlist_%s \"%s (V$varFlags)\"", t->label, cv->label );
					cmd( "lappend slist_%s var", t->label );
				}
				else
				{
					cmd( "lappend tlist_%s \"%s (V_%d$varFlags)\"", t->label, cv->label, cv->num_lag );
					cmd( "lappend slist_%s lvar", t->label );
				}
			}

			if ( cv->param == 1 )
			{
				cmd( "lappend tlist_%s \"%s (P$varFlags)\"", t->label, cv->label );
				cmd( "lappend slist_%s par", t->label );
			}

			if ( cv->param == 2 )
			{
				if ( cv->num_lag == 0 )
				{
					cmd( "lappend tlist_%s \"%s (F$varFlags)\"", t->label, cv->label );
					cmd( "lappend slist_%s fun", t->label );
				}
				else
				{
					cmd( "lappend tlist_%s \"%s (F_%d$varFlags)\"", t->label, cv->label, cv->num_lag );
					cmd( "lappend slist_%s lfun", t->label );
				}
			}
		}
}


/****************************************************
DRAW_OBJ
****************************************************/
void draw_obj( object *t, object *sel, int level, int center, int from, bool zeroinst )
{
	bool fit_wid, to_compute;
	double h_fact, v_fact, range_fact;
	int h, i, j, k, step_level, step_type, begin, count, max_wid, range_init;
	char str[ MAX_LINE_SIZE ], ch[ MAX_ELEM_LENGTH ], ch1[ MAX_LINE_SIZE ];
	object *cur;
	bridge *cb;

	create_float_list( t );		// create floating element list

	h_fact = get_double( "hfactM" );
	v_fact = get_double( "vfactM" );
	range_fact = get_double( "rfactM" );
	range_init = get_int( "rinitM" );
	step_level = get_int( "vstepM" );
	step_level = round( step_level * v_fact );

	// find current tree depth
	for ( j = 0, cur = t; cur->up != NULL; ++j, cur = cur->up );

	// draw node only if it is not the root
	if ( t->up != NULL )
	{
		strcpyn( ch, t->label, MAX_ELEM_LENGTH );
		strcpy( ch1, "" );

		// count number of brothers and define maximum width for number string
		for ( k = 0, cb = t->up->b; cb != NULL; ++k, cb = cb->next );

		switch ( j )
		{
			case 1:								// first line objects
				max_wid = 15 * h_fact;
				break;
			case 2:								// second line objects
				max_wid = min( 15 * h_fact, 80 * h_fact / k );
				break;
			case 3:
				max_wid = min( 7 * h_fact, 10 * h_fact / k );
				break;
			case 4:
				max_wid = min( 4 * h_fact, 6 * h_fact / k );
				break;
			default:							// all other lines
				max_wid = min( 2 * h_fact, 3 * h_fact / k );
		}


		// format number string
		if ( zeroinst )
		{
			strcpy( ch1, "0" );

			// if parent is multi-instanced, add ellipsis to the zero
			if ( t->up->up != NULL )
			{
				// must search out of the blueprint, where we are now
				// may get the wrong parent if the parent is replicated somewhere
				cur = root->search( t->up->up->label );
				if ( cur != NULL )
				{
					cb = cur->search_bridge( t->up->label );
					for ( k = 0, cur = cb->head; cur != NULL; ++k, cur = cur->next );

					if ( k > 1 )				// handle multi-instanced parents
						strcatn( ch1, "\u2026", MAX_LINE_SIZE );
				}
			}
		}
		else									// compute number of groups of this type
		{
			fit_wid = to_compute = true;
			for ( h = 0, cur = t; cur != NULL ; ++h, cur = cur->hyper_next( ) )
			{
				if ( strlen( ch1 ) >= ( unsigned ) max_wid )
				{
					strcatn( ch1, "\u2026", MAX_LINE_SIZE );
					fit_wid = false;
					break;
				}

				skip_next_obj( cur, &count );
				snprintf( str, MAX_LINE_SIZE, "%s%d", strlen( ch1 ) > 0 ? " " : "", count );
				strcatn( ch1, str, MAX_LINE_SIZE );

				to_compute = cur->to_compute;

				for ( ; cur->next != NULL; cur = cur->next ); // reaches the last object of this group
			}

			// count number of instances of parent and check for zero instances
			if ( fit_wid && t->up->up != NULL )	// first level cannot have multiple zero instances
			{
				cb = t->up->up->search_bridge( t->up->label );
				for ( k = 0, cur = cb->head; cur != NULL; ++k, cur = cur->next );

				if ( h < k )					// found zero instanced object?
					strcatn( ch1, "\u2026", MAX_LINE_SIZE );
			}

			if ( ! to_compute )
				strcatn( ch1, "-", MAX_LINE_SIZE );
		}

		if ( t->up->up != NULL )
			put_line( from, level, center );

		put_node( center, level, t->label, t == sel ? true : false );
		put_text( ch, ch1, center, level, t->label );
	}
	else
	{
		cmd( ".str.f.c delete all" );
		level = get_int( "borderM" );
		center = 0;
	}

	// limit the maximum depth of plot
	if ( j >= MAX_LEVEL )
		return;

	// count the number of son object types
	for ( i = 0, cb = t->b; cb != NULL; ++i, cb = cb->next );

	// root? adjust tree begin
	if ( j == 0 )
	{
		level -= step_level;

		// set the default horizontal scale factor per level
		for ( k = 0; k < MAX_LEVEL; ++ k )
			level_factor[ k ] = 0.3;

		// pick hand set scale factors
		switch ( i )
		{
			case 0:
			case 1:
				level_factor[ 0 ] = 0.4;
				level_factor[ 1 ] = 0.1;
				level_factor[ 2 ] = 0.5;
				level_factor[ 3 ] = 0.6;
				break;
			case 2:
				level_factor[ 0 ] = 0.5;
				level_factor[ 1 ] = 0.8;
				level_factor[ 2 ] = 0.6;
				level_factor[ 3 ] = 0.4;
				break;
			case 3:
				level_factor[ 0 ] = 0.6;
				level_factor[ 1 ] = 1.1;
				level_factor[ 2 ] = 0.4;
				level_factor[ 3 ] = 0.4;
				break;
			case 4:
				level_factor[ 0 ] = 0.7;
				level_factor[ 1 ] = 1.2;
				level_factor[ 2 ] = 0.3;
				level_factor[ 3 ] = 0.4;
				break;
			case 5:
				level_factor[ 0 ] = 0.7;
				level_factor[ 1 ] = 1.3;
				level_factor[ 2 ] = 0.2;
				level_factor[ 3 ] = 0.4;
				break;
			default:
				level_factor[ 0 ] = 0.9 + ( i - 5.0 ) / 10;
				level_factor[ 1 ] = 1.2 + ( i - 5.0 ) / 20;
				level_factor[ 2 ] = 0.3 + ( i - 5.0 ) / 30;
				level_factor[ 3 ] = 0.5 + ( i - 5.0 ) / 40;
		}

		range_type = round( level_factor[ 0 ] * range_init * h_fact );
	}
	else
	{
		// reduce object type width at each level
		range_type = round( fabs( level_factor[ j ] * range_init / pow( 2, j + range_fact ) - pow( range_init * 2 / 3, 1 / j ) + 1 ) * h_fact );
	}

	if ( i <= 1 )					// single object type son?
	{
		begin = center;				// adjust to print below parent
		step_type = 0;
	}
	else
	{
		begin = center - range_type / 2;
		step_type = range_type / ( i - 1 );
	}

	// draw sons
	for ( i = begin, cb = t->b; cb != NULL; i += step_type, cb = cb->next )
		if ( cb->head != NULL )
			draw_obj( cb->head, sel, level + step_level, i, center, zeroinst );
		else
		{	// try to draw zero instance objects
			cur = blueprint->search( cb->blabel );
			if ( cur != NULL )
				draw_obj( cur, sel, level + step_level, i, center, true );
		}
}


/****************************************************
PUT_NODE
****************************************************/
void put_node( int x, int y, const char *str, bool sel )
{
	cmd( ".str.f.c create oval [ expr { %d - $nsizeM / 2 } ] [ expr { %d + $vmarginM - $nsizeM / 2 } ] [ expr { %d + $nsizeM / 2 } ] [ expr { %d + $vmarginM + $nsizeM / 2 } ] -fill $colorsTheme(%s) -outline $colorsTheme(dfg) -tags %s", x, y, x, y, sel ? "hc" : "isbg", str );
}


/****************************************************
PUT_LINE
****************************************************/
void put_line( int x1, int y1, int x2 )
{
	cmd( ".str.f.c create line %d [ expr { round ( %d - $vstepM * $vfactM + $vmarginM + $nsizeM / 2 ) } ] %d [ expr { round ( %d + $nsizeM / 2 ) } ] -fill $colorsTheme(dfg)", x1, y1, x2, y1 );
}


/****************************************************
PUT_TEXT
****************************************************/
void put_text( const char *str, const char *n, int x, int y, const char *str2 )
{
	cmd( ".str.f.c create text %d %d -text \"%s\" -fill $colorsTheme(hl) -tags %s", x, y - 1, str, str2 );

	// text for node numerosity (handle single "1" differently to displace from line)
	if ( ! strcmp( n, "1" ) )
		cmd( ".str.f.c create text [ expr { %d - 2 } ] [ expr { %d + 2 * $vmarginM + 1 } ] -text \"%s\" -fill $colorsTheme(fg) -tags %s", x, y, n, str2 );
	else
		cmd( ".str.f.c create text %d [ expr { %d + 2 * $vmarginM + 1 } ] -text \"%s\" -fill $colorsTheme(fg) -tags %s", x, y, n, str2 );

	cmd( ".str.f.c bind %s <Enter> { \
			set res_g %s; \
			if { [ info exists res_g_id ] } { \
				after cancel $res_g_id \
			}; \
			destroy .list; \
			set res_g_id [ after $ttipdelay { \
				destroy .list; \
				tooltip::hide; \
				toplevel .list -class Tooltip -background $colorsTheme(ttip) -borderwidth 0 -highlightthickness 1 -highlightbackground $colorsTheme(fg); \
				if { $CurPlatform eq \"mac\" } { \
					tk::unsupported::MacWindowStyle style .list help none \
				} else { \
					wm overrideredirect .list 1 \
				}; \
				catch { wm attributes .list -topmost 1 }; \
				catch { wm attributes .list -alpha 0.99 }; \
				wm positionfrom .list program; \
				wm withdraw .list; \
				label .list.t -text \"%s (#%s)\" -font \"$ttfontB\" -foreground $colorsTheme(obj) -background $colorsTheme(ttip); \
				pack .list.t -anchor w -ipadx 1; \
				set res_g_i 0; \
				if { [ llength $tlist_%s ] > 0 } { \
					foreach res_g_t $tlist_%s res_g_s $slist_%s { \
						label .list.e$res_g_i -text \"$res_g_t\" -font \"$ttfont\" -foreground $colorsTheme($res_g_s) -background $colorsTheme(ttip); \
						pack .list.e$res_g_i -anchor w -ipadx 1; \
						incr res_g_i \
					} \
				}; \
				update idletasks; \
				if { $CurPlatform eq \"mac\" } { \
					set __focus__ [ focus ] \
				}; \
				wm geometry .list +[ expr { %%X + 5 } ]+[ expr { %%Y + 5 } ]; \
				wm deiconify .list; \
				catch { raise .list }; \
				if { $CurPlatform eq \"mac\" } { \
					after idle { catch { focus -force $__focus__ } } \
				} \
			} ] \
		}", str2, str2, str, n, str2, str2, str2 );

	cmd( ".str.f.c bind %s <Leave> { \
			if { [ info exists res_g_id ] } { \
				after cancel $res_g_id \
			}; \
			unset -nocomplain res_g res_g_id; \
			destroy .list \
		}", str2 );
}
