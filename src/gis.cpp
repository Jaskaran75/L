/*************************************************************

  LSD 7.1 - December 2018
  written by Marco Valente, Universita' dell'Aquila
  and by Marcelo Pereira, University of Campinas

  This file: Frederik Schaff, Ruhr-University Bochum

  This file: Frederik Schaff, Ruhr-University Bochum
  LSD is distributed under the GNU General Public License

 *************************************************************/

/****************************************************************************************
    GIS.CPP
  GIS tools: Advancing LSD to represent a GIS structure, where objects can be
  located in 2d continuous space defined by the matrix [0,xn)*[0,yn).

  All functions work on specially defined LSD object's data structures (named here as
    "gisPosition" and "gisMap"), with the following organization:


    object --+-- gisPosition  --+- x (double) : position in x direction
                                +- y (double) : position in y direction
                                +- z (double) : position in z direction (if any, not yet used)
                                +- map (ptr)  : pointer to gisMap
                                +- objDis_inRadius  : container used to store temporary elements for radius search
                                +- it_obj : iterator for the objDis_inRadius container. Used for the Cycles.


    The gisMap holds pointers to the elements associated with it. The pointers are
    stored in a 2d container "elements". Each element in the container is
    associated with a position in the grid, going from (0,0) to (xn-1,yn-1).
    For example, an object that has x=0.5 and y=1.9 would be 'registered' at
    position (0,1). This allows to use efficient spatial search algorithms.

    gisMap --+- xn (integer) : x-dimension boundary
             +- yn (integer) : y-dimension boundary
             +- wrap --+- wrap_left : wrapping to left allowed
                       +- wrap_right
                       +- wrap_top
                       +- wrap_bottom
             +- elements (stl container, random access, xn * yn): contains pointers to all elements in the map
             +- nelements (integer) : number of total elements registered in the map

    For explanations of the single methods, please refer to the comments that
    precede the method.
****************************************************************************************/
#include "decl.h"

#if defined( CPP11 )

char gismsg[300];

void object::set_distance_type( double type )
{
  return set_distance_type(int(type));
}

void object::set_distance_type( int type )
{
  if (type != 0 && type != 1 && type != 2 ) {
    sprintf( gismsg, "failure in set_distance_type() for distance type '%c'", type );
    error_hard( gismsg, "this is not a valid type",
                "Change to a valid type (0 / e, 1 / m or 2 / c)" );
    return;
  }
  
  if (type == 0)
  { return set_distance_type('e'); }
  else if (type == 1)
  { return set_distance_type('m'); }
  else if (type == 2)
  { return set_distance_type('c'); }
}

void object::set_distance_type( char const type[] )
{
  return set_distance_type(type[0]);
}



void object::set_distance_type( char type )
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in set_distance_type() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return;
  }
  
  if (type != 'e' && type != 'm' && type != 'c' &&
      type != 'E' && type != 'M' && type != 'C') {
    sprintf( gismsg, "failure in set_distance_type() for distance type '%c'", type );
    error_hard( gismsg, "this is not a valid type",
                "Change to a valid type (Chebyshev, Euclidean or Manhattan)" );
    return;
  }
  
  position->map->distance_type = type;
  if (this == root){ //only print for root.
    sprintf( gismsg, "\nSwitched distance for root to type '%c' (c: Chebyshev, e: Euclidean, m: Manhattan)", type);
    plog(gismsg);
  }
  return;
}

//  init_gis_singleObj
//  Initialise the gis and add the single object to it
bool object::init_gis_singleObj(double _x, double _y, int xn, int yn, int _wrap)
{
  if ( ptr_map() != NULL ) {
    sprintf( gismsg, "failure in init_gis_singleObj() for object '%s'", label );
    error_hard( gismsg, "the object was already part of a GIS",
                "check your code to prevent this situation" );
    return false; //there is already a gis structure
  }
  
  gisMap* map = init_map(xn, yn, _wrap); //create gis
  return register_at_map(map, _x, _y); //register the object at the gis
}

//  init_gis_regularGrid
//  Initialise a regular Grid GIS and link the objects of the same type to it.
//  The gis objects need to be contained in the calling object
bool object::init_gis_regularGrid(char const lab[], int xn, int yn, int _wrap, int t_update, int n)
{
  object* firstNode;
  object* cur;
  
  if ( 0 == strcmp(label, lab) ) {
    firstNode = up->search( lab );
  }
  else {
    firstNode = search( lab );
  }
  
  if ( firstNode == NULL ) {
    sprintf( gismsg, "failure in init_gis_regularGrid() for object '%s'", label );
    error_hard( gismsg, "No object present below callee object",
                "check your code to prevent this situation" );
    return false; //error
  }
  
  if ( firstNode->ptr_map() != NULL ) {
    sprintf( gismsg, "failure in init_gis_regularGrid() for object '%s'", lab );
    error_hard( gismsg, "the object was already part of a GIS",
                "check your code to prevent this situation" );
    return false; //there is already a gis structure
  }
  
  gisMap* map = init_map(xn, yn, _wrap); //create gis
  
  int numNodes = xn * yn;
  
  if (n > 0) //otherwise we create a complete lattice, the default
  { numNodes = min(n, numNodes); }
  
  add_n_objects2( lab, nodes2create( this, lab, numNodes ), t_update ); // creates the missing node objects,
  
  //switch between sparse and complete space
  if (numNodes < xn * yn) {
    std::vector< std::pair< double, std::pair< int, int > > > random_pos;
    random_pos.reserve(xn * yn);
    
    for (int x = 0; x < xn; ++x) {
      for (int y = 0; y < yn; ++y) {
        random_pos.emplace_back(RND, std::make_pair(x, y));
      }
    }
    
    std::sort( random_pos.begin(),  random_pos.end(), [](auto const & A, auto const & B ) {
      return A.first < B.first;
    } ); //sort only by RND
    //add to positions
    firstNode = search( lab );
    int i = 0;
    
    for ( cur = firstNode; cur != NULL && i < numNodes; cur = go_brother( cur ), ++i ) {
      if (cur->register_at_map(map, random_pos.at(i).second.first, random_pos.at(i).second.second) == false) {            // scan all nodes aplying ID numbers
        return false; //error!
      }
    }
    
  }
  else {   //complete map
    int _x = 0;
    int _y = 0;
    firstNode = search( lab );
    
    for ( cur = firstNode; cur != NULL; cur = go_brother( cur ) ) {
      if (cur->register_at_map(map, _x, _y) == false) {           // scan all nodes aplying ID numbers
        return false; //error!
      }
      
      ++_y;
      
      if (_y == yn) {
        _y = 0;
        ++_x;
      }
    }
    
    if (_x != xn || _y != 0) {
      sprintf( gismsg, "failure in init_gis_regularGrid() for object '%s'", label );
      error_hard( gismsg, "check the implementation",
                  "please contact the developer" );
      return false; //error!
    }
    else {
      return true;
    }
  }
  
}

//  ptr_map
//  Check if the object is a gis object and return pointer to map.
gisMap* object::ptr_map()
{
  if (position == NULL) {
    return NULL;
  }
  else {
    return position->map;
  }
}

//  init_map
//  create a new map and return the pointer
//  Caution: Only use in combo with creating a gis object
gisMap* object::init_map(int xn, int yn, int _wrap)
{
  gisMap* map = new gisMap(xn, yn, _wrap);
  
  if ( map == NULL ) {
    error_hard( "cannot allocate memory for init_map()",
                "out of memory" );
    return NULL;
  }
  
  return map;
}

//  delete_map
//  delete the gis data structure, resetting all the objects.
bool object::delete_map()
{
  gisMap* map = ptr_map();
  
  if (map == NULL ) {
    sprintf( gismsg, "failure in delete_map() for object '%s'", label );
    error_hard( gismsg, "the object was not part of a gis",
                "check your code to prevent this situation" );
    return false; //not part of a gis
  }
  
  //temporary vector with all items, if any
  std::deque<object*> temp_list;
  
  for (auto itemx : map->elements) {
    for (auto itemxy : itemx) {
      for (object* obj_ptr : itemxy) {
        temp_list.push_back(obj_ptr);
      }
    }
  }
  
  for (object* item : temp_list) {
    if (item->unregister_from_gis() == false) { //the last unregister deletes the map
      return false;
    }
  }
  
  return true;
}

//  unregister_from_gis
//  unregister the object from the space
bool object::unregister_from_gis()
{
  if (  unregister_position( false ) == false) {
    sprintf( gismsg, "failure in unregister_from_gis() for object '%s'", label );
    error_hard( gismsg, "not connected to a gis",
                "check your code to prevent this situation" );
    return false; //error
  }
  
  delete position;
  position = NULL;
  return true;
}

//  register_at_map_between
bool object::register_at_map_between(object* shareObj, object* shareObj2)
{
  if (shareObj -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in register_at_map_between() for object '%s' to be positioned between object %s and %s", label, shareObj->label, shareObj2->label );
    error_hard( gismsg, "the destination object (1) is not registered in any space",
                "likely, the shareObj provided is not registered in any space. Check your code to prevent this situation" );
    return false;
  }
  
  if (shareObj2 -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in register_at_map_between() for object '%s' to be positioned between object %s and %s", label, shareObj->label, shareObj2->label );
    error_hard( gismsg, "the destination object (2) is not registered in any space",
                "likely, the shareObj provided is not registered in any space. Check your code to prevent this situation" );
    return false;
  }
  
  return register_at_map_between(shareObj -> position -> map, shareObj -> position -> x, shareObj -> position -> y, shareObj2 -> position -> x, shareObj2 -> position -> y );
}
//  register_at_map_between
//  register the object at the map, using x and y positions
bool object::register_at_map_between(gisMap* map, double _x, double _y, double _x2, double _y2)
{
  if (map == NULL) {
    sprintf( gismsg, "failure in register_at_map() for object '%s'", label );
    error_hard( gismsg, "no map to connect to provided",
                "check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  if ( ptr_map() != NULL ) { //already registered?!
    if ( ptr_map() != map ) {
      sprintf( gismsg, "failure in register_at_map() for object '%s'", label );
      error_hard( gismsg, "already registered in a space different from the one provided",
                  "check your code to prevent this situation. If you want to change the space, deregister elemet from old one first." );
      return false; //re-registering not allowed. derigster at gis first."
    }
  }
  
  double x_new, y_new;
  position_between(map, x_new, y_new, _x, _y, _x2, _y2); //Check for wrapping
  
  if ( ptr_map() == map ) {
    plog("\nInfo (t=%i): In register_at_map() the item %s is already part of the space.", "", t, label);
    plog("It will be moved from pos (%g,%g) to pos (%g,%g) instead.", "", position->x, position->y, x_new, y_new);
    return change_position(x_new, y_new);
  }
  
  position = new gisPosition(map, x_new, y_new);
  
  if ( position == NULL ) {
    error_hard( "cannot allocate memory for register_at_map()",
                "out of memory");
    return false;
  }
  
  register_position(y_new, y_new);
  return true;
}

//position_between - wrapper
void object::position_between(double& x_out, double& y_out, object* shareObj, object* shareObj2, double rel_pos)
{
  if (shareObj -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in position_between() with object %s and %s", shareObj->label, shareObj2->label );
    error_hard( gismsg, "the destination object (1) is not registered in any space",
                "Check your code to prevent this situation" );
    return;
  }
  
  if (shareObj2 -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in position_between() with object %s and %s", shareObj->label, shareObj2->label );
    error_hard( gismsg, "the destination object (2) is not registered in any space",
                "Check your code to prevent this situation" );
    return;
  }
  
  if (shareObj -> ptr_map() != shareObj2 -> ptr_map() ) {
    sprintf( gismsg, "failure in position_between() with object %s and %s", shareObj->label, shareObj2->label );
    error_hard( gismsg, "the destination object (1) is not registered in the same space as obj (2)",
                "Check your code to prevent this situation" );
    return;
  }
  
  position_between(shareObj->ptr_map(), x_out, y_out, shareObj->position->x, shareObj->position->y, shareObj2->position->x, shareObj2->position->y, rel_pos);
}
//position_between - wrapper
void object::position_between(double& x_out, double& y_out, double x_1, double y_1, double x_2, double y_2, double rel_pos)
{
  if (this->ptr_map() == NULL ) {
    sprintf( gismsg, "failure in position_between() with object %s", this->label);
    error_hard( gismsg, "the object is not registered in any space",
                "Check your code to prevent this situation" );
    return;
  }
  
  position_between(this->ptr_map(), x_out, y_out, x_1, y_1, x_2, y_2, rel_pos);
}

//position_between
//find position at half distance
//or, at relative distance to x_1,y_1 of 'rel_pos', where a rel_pos of 1 indicates the position is at x_2,y_2 and 1/2 it is at the half distance
//a negative distance defaults to the half-position
//as usual, wrapping is taken int o account, direction is not!
void object::position_between(gisMap* map, double& x_out, double& y_out, double x_1, double y_1, double x_2, double y_2, double rel_pos)
{
  //similar to pseudo_distance
  double x_n = map->xn;
  double y_n = map->yn;
  
  //default behaviour
  if (rel_pos < 0.0) {
    rel_pos = 0.5;
  }
  else if (rel_pos > 1.0 ) {
    error_hard( "failure in position_between()", "the rel_pos needs to be in (0,1)",
                "Fix your code to prevent this case" );
    return;
  }
  
  //we split the movement in movement in x direction and y direction and make sure that it is positive.
  double rel_pos_x = rel_pos;
  double rel_pos_y = rel_pos;
  
  if (x_1 > x_2) {
    std::swap(x_1, x_2);
    rel_pos_x = 1.0 - rel_pos;
  }
  
  if (y_1 > y_2) {
    std::swap(y_1, y_2);
    rel_pos_y = 1.0 - rel_pos;
  }
  
  
  double x_dist = x_2 - x_1;
  double y_dist = y_2 - y_1;
  
  //we need to take care of wrapping. Only if wrapping on both sides of a
  //direction (left & right, up & down) is allowed, we consider an option in between.
  //otherwise between will always be inside the two original positions.
  
  // no wrapping is default.
  
  
  if (map->wrap.noWrap == false) {
    if (map->wrap.right != map->wrap.left) {
      error_hard( "failure in position_between()", "the wrapping on the x axis is mixed",
                  "Usage of position_between() is not allowed in this case." );
      return;
    }
    else if ( true == map->wrap.right) {
      double x_alt_dist = x_1 + (x_n - x_2);
      
      if ( x_alt_dist < x_dist ) {
        x_dist = -x_alt_dist; //move in opposite direction, using wrapping
      }
    }
    
    if (map->wrap.top != map->wrap.bottom) {
      error_hard( "failure in position_between()", "the wrapping on the y axis is mixed",
                  "Usage of position_between() is not allowed in this case." );
      return;
    }
    else if ( true == map->wrap.top) {
      double y_alt_dist = y_1 + (y_n - y_2);
      
      if ( y_alt_dist < y_dist ) {
        y_dist = -y_alt_dist; //move in opposite direction, using wrapping
      }
    }
  }
  
  x_out = x_1 + ( x_dist * rel_pos );
  y_out = y_1 + ( y_dist * rel_pos );
  
  if (map->wrap.noWrap == false) {
    check_positions(map, x_out, y_out); //adjust new positions to wrapping, if necessary.
  }
  else if (false == check_positions(map, x_out, y_out, true)) { //just check positions
    sprintf( gismsg, "failure in %s. In p1(%g,%g), p2(%g,%g), ratio %g. Out (%g,%g).", __func__, x_1, y_1, x_2, y_2, rel_pos, x_out, y_out);
    error_hard( gismsg, "this should not happen",
                "contact the developer" );
  }
  
}

//  register_at_map
//  register the object at the map, using the same position as share_obj
bool object::register_at_map(object* shareObj)
{
  if (shareObj -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in register_at_map() for object '%s' at position of object %s", label, shareObj->label );
    error_hard( gismsg, "the destination object is not registered in any space",
                "likely, the shareObj provided is not registered in any space. Perhaps switch target obj and gis obj? Check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  return register_at_map(shareObj -> position -> map, shareObj -> position -> x, shareObj -> position -> y );
}

//  register_allOfKind_at_grid_rnd
//  Register a number of objects at the map, using random positions
//  at the grid, but not two at the same position.
void object::register_allOfKind_at_grid_rnd(object* obj)
{
  register_allOfKind_at_grid_rnd_cnd(obj, "", "", 0.0);
}

//Conditional Version.
void object::register_allOfKind_at_grid_rnd_cnd(object* obj, char const varLab[], char const condition[], double condVal)
{
  if (ptr_map() == NULL ) {
    sprintf( gismsg, "failure in register_allOfKind_at_grid_rnd() for objects '%s' at space shared with gis object %s", obj->label, label );
    error_hard( gismsg, "the gisObj object is not registered in any space",
                "likely, the gisObj provided is not registered in any space. Perhaps switch target obj and gis obj? Check your code to prevent this situation" );
    return;
  }
  
  object* cur = obj->up->search(obj->label); //make sure we start with the first one
  
  //if we have a condition active, we create a black-list of positions.
  std::vector<std::pair<int, int> > blacklist;
  bool is_conditional = !( 0 == strcmp(varLab, "") );
  
  if ( is_conditional ) {
    object* CondOb = cur->search_var(cur, varLab, true, false)->up; //first related object
    
    if (CondOb == NULL) {
      sprintf( gismsg, "failure in register_allOfKind_at_grid_rnd_cnd() for conditional variable %s", varLab );
      error_hard( gismsg, "the conditional object holding this variable cannot be found.",
                  "Check your code to prevent this situation" );
    }
    
    
    
    if (CondOb->ptr_map() == NULL) {
      sprintf( gismsg, "failure in register_allOfKind_at_grid_rnd_cnd() for conditional objects '%s' holding conditiona variable %s", CondOb->label, varLab );
      error_hard( gismsg, "the conditional object is not registered in any space",
                  "Check your code to prevent this situation" );
      return;
    }
    
    if (CondOb->ptr_map() != ptr_map()) {
      sprintf( gismsg, "failure in register_allOfKind_at_grid_rnd_cnd() for conditional objects '%s' holding conditiona variable %s", CondOb->label, varLab );
      error_hard( gismsg, "the conditional object is not registered in the same space as the target objects",
                  "Check your code to prevent this situation" );
      return;
    }
    
    //add all for which the condition is not true to the blacklist.
    while (CondOb != NULL) {
      if ( false == CondOb->check_condition(varLab, condition, condVal) ) {
        blacklist.emplace_back(int(CondOb->get_pos('x')), int(CondOb->get_pos('y')));
        // sprintf( gismsg, "\nAdding pos %i,%i to blacklist",blacklist.back().first,blacklist.back().second);
        // plog(gismsg);
      }
      
      CondOb = CondOb->next; //only use next, i.e. the objects must have the same parent.
    }
  }
  
  
  //Create random sorted list of all grid points
  int xn = position -> map->xn;
  int yn = position -> map->yn;
  std::vector< std::tuple< double, int, int > > positions;
  positions.reserve(xn * yn);
  
  for (int x = 0; x < xn; ++x) {
    for (int y = 0; y < yn; ++y) {
      if ( !is_conditional
           ||  ( is_conditional
                 && std::find(blacklist.begin(), blacklist.end(), std::pair<int, int>(x, y) ) == blacklist.end() )
         ) {
        positions.emplace_back(RND, x, y);
      }
      
      // else {
      // sprintf( gismsg, "\nExcluding option %i, %i", x, y);
      // plog(gismsg);
      // }
    }
  }
  
  std::sort(positions.begin(), positions.end());
  int max_elements = positions.size();
  //Cycle through all objects in the linked list and at them to random places
  
  for (auto const& pos : positions) {
    if (cur == NULL)
    { break; }
    
    if (max_elements == 0) {
      sprintf( gismsg, "failure in register_allOfKind_at_grid_rnd_cnd(): %i is not enough free space to host all elements of type %s", positions.size(), label );
      error_hard( gismsg, "the conditional object is not registered in any space",
                  "Check your code to prevent this situation" );
      return;
    }
    
    cur->register_at_map(position -> map, std::get<1>(pos), std::get<2>(pos) );
    cur = cur->next;
  }
}


//  register_at_map_rnd
//  register the object at the map, using random positions.
//  flag if only gridded
bool object::register_at_map_rnd(object* gisObj, bool snap_grid)
{
  if (gisObj -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in register_at_map_rnd() for object '%s' at position of object %s", label, gisObj->label );
    error_hard( gismsg, "the gisObj object is not registered in any space",
                "likely, the gisObj provided is not registered in any space. Perhaps switch target obj and gis obj? Check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  double x = 0;
  double y = 0;
  
  if (false == snap_grid) {
    do {
      x = uniform (0, gisObj -> position -> map->xn);
    } while (x == gisObj -> position -> map->xn); //prevent boarder case
    
    do {
      y = uniform (0, gisObj -> position -> map->yn);
    } while (x == gisObj -> position -> map->yn); //prevent boarder case
  }
  else {
    x = uniform_int (0, gisObj -> position -> map->xn - 1);
    y = uniform_int (0, gisObj -> position -> map->yn - 1);
  }
  
  return register_at_map(gisObj -> position -> map, x, y );
}

//  register_at_map
//  register the object at the map, using x and y positions
bool object::register_at_map(object* where, double _x, double _y, int lattice_color, int lattice_priority)
{
  return register_at_map(where->ptr_map(), _x, _y, lattice_color, lattice_priority);
}

bool object::register_at_map(gisMap* map, double _x, double _y, int lattice_color, int lattice_priority)
{
  if (map == NULL) {
    sprintf( gismsg, "failure in register_at_map() for object '%s'", label );
    error_hard( gismsg, "no map to connect to provided",
                "check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  if ( ptr_map() != NULL ) { //already registered?!
    if ( ptr_map() != map ) {
      sprintf( gismsg, "failure in register_at_map() for object '%s'", label );
      error_hard( gismsg, "already registered in a space different from the one provided",
                  "check your code to prevent this situation. If you want to change the space, deregister elemet from old one first." );
      return false; //re-registering not allowed. derigster at gis first."
    }
    else {
      plog("\nInfo (t=%i): In register_at_map() the item %s is already part of the space.", "", t, label);
      plog("It will be moved from pos (%g,%g) to pos (%g,%g) instead.", "", position->x, position->y, _x, _y);
      return change_position(_x, _y);
    }
  }
  
  check_positions(map, _x, _y, false); //adjust position if possible.
  position = new gisPosition(map, _x, _y, lattice_color, lattice_priority);
  
  if ( position == NULL ) {
    error_hard( "cannot allocate memory for register_at_map()",
                "out of memory");
    return false;
  }
  
  register_position(_x, _y);
  return true;
}

//  register_position
//  register the object in the already associated gis at a new position.
bool object::register_position(double _x, double _y)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in register_position() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return false;
  }
  
  if (_x < 0.0 || _x >= double(position->map->xn) || _y < 0.0 || _y >= double(position->map->yn) ) {
    sprintf( gismsg, "failure in register_position() for object '%s' position (%g,%g)", label, _x, _y );
    error_hard( gismsg, "the position is outside the map",
                "check your code to prevent this situation" );
    return false; //error!
  }
  
  position->x = _x;
  position->y = _y;
  int x = position->x;
  int y = position->y;
  position->map->elements.at(x).at(y).push_back( this );
  position->map->nelements++;
#ifndef NO_WINDOW
  
  if (position->map->has_lattice) {
    update_lattice_gis(x, y);
  }
  
#endif
  return true;
}

// unregister_position
// unregister the object at the position.
// when move is false, the map is deleted if it is the last element in the map.
// when has_lattice, the lattice is updated to the colour of the last element alive or the init default.
bool object::unregister_position(bool move)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in unregister_position() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return false;
  }
  
  //https://stackoverflow.com/a/31329841/3895476
  int x = (int) position->x;
  int y = (int) position->y;
  auto begin = position->map->elements.at(x).at(y).begin();
  auto end   = position->map->elements.at(x).at(y).end();
  bool is_removed = false;
  
  for (auto it_item =  begin;  it_item != end; it_item++) {
    if (*it_item == this ) {
      position->map->elements.at(x).at(y).erase(it_item);
      position->map->nelements--;
#ifndef NO_WINDOW
      
      if (position->map->has_lattice) {
        update_lattice_gis(x, y);
      }
      
#endif
      
      if (move == false && position->map->nelements == 0) { //important: If unregistering before move, do not delete gis map
        //Last element removed
#ifndef NO_WINDOW
        if (position->map->has_lattice) {
          close_lattice_gis();
        }
        
#endif
        delete position->map;
        position->map = NULL;
      }
      
      return true;
    }
  }
  
  //Default
  sprintf( gismsg, "failure in unregister_position() for object '%s'", label );
  error_hard( gismsg, "the object is not registered in the map connected to",
              "check your code to prevent this situation" );
  return false;
}

//change_position
//change the objects position to the position of shareObj
bool object::change_position(object* shareObj)
{
  if (shareObj -> ptr_map() == NULL ) {
    sprintf( gismsg, "failure in change_position() for object '%s' at position of object %s", label, shareObj->label );
    error_hard( gismsg, "the destination object is not registered in any space",
                "likely, the shareObj provided is not registered in any space. Check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in change_position() for object '%s'", label );
    error_hard( gismsg, "the source object is not registered in any space",
                "check your code to prevent this situation" );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  if (shareObj -> ptr_map() != ptr_map() ) {
    sprintf( gismsg, "failure in change_position() for object '%s' at position of object %s", label, shareObj->label );
    error_hard( gismsg, "the destination object is not registered at the same space as the target object",
                "check your code to prevent this situation. If you want to use positions from one space in another, use explicit approach via x,y coordinates." );
    return false; //re-registering not allowed. derigster at gis first."
  }
  
  return change_position(shareObj->position->x, shareObj->position->y);
}

//  change_position
//  change_position to the new position x y
bool object::change_position(double _x, double _y, bool noAdjust) //def: noAdjust=false / do adjust
{
  if (check_positions(_x, _y, noAdjust) == false) {
    return false; //Out of range
  }
  
  if (unregister_position(true) == false) {
    return false; //cannot change position, error in init position
  }
  
  return register_position( _x,  _y);
}

//  pseudo_distance
//  Calculate the pseudo (squared) distance between an object p and another object b.
double object::pseudo_distance(object* b, bool noWrap)
{
  //all checks in distance()
  return pseudo_distance( b->position->x, b->position->y, noWrap );
}


//  pseudo_distance
//  Calculate the pseudo (squared) distance between an object p and another object b.
double object::pseudo_distance(double x_2, double y_2, bool noWrap)
{
  return pseudo_distance(position->x, position->y, x_2, y_2, noWrap);
}

//  pseudo_distance
//pseudo distance between to point in plain
double object::pseudo_distance(double x_1, double y_1, double x_2, double y_2, bool noWrap)
{
  double xn = position->map->xn;
  double yn = position->map->yn;
  
  char distance_type = position->map->distance_type;
  
  double x_dist = x_1 - x_2;
  double y_dist = y_1 - y_2;
  
  if (!noWrap && position->map->wrap.noWrap == false) {
    if (position->map->wrap.right && x_1 > x_2) {
      double alt_x_dist = xn - x_1 + x_2;
      
      if (alt_x_dist < x_dist) {
        x_dist = alt_x_dist;
      }
    }
    else if (position->map->wrap.left) {
      double alt_x_dist = xn - x_2 + x_1;
      
      if (alt_x_dist < -x_dist) {
        x_dist = alt_x_dist;
      }
    }
    
    if (position->map->wrap.top && y_1 > y_2) {
      double alt_y_dist = yn - y_1 + y_2;
      
      if (alt_y_dist < y_dist) {
        y_dist = alt_y_dist;
      }
    }
    else if (position->map->wrap.bottom) {
      double alt_y_dist = yn - y_2 + y_1;
      
      if (alt_y_dist < -y_dist) {
        y_dist = alt_y_dist;
      }
    }
  }
  
  switch (distance_type) {
    case 'e' : //Euclidean
    case 'E' :
      return x_dist * x_dist + y_dist * y_dist;
      
    case 'm' : //Manhattan
    case 'M' :
      return abs(x_dist) + abs(y_dist);
      
    case 'c' : //Chebyshev
    case 'C' :
      return min(abs(x_dist), abs(y_dist));
  }
}

bool object::boundingBox(int& left_io, int& right_io, int& top_io, int& bottom_io, double radius)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in boundingBox() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }
  
#endif
  return boundingBox(position->x, position->y, left_io, right_io, top_io, bottom_io, radius);
}

//Calculate the bounding box points.
//Takes care of wrapping
bool object::boundingBox(double x, double y, int& left_io, int& right_io, int& top_io, int& bottom_io, double radius)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in boundingBox() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }
  
#endif
  
  if (radius < 0) {
    left_io = 0;
    right_io = position->map->xn - 1;
    top_io = position->map->yn - 1;
    bottom_io = 0;
  }
  
  //define the bounding box
  left_io   = floor( x - radius );
  right_io  = ceil(  x + radius );
  top_io    = ceil(  y + radius );
  bottom_io = floor( y - radius );
  
  //adjust box for wrapping
  if (position->map->wrap.left == false)
  { left_io = max(0, left_io); }
  
  if (position->map->wrap.right == false)
  { right_io = min(position->map->xn, right_io); }
  
  if (position->map->wrap.top == false)
  { top_io = min(position->map->yn, top_io); }
  
  if (position->map->wrap.bottom == false)
  { bottom_io = max(0, bottom_io); }
  
  return true;
}

//  traverse_boundingBox
//  a function that receives all objects inside the bounding box of the object
//  at x,y with radius and performs whatever the provided funtion do_stuff tells
bool object::traverse_boundingBox(double radius, std::function<bool(object* use_obj)> do_stuff )
{
  //define the bounding box
  int x_left, x_right, y_top, y_bottom;
  
  if (boundingBox(x_left, x_right, y_top, y_bottom, radius) == false) {
    return false; //Error gismsg in boundingBox
  }
  
  for (int x = x_left; x <= x_right; x++) {
    for (int y = y_bottom; y <= y_top; y++) {
      access_GridPosElements(x, y, do_stuff); //do nothing if rvalue is false/wrong
    }
  }
  
  return true; //went to end without any break;
}

//  access_GridPosElements
//  Access all elements registered at the position x,y and use the function on them
bool object::access_GridPosElements (int x, int y, std::function<bool(object* use_obj)> do_stuff)
{
  //control for wrapping and adjust if necessary
  double x_test = x;
  double y_test = y;
  
  if (check_positions(x_test, y_test) == false ) {
    return false; //invalid position
  }
  
  //Cycle through all the potential ellements
  for (object* candidate : position->map->elements.at(int(x_test)).at(int(y_test)) ) {
    do_stuff(candidate); //do not use rvalue (true/false)
  }
  
  return true;
}

//  traverse_boundingBoxBelt
//  a function that receives all objects inside the belt of the bounding box
//  (i.e., only the right/left columns and top/bottom rows of the grid)
//  at x,y with radius and performs whatever the provided funtion do_stuff tells
bool object::traverse_boundingBoxBelt(double radius, std::function<bool(object* use_obj)> do_stuff )
{
  //define the bounding box
  int x_left, x_right, y_top, y_bottom;
  
  if (boundingBox(x_left, x_right, y_top, y_bottom, radius) == false) {
    return false; //Error gismsg in boundingBox
  }
  
  //left column
  for (int x = x_left, y = y_top; y >= y_bottom; y--) {
    access_GridPosElements(x, y, do_stuff); //do nothing if rvalue is false/wrong
  }
  
  //right column
  for (int x = x_right, y = y_top; y >= y_bottom; y--) {
    access_GridPosElements(x, y, do_stuff); //do nothing if rvalue is false/wrong
  }
  
  //top row w/o left/right element
  for (int y = y_top, x = x_left + 1; x < x_right; x++) {
    access_GridPosElements(x, y, do_stuff); //do nothing if rvalue is false/wrong
  }
  
  //bottom row w/o left/right element
  for (int y = y_bottom, x = x_left + 1; x < x_right; x++) {
    access_GridPosElements(x, y, do_stuff); //do nothing if rvalue is false/wrong
  }
  
  return true; //went to end without any break;
}

// distance
// Calculate the distance between to objects in the same gis.
double object::distance(object* b, bool noWrap)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in distance() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  if (b->ptr_map() == NULL) {
    sprintf( gismsg, "failure in distance() for second object '%s'", b->label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  if (b->ptr_map() != ptr_map()) {
    //both elements need to be part of the same gis
    sprintf( gismsg, "failure in distance() for objects '%s' and '%s'", label, b->label );
    error_hard( gismsg, "the objects are not on the same map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  char distance_type = position->map->distance_type;
  
  switch (distance_type) {
    case 'e' : //Euclidean
    case 'E' :
      return sqrt( pseudo_distance(b, noWrap) );
      
    case 'm' : //Manhattan
    case 'M' :
    case 'c' : //Chebyshev
    case 'C' :
      return pseudo_distance(b, noWrap);  //pseudo and distance are equivalent
  }
}

double object::distance(double x, double y, bool noWrap)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in distance() (x,y) for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  char distance_type = position->map->distance_type;
  
  switch (distance_type) {
    case 'e' : //Euclidean
    case 'E' :
      return sqrt( pseudo_distance( x, y ) );
      
    case 'm' : //Manhattan
    case 'M' :
    case 'c' : //Chebyshev
    case 'C' :
      return pseudo_distance( x, y );  //pseudo and distance are equivalent
  }
}

double object::distance(double x_1, double y_1, double x_2, double y_2, bool noWrap)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in distance() (x,y) for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  char distance_type = position->map->distance_type;
  
  switch (distance_type) {
    case 'e' : //Euclidean
    case 'E' :
      return sqrt( pseudo_distance( x_1, y_1, x_2, y_2 ) );
      
    case 'm' : //Manhattan
    case 'M' :
    case 'c' : //Chebyshev
    case 'C' :
      return pseudo_distance( x_1, y_1, x_2, y_2 );  //pseudo and distance are equivalent
  }
}

//calulate the center positions.
double object::center_position(char xy)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in center('%c') for object '%s'", xy, label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  switch (xy) {
    case 'x':
    case 'X':
      return position->map->center_x;
      
    case 'y':
    case 'Y':
      return position->map->center_y;
  }
}

double object::max_distance()
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in max_distance() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  double max_distance = position->map->max_distance; //reference - change in map
  
  if ( max_distance < 0) {
    //initialise
    if (true == position->map->wrap.noWrap) {
      max_distance = distance(0, 0, position->map->xn, position->map->yn);
    }
    else {
      double op0 = distance(position->map->center_x, position->map->center_y, position->map->xn, position->map->yn); //default for full wrapping
      double opA = distance(0, 0, position->map->xn, position->map->yn); //left bottom
      double opB = distance(position->map->xn, position->map->yn, 0, 0); //right top
      double opC = distance(0, position->map->yn, position->map->xn, 0); //left top
      double opD = distance(position->map->xn, 0, 0, position->map->yn); //right bottom
      
      max_distance = max(op0, max(opA, max(opB, max(opC, opD))));
    }
    
    position->map->max_distance = max_distance;
  }
  
  return max_distance;
}

double object::relative_distance(double abs_distance)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in relative_distance() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  return abs_distance / max_distance();
}

double object::absolute_distance(double rel_distance)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in absolute_distance() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return NaN;
  }
  
  return rel_distance * max_distance();
}

char object::read_distance_type()
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in read_distance_type() for object '%s'", label );
    error_hard( gismsg, "the object is not yet connected to a map",
                "check your code to prevent this situation" );
    return '0';
  }
  
  return ptr_map()->distance_type;
}

//  add_if_dist_lab_cond
//  in addition to add_if_dist_lab checks if VAR CONDITION condVAL is true
//  VAR is a variable contained in the object lab.
//  if operator() is called with negative pseudo_radius, it will ignore the distance check
struct add_if_dist_lab_cond {
  object* this_obj; //object for which the search is conducted
  double radius;
  char lab[ MAX_ELEM_LENGTH ];
  object* fake_caller; //can be a fake-caller if not NULL.
  int lag;
  char varLab[ MAX_ELEM_LENGTH ];
  bool has_condition;
  double condVal;
  double pseudo_radius;
  char condition[3]; // 1 : == ; 2 : > ; 3 : < ; 4: !=; that's it
  
  //constructor
  add_if_dist_lab_cond(object* this_obj, double radius, char const _lab[], object* fake_caller, int lag, char const _varLab[], char const _condition[], double condVal)
    : this_obj(this_obj), radius(radius), fake_caller(fake_caller), lag(lag), condVal(condVal) {
    strcpy(lab, _lab);
    strcpy(varLab, _varLab);
    
    if (strlen(_condition) == 0)
    { has_condition = false; }
    else {
      has_condition = true;
      strcpy(condition, _condition);
    }
    
    char distance_type = this_obj->read_distance_type();
    
    switch (distance_type) {
      case 'e' : //Euclidean
      case 'E' :
        pseudo_radius =  radius * radius;
        break;
        
      case 'm' : //Manhattan
      case 'M' :
      case 'c' : //Chebyshev
      case 'C' :
        pseudo_radius = radius;  //pseudo and real distance are equivalent
        break;
    }
  };
  
  //  defaults for the unconditional call are set in the decl.h for the COND
  //  methods.
  
  bool operator()(object* candidate) const {
    if (candidate == this_obj)
    { return false; } //do not collect self
    
    if ( 0 == strcmp(candidate->label, lab) ) {
      double ps_dst = this_obj->pseudo_distance(candidate);
      
      // double ps_dst = -1;
      // if ( pseudo_radius >= 0 ){
      // ps_dst = this_obj->pseudo_distance(candidate);
      // }
      if (pseudo_radius < 0 || ps_dst <= pseudo_radius) {
        bool isCandidate = true;
        
        if (has_condition  && ! (candidate->check_condition( varLab, condition, condVal, fake_caller, lag) ) )
        { isCandidate = false; }
        
        if ( isCandidate == true ) {
          this_obj->position->objDis_inRadius.push_back(make_pair(ps_dst, candidate));
          return true;
        }
      }
    }
    
    return false;
  } //operator()
}; //add_if_dist_lab_cond

//  sort_objDisSet
//  sort by distance to caller and also by pointer, if necessary.
inline void object::sort_objDisSet()
{
  std::sort( position->objDis_inRadius.begin(),  position->objDis_inRadius.end(), [](auto const & A, auto const & B ) {
    return A.first < B.first;
  } ); //sort only by distance
}

//  make_objDisSet_unique
//  make the set unique. May assume sorted set.
void object::make_objDisSet_unique(bool sorted)
{
  if (sorted == false)
  { sort_objDisSet(); }
  
  for (auto it = position->objDis_inRadius.begin(); it != position->objDis_inRadius.end(); /*nothing*/) {
    if (it == position->objDis_inRadius.begin()) {
      ++it;
      continue; //skip 1. entry
    }
    
    if (std::prev(it)->second == it->second) {
      it = position->objDis_inRadius.erase(it);
    }
    else {
      ++it;
    }
  }
}

//  randomise_objDisSetIntvls
//  go through intervals with same distance and randomise the order in each
//  if already sorted, do not sort again.
void object::randomise_objDisSetIntvls(bool sorted)
{
  if (sorted == false)
  { sort_objDisSet(); }
  
  int i_start = 0;
  int i_n = position->objDis_inRadius.size();
  
  for (int i = 0; i < i_n; ++i) {
  
    //entering new interval or at end of last interval?
    if (position->objDis_inRadius.at(i).second != position->objDis_inRadius.at(i_start).second || i == i_n - 1 ) {
    
      //interval with more than one object?
      if (i != i_start) {
        //create a list with the elements of the interval and random values.
        std::vector< std::pair<double, std::pair<double, object*> > > rnd_vals;
        rnd_vals.reserve(i - i_start + 1);
        int i_intvl = i_start;
        
        for (int j = i_start; j <= i; j++ ) {
          rnd_vals.emplace_back(std::make_pair(RND, position->objDis_inRadius.at(j) ) );
        }
        
        //sort the interval by random numbers of associated list, using a buffer
        std::sort( rnd_vals.begin(), rnd_vals.end(), [](auto const & A, auto const & B ) {
          return A.first < B.first;
        } ); //sort only by RND
        i_intvl = i_start;
        
        for (auto const& item : rnd_vals) {
          position->objDis_inRadius.at(i_intvl++) = item.second; //copy information from sorted intvl
        }
      }
      
      i_start = i;
    }
  }
}

//  randomise_objDisSetFull
//  randomise the objects.
// this may not be the fastest approach!
void object::randomise_objDisSetFull()
{

  //create temporary container with randomised indices for target container
  size_t i_n = position->objDis_inRadius.size();
  std::vector< std::pair<double, size_t > > rnd_vals;   //randomised index container
  rnd_vals.reserve(i_n);
  
  for (size_t i = 0; i < i_n; ++i) {
    rnd_vals.emplace_back(std::make_pair(RND, i) );
  }
  
  std::sort( rnd_vals.begin(), rnd_vals.end(), [](auto const & A, auto const & B ) {
    return A.first < B.first;
  } ); //sort only by RND
  
  //fill temporary container with new sorting
  std::deque<std::pair <double, object*> > objDis_inRadius_sorted;
  
  //     objDis_inRadius_sorted.resize(i_n);
  for (size_t index = 0; index < i_n; ++index) {
    objDis_inRadius_sorted.emplace_back(position->objDis_inRadius.at( rnd_vals.at(index).second  ));
  }
  
  //move container
  position->objDis_inRadius = std::move(objDis_inRadius_sorted); //move elements
}

// it_full
// Grab all objects with label for cycle in random order
void object::it_full(char const lab[], bool random)
{
  position->objDis_inRadius.clear();//reset vector
  
  for (int x = 0; x < position->map->xn; x++) {
    for (int y = 0; y < position->map->yn; y++) {
      for (object* candidate : position->map->elements.at(x).at(y)) {
        if ( 0 == strcmp(candidate->label, lab) )
          if (true == random) {
            position->objDis_inRadius.push_back(std::make_pair(RND, candidate));  //use rnd value as pseudo distance for sorting
          }
          else {
            position->objDis_inRadius.push_back(std::make_pair(0.0, candidate));  //no sorting
          }
      }
    }
  }
  
  if (true == random) {
    sort_objDisSet();
  }
  
  position->it_obj = std::begin(position->objDis_inRadius);
}


// it_in_radius -- works on position->objDis_inRadius
// produce iterable list of objects with label inside of radius around origin.
// the list is stored with the asking object. This allows parallelisation AND easy iterating with a macro.
// give back first element in list
//random = 'f': Do not randomise. random = 'd': randomise if equal distance. random = 'r': randomise completely
void object::it_in_radius(char const lab[], double radius, char random, object* caller, int lag, char const varLab[], char const condition[], double condVal)
{

  position->objDis_inRadius.clear();//reset vector
  
  //depending on the call of this function, the conditions are initialised meaningfully or not.
  add_if_dist_lab_cond functor_add(this, radius, lab, caller, lag, varLab, condition, condVal); //define conditions for adding
  
  traverse_boundingBox(radius, functor_add ); //add all elements inside bounding box to the list, if they are within radius
  
  sort_objDisSet();
  make_objDisSet_unique(true); //sorted = true
  
  if (random == 'd') {
    randomise_objDisSetIntvls(true); //sorted = true
  }
  else if (random == 'r') {
    randomise_objDisSetFull(); //randomise completely
  }
  
  position->it_obj = std::begin(position->objDis_inRadius);
}

//  next_neighbour_exists
//  check if the current state points to an existing object or all objects have been traversed.
bool object::next_neighbour_exists()
{
  return position->it_obj != position->objDis_inRadius.end();
}

//  next_neighbour()
//  produce the next neighbour in the CYCLE command or NULL if no next, ending the cycle.
object* object::next_neighbour()
{
  object* next_ngbo = NULL;
  
  if (position->it_obj != std::end(position->objDis_inRadius) ) {
    next_ngbo =  position->it_obj->second;
    position->it_obj++; //advance
  }
  else {
    position->objDis_inRadius.clear(); //this is imperformant but helps with debugging.
  }
  
  return next_ngbo;
}

//  first_neighbour_rnd_full
//  Initialise the gis neighbour search using the full landscape and a random order
object* object::first_neighbour_full(char const lab[], bool random)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in first_neighbour_full() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  it_full(lab, random);
  return next_neighbour();
}

//  first_neighbour
//  Initialise the nearest neighbour search and return nearest neighbours
object* object::first_neighbour(char const lab[], double radius, char random, object* caller, int lag, char const varLab[], char const condition[], double condVal)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in next_neighbour() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  it_in_radius(lab, radius, random, caller, lag, varLab, condition, condVal);
  return next_neighbour();
}

//  first_neighbour
//  Initialise the nearest neighbour search and return nearest neighbours
//  This version: Recursive search until either n neighbours or outside radius.
//  In the cycle there might be more than n items. The user needs to control.
object* object::first_neighbour_n(char const lab[], int nelements, double radius, char random, object* caller, int lag, char const varLab[], char const condition[], double condVal)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in first_neighbour_n() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  switch_closest_in_distance(nelements, lab, radius, false /* we randomise here */, caller, lag, varLab, condition, condVal);
  
  if ('d' == random)
  { randomise_objDisSetIntvls(true /*is sorted*/); }
  else if ('r' == random)
  { randomise_objDisSetFull(); }
  else if ('f' != random) {
    sprintf( gismsg, "failure in first_neighbour_n() for object '%s' with random option '%s'", label, random );
    error_hard( gismsg, "the random option is not recognised",
                "check your code to prevent this situation. Valid options are 'r','d','f'." );
    return NULL;
  }
  
  position->it_obj = std::begin(position->objDis_inRadius);
  return next_neighbour();
}

//  complete_radius
//  Return the radius that is necessary to include all the potential items
//  provides a boundary for the spatial search algorithms
//  NOT OPTIMISED
double object::complete_radius()
{
  return max(  max(position->x, position->map->xn - position->x), max(position->y, position->map->yn - position->y) );
}


//  closest_in_distance
//  find object with label lab closest to caller, if any inside radius fits
//  efficient implementation with increasing search radius
object* object::closest_in_distance(char const lab[], double radius, bool random, object* fake_caller, int lag, char const varLab[], char const condition[], double condVal)
{
  return switch_closest_in_distance(1, lab, radius, random, fake_caller, lag, varLab, condition, condVal);
}

//  nclosest_in_distance
//  find object with label lab closest to caller, if any inside radius fits
//  efficient implementation with increasing search radius
object* object::nclosest_in_distance(char const lab[], int nelements, double radius, bool random, object* fake_caller, int lag, char const varLab[], char const condition[], double condVal)
{
  return switch_closest_in_distance(nelements, lab, radius, random, fake_caller, lag, varLab, condition, condVal);
}

//  closest_in_distance
//  find object with label lab closest to caller, if any inside radius fits
//  efficient implementation with increasing search radius
object* object::switch_closest_in_distance(int nelements, char const lab[], double radius, bool random, object* fake_caller, int lag, char const varLab[], char const condition[], double condVal)
{
  if (nelements < 1) {
    sprintf( gismsg, "failure in switch_closest_in_distance() for object '%s' with nelements '%i'", label, nelements );
    error_hard( gismsg, "invalid option.",
                "check your code to prevent this situation. Make nelements at least 1" );
    return NULL;
  }
  
  double max_radius = complete_radius(); //we do not need to go beyond this radius
  
  if (radius < 0) {
    radius = max_radius; //we search everything
  }
  
  double cur_radius = ceil(min(radius, 1.0));
  position->objDis_inRadius.clear();//reset vector
  
  //depending on the call of this function, the conditions are initialised meaningfully or not.
  add_if_dist_lab_cond functor_add(this, radius, lab, fake_caller, lag, varLab, condition, condVal); //define conditions for adding
  
  //In a first initial step, we identify the items in the boundary box.
  traverse_boundingBox(cur_radius, functor_add ); //add all elements inside bounding box to the list, if they are within radius
  make_objDisSet_unique(false); //sort and make unique
  
  //In the single neighbour case, stop when a neighbour in distance is found.
  //In the n neighbours case, stop whenever the number of elements is >= n.
  //In both cases stop also when the maximum distance is matched.
  
  for (/*is init*/; (cur_radius < radius && cur_radius < max_radius); ++cur_radius ) {
    if (position->objDis_inRadius.size() >= nelements) {
    
      //check if there is a closed interval OR the radius is at least 1 level beyond the element
      if (position->objDis_inRadius.front().first < position->objDis_inRadius.back().first //the element in the back is further away then one in the front.
          || ceil(position->objDis_inRadius.back().first) < cur_radius ) {
        break; //we found a solution set (can be empty)
      }
    }
    
    traverse_boundingBoxBelt(cur_radius, functor_add );//add new options
    make_objDisSet_unique(false); //sort and make unique
  }
  
  
  if (position->objDis_inRadius.empty() == true) {
    return NULL; //no option found;
  }
  else {
    if ( 1 == nelements) {
      if ( random == false )
      { return position->objDis_inRadius.front().second; }
      
      int n = -1;  //select randomly amongst set of candidates with minimum distance
      
      for (auto const& item : position->objDis_inRadius) {
        if (item.first == position->objDis_inRadius.front().first) { //compare element distance with distance of first element
          ++n;
        }
      }
      
      if (n > 0) {
        n = uniform_int(0, n);
      }
      
      return position->objDis_inRadius.at( n ).second;
    }
    else {
      //remove elements which are extra AND farther away then the first nelements
      if (position->objDis_inRadius.size() > nelements) {
        for (int i = position->objDis_inRadius.size() - 1; i >= nelements; --i) {
          if ( position->objDis_inRadius.at(nelements) < position->objDis_inRadius.at(i) ) {
            position->objDis_inRadius.pop_back();
          }
          else {
            break;
          }
        }
      }
      
      //Check: Report the number.
      // sprintf(gismsg,"\nIn %s called as nperson version with %i elements there are actually %i elements", __func__, nelements, position->objDis_inRadius.size());
      // plog(gismsg);
      
      //Randomisation outside.
      
      return position->objDis_inRadius.front().second; //return
    }
  }
}


//  search_at_position
//  find object lab at position xy. If it exists, produce it, else return NULL
//  if single is true, check that it is the only one
//    and if not throw exception. Use exact position.
//  if not single, randomise order of potential candidates.
object* object::search_at_position(char const lab[], double x, double y, bool single)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  
  if (check_positions(x, y) == false ) {
    sprintf( gismsg, "failure in search_at_position() searching at position (%g,%g) for '%s'", x, y, lab );
    error_hard( gismsg, "the position is not on the map",
                "check your code to prevent this situation. Could be wrapping issues." );
    return NULL; //position incorrect
  }
  
  std::vector<object*> singleCandidates;
  
  for (object* candidate : position->map->elements.at(int(x)).at(int(y)) ) {
    //return first element with label
    if (x == candidate->position->x && y == candidate->position->y) {
      if ( 0 == strcmp(lab, candidate->label) ) {
        if (single == true && singleCandidates.empty() == false) {
          sprintf( gismsg, "failure in search_at_position() searching at position (%g,%g) for '%s'", x, y, lab );
          error_hard( gismsg, "there are several (at least two) items of this type present at the map.",
                      "check your code to prevent this situation." );
          return NULL;
        }
        
        singleCandidates.push_back(candidate);
      }
    }
  }
  
  if (single == true) {
    if (singleCandidates.empty() == false)
    { return singleCandidates.front(); }
    else
    { return NULL; } //no candidate at position
  }
  else {
    int select_random = uniform_int(0, singleCandidates.size() - 1);
    return singleCandidates.at(select_random);
  }
}

//  search_at_position
//  see above, just transform the position of the element to coordinates.
//  in the grid=true version use the truncated coordinates of the calling
//  object.
object* object::search_at_position(char const lab[], bool single, bool grid)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  
  if (grid == false)
  { return search_at_position(lab, position->x, position->y, single); }
  else
  { return search_at_position(lab, trunc(position->x), trunc(position->y), single); }
}

//use position of this to look at map of where.
object* object::search_at_position(char const lab[], bool single, bool grid, object* where)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
    if (where->ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_position() for where object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  
  if (grid == false)
  { return where->search_at_position(lab, position->x, position->y, single); }
  else
  { return where->search_at_position(lab, trunc(position->x), trunc(position->y), single); }
}



//  search_at_neighbour_position
//  Find an object at a position reachable by moving one step in the direction
//  if single is true, check that it is the only one
//    and if not throw exception. Use exact position.
//  if not single, randomise order of potential candidates.
//  Note: Only consideres grid-moves.
object* object::search_at_neighbour_position(char const lab[], int direction, bool single)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_neighbour_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return NULL;
  }
  
#endif
  double x_inOut = position->x;
  double y_inOut = position->y;
  
  if (get_move_position(ptr_map(), direction, x_inOut, y_inOut, true) == false) {
    sprintf( gismsg, "failure in search_at_neighbour_position() for object '%s' and direction %i", label, direction );
    error_hard( gismsg, "the move is not allowed",
                "check your code to prevent this situation" );
    return NULL;
  }
  
  return search_at_position(lab, trunc(x_inOut), trunc(y_inOut), single);
}

object* object::search_at_neighbour_position(char const lab[], char const direction[], bool single)
{
  return search_at_neighbour_position(lab, char2int_direction(direction), single);
}


//  elements_at_position
//  Count the number of elements with label lab at the position

double object::elements_at_position(char const lab[], double x, double y)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in search_at_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return 0;
  }
  
#endif
  int n = 0;
  
  for (auto const& item : position->map->elements.at(int(x)).at(int(y)) ) {
    if (item->position->x == x && item->position->y == y) {
      if ( 0 == strcmp(item->label, lab ) ) {
        n++;
      }
    }
  }
  
  return double(n);
}

double object::elements_at_position(char const lab[], bool grid)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in elements_at_position() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return 0;
  }
  
#endif
  
  if (grid == false)
  { return elements_at_position(lab, position->x, position->y); }
  else
  { return elements_at_position(lab, trunc(position->x), trunc(position->y)); }
}

//  random_pos
//  Produce a random x or y position, only ensuring that it is inside the map
double object::random_pos(const char xy)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in random_pos() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  switch (xy) {
  
    case 'x':
    case 'X':
      return uniform(0, position->map->xn);
      
    case 'y':
    case 'Y':
      return uniform(0, position->map->yn);
      
    default :
      sprintf( gismsg, "failure in random_pos() for object '%s' parameter '%c'", label, xy );
      error_hard( gismsg, "the parameter is not correct.",
                  "check your code to prevent this situation. Options are 'x' or 'y'" );
      return -1;
  }
}

//  get_pos
//  get the position of the object and return it.
double object::get_pos(char xyz)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in pos() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  switch (xyz) {
    case 'x' :
    case 'X' :
      return position->x;
      
    case 'y' :
    case 'Y' :
      return position->y;
      
    case 'Z' :
    case 'z' :
      return position->z;
      
    default  :
      ;
  }
  
  sprintf( gismsg, "failure in pos() for object '%s'", label );
  error_hard( gismsg, "only position x, y or z possible",
              "check your code to prevent this situation" );
  return -1;
}

//  char2int_direction
//  helper: Transform character direction format to integer.
int object::char2int_direction(char const direction[])
{  
  switch (direction[0]) {
    case 'n':
      if (direction[1] == 'w') {
        return 8; //north-west
      }
      else if (direction[1] == 'e') {
        return 2; //north-east
      }
      else {
        return 1; //north
      }      
      
    case 'e':
      if (direction[1] == 'n' ) {
        return 2; //north-east
      } else if (direction[1] == 's') {
        return 4; //south-east;
      } else {
        return 3; //east
      }
      
    case 's':
      if (direction[1] == 'w') {
        return 6; //south-west
      }
      else if (direction[1] == 'e') {
        return 4; //south-east
      }
      else {
        return 5; //south
      }
      
    case 'w':
      if (direction[1] == 'n') {
        return 2; //north-west
      } else if (direction[1] == 's') {
        return 6; //south-west
      } else {
        return 7; //west      
      }

    case '!': 
      return char2int_direction(&direction[1]); //if the first element is ! then check the second.
      
    default :
      return 0; //stay put.
  }
}

//  move
//  move the object in the direction specified (one step).
//  return true, if movement was possible, or false, if not.
//  Currently only boarders may prevent movement.
//  to do: Add version with COND Check.
bool object::move(char const direction[])
{

  return move(char2int_direction(direction));
}

//  move
//  see above.
bool object::move(int direction)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in move() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }
  
#endif
  double x_out = position->x;
  double y_out = position->y;
  
  if (get_move_position(ptr_map(), direction, x_out, y_out, true) == false) {
    return false;
  }
  else {
    return change_position(x_out, y_out);
  }
}

/* Move in the direction of the target.
 * Always moves first diagonal (if allowed) before moving straight.
 *  
 */
bool object::move_toward(object* target){
  #ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in move_toward() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }

    if (target->ptr_map() == NULL) {
    sprintf( gismsg, "failure in move_toward() for object '%s' and target 's'", label, target->label );
    error_hard( gismsg, "the target is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }
  
#endif

  //For x and y direction, determine naively without wraping the direction.
  //Then check if in case of wrapping moving in other direction is faster.

  //horizontal distance: +1 move up, -1 move down, 0 stay. 
  int horizontalDirection = target->position->x > position->x ? 1 : (target->position->x < position->x ? -1 : 0 );
  //check wrap horizontal
  double noWrap_xDist = pseudo_distance(position->x, 0, target->position->x, 0, true);
  double wrap_xDist = ptr_map()->wrap.noWrap ? noWrap_xDist : pseudo_distance(position->x, 0, target->position->x, 0, false);
  if ( wrap_xDist < noWrap_xDist )  horizontalDirection *= -1;

  //vertical distance: +1 move up, -1 move down, 0 stay. 
  int verticalDirection = target->position->y > position->y ? 1 : (target->position->y < position->y ? -1 : 0 );

  //check wrap vertical
  double noWrap_yDist = pseudo_distance(0, position->y, 0, target->position->y, true);
  double wrap_yDist = ptr_map()->wrap.noWrap ? noWrap_yDist : pseudo_distance(0, position->y, 0, target->position->y, false);
  if ( wrap_yDist < noWrap_yDist )  verticalDirection *= -1;
  
  //based on x and y direction and distance type, define direction.
  std::string direction = "";
  direction += verticalDirection > 0 ? 'n' : (verticalDirection < 0 ? 's' : '!');
  direction += horizontalDirection > 0 ? 'e' : (horizontalDirection < 0 ? 'w' : '!');

  // char msg[ TCL_BUFF_STR ];
  // sprintf( msg,"Pos: %g,%g  Target: %g,%g  Wrap: %s  Resulting direction: %s",
  // position->x, position->y, target->position->x, target->position->y, position->map->wrap.noWrap?"none":"some", direction.c_str());
  // plog( msg );
  

  return move(direction.c_str());
};

//  Calculate the new position for after the move. Set the inOut position accordingly.
bool object::get_move_position(gisMap* map, int direction, double& x_inOut, double& y_inOut, bool noChange)
{
  switch (direction) {
    case 0:
      return true;    //no move
      
    case 1:
      y_inOut++;
      break; //n
      
    case 2:
      y_inOut++;
      x_inOut++;
      break; //ne
      
    case 3:
      x_inOut++;
      break;  //e
      
    case 4:
      y_inOut--;
      x_inOut++;
      break; //se
      
    case 5:
      y_inOut--;
      break; //s
      
    case 6:
      y_inOut--;
      x_inOut--;
      break; //sw
      
    case 7:
      x_inOut--;
      break; //w
      
    case 8:
      y_inOut++;
      x_inOut--;
      break; //nw
  }
  
  return check_positions(map, x_inOut, y_inOut, noChange); //Adjust if necessary. Control for Wrapping.
}

//wrapper
bool object::check_positions(double& _xOut, double& _yOut, bool noChange)
{
#ifndef NO_POINTER_CHECK

  if (this->ptr_map() == NULL) {
    sprintf( gismsg, "failure in check_positions() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return false;
  }
  
#endif
  return check_positions(ptr_map(), _xOut, _yOut, noChange);
}


//  Return a vector with possible directions, including stay put.
std::vector<int> object::possible_movements_full(){
    return possible_movements(true);
}    

//  Return a vector with possible directions, not including stay put.
std::vector<int> object::possible_movements_move(){
    return possible_movements(false);
}    

//  Return a vector with possible directions, including stay put.
std::vector<int> object::possible_movements_full(double const x_pos, double const y_pos){
    return possible_movements(x_pos, y_pos, true);
}    

//  Return a vector with possible directions, not including stay put.
std::vector<int> object::possible_movements_move(double const x_pos, double const y_pos){
    return possible_movements(x_pos, y_pos, false);
}    

std::vector<int> object::possible_movements(bool noMoveOption){

  if (this->ptr_map() == NULL) {
    sprintf( gismsg, "failure in possible_movements() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return std::vector<int>();
  }
  
  return possible_movements(get_pos('x'),get_pos('y'),noMoveOption);
  
}

std::vector<int> object::possible_movements(double const x_pos, double const y_pos, bool noMoveOption){

  if (this->ptr_map() == NULL) {
    sprintf( gismsg, "failure in possible_movements() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return std::vector<int>();
  }
    
    std::vector<int> directions;
    if (noMoveOption)
        directions.push_back(0);
    
    for (int direction = 1; direction <= 8; direction++){
        double temp_x = x_pos; //get modified!
        double temp_y = y_pos;
        if ( true == get_move_position(ptr_map(), direction, temp_x, temp_y, true) ) {
            directions.push_back(direction);
        }        
    }
    
    return directions;
}

//  check_positions
//  Function to check if the x any y are in the bounds of the map.
//  If wrapping is possible and allowed, the positions are transformed
//  accordingly
//  in case change of positions is not allowed, only check and do not adjust
bool object::check_positions(gisMap* map, double& _xOut, double& _yOut, bool noChange)
{

  if (   (_xOut >= 0.0 && _xOut < double(map->xn) )
         && (_yOut >= 0.0 && _yOut < double(map->yn) ) ) {
    return true; //all fine, nothing to change.
  }
  
  //no change allowed.
  if (noChange == true)
  { return false; }
  
  double _x = _xOut;
  double _y = _yOut;
  
  if (_x < 0) {
    if (map->wrap.left == true) {
      while (_x < 0) {
        _x = double(map->xn) + _x;   //_x is neg
      }
    }
    else {
      return false;
    }
  }
  
  if (_y < 0) {
    if (map->wrap.bottom == true) {
      while (_y < 0) {
        _y = double(map->yn) + _y;   //_y is neg
      }
    }
    else {
      return false;
    }
  }
  
  if (_x >= map->xn) {
    if (map->wrap.right == true) {
      while (_x >= map->xn) {
        _x -= double(map->xn);
      }
    }
    else {
      return false;
    }
  }
  
  if (_y >= map->yn) {
    if (map->wrap.top == true) {
      while (_y >= map->yn) {
        _y -= double(map->yn);
      }
    }
    else {
      return false;
    }
  }
  
  if (check_positions(map, _x, _y, noChange) == false) { //recursively for new corner cases!
    return false;
  }
  
  _yOut = _y; //set values
  _xOut = _x;
  return true;
}

//Initialise a lattice for the gis.
double object::init_lattice_gis(int init_color, double pixW, double pixH)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in gis_init_lattice() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  //double init_lattice( int init_color, double nrow, double ncol, double pixW, double pixH )
  //check if lattice has been initialised before.
  if (lattice != NULL) {
    sprintf( gismsg, "failure in gis_init_lattice() for object '%s'", label );
    error_hard( gismsg, "the lattice has already been initialised. Re-initialising is not allowed.",
                "check your code to prevent this situation" );
    return -1;
  }
  
  position->map->has_lattice = true;
  position->map->local_lattice.resize(position->map->xn);
  
  for (auto& x : position->map->local_lattice) {
    x.resize(position->map->yn); //number of rows, copy
    
    for (auto& item : x) {
      item = init_color;
    }
  }
  
  return init_lattice( init_color, position->map->yn, position->map->xn, pixW, pixH  );
}

void object::close_lattice_gis( )
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in close_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return;
  }
  
#endif
  
  //double init_lattice( int init_color, double nrow, double ncol, double pixW, double pixH )
  //check if lattice has been initialised before.
  if (lattice == NULL) {
    sprintf( gismsg, "failure in close_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the lattice was not initialised.",
                "check your code to prevent this situation" );
    return;
  }
  
  if (false == position->map->has_lattice) {
    sprintf( gismsg, "failure in close_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the lattice was not initialised as visual GIS representation.",
                "check your code to prevent this situation" );
    return;
  }
  
  position->map->has_lattice = false;
  position->map->local_lattice.clear();
  close_lattice( ); //close original lattice
}

//Lattice commands adjusted for GIS

double object::write_lattice_gis(double colour)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in write_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  return write_lattice_gis(position->x, position->y, colour, true);
}

double object::write_lattice_gis(double x, double y, double colour, bool noChange)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in write_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  if (false == position->map->has_lattice) {
    sprintf( gismsg, "failure in write_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the map is not connected to a graphical lattice",
                "check your code to prevent this situation" );
    return -1;
  }
  
  if (check_positions(x, y, noChange) == false) {
    return -1; //error
  }
  
  //save to static gis lattice
  position->map->local_lattice.at( (int)x ).at( (int)y ) = (int) colour;
  return update_lattice_gis( (int)x, (int)y ); //send to graphical lattice
}



double object::update_lattice_gis(int x, int y) //we know that x y are correct by here
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in update_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  if (false == position->map->has_lattice) {
    sprintf( gismsg, "failure in update_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the map is not connected to a graphical lattice",
                "check your code to prevent this situation" );
    return -1;
  }
  
  
  //Cycle through all elements at position and select colour from the one with the highest priority.
  //If none is present, default to the static / non-object lattice
  int highest_lat_priority = 0;
  int lat_color = position->map->local_lattice.at(x).at(y); //default with minimum priority (<0)
  
  for ( const auto& item : position->map->elements.at(x).at(y) ) {
    if (item->position->lattice_priority >= highest_lat_priority) {
      highest_lat_priority = item->position->lattice_priority;
      lat_color = item->position->lattice_color;
    }
  }
  
  //transform coordinates for lattice. internally lattice starts with (0,0) top left. (in update_lattice/externally with (1,1) )
  //gis starts with (0,0) top down.
  
  return update_lattice( position->map->yn - y,   x + 1, lat_color ); //send to graphical lattice
}

double object::read_lattice_gis( )
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in read_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  return read_lattice_gis(position->x, position->y, true);
}

double object::read_lattice_gis( double x, double y, bool noChange)
{
#ifndef NO_POINTER_CHECK

  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in read_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  
  if (false == position->map->has_lattice) {
    sprintf( gismsg, "failure in read_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "the map is not connected to a graphical lattice",
                "check your code to prevent this situation" );
    return -1;
  }
  
  if (check_positions(x, y, noChange) == false) {
    sprintf( gismsg, "failure in read_lattice_gis() for object '%s'", label );
    error_hard( gismsg, "reading from xy outside of canvas",
                "check your code to prevent this situation" );
    return -1; //error
  }
  
  //transform coordinates for lattice. lattice starts with (0,0) top left.
  //gis starts with (0,0) top down.
  return read_lattice( position->map->yn - y, x + 1 );
}

void object::set_lattice_priority(int priority)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in set_lattice_priority() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return;
  }
  
  position->lattice_priority = priority;
  update_lattice_gis( (int) position->x, (int) position->y );
}
void object::set_lattice_color(int color)
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in set_lattice_color() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return;
  }
  
  position->lattice_color = color;
  update_lattice_gis( (int) position->x, (int) position->y );
}

double object::read_lattice_color( void )
{
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in read_lattice_color() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
  if (position->lattice_priority < 0) {
    sprintf( gismsg, "failure in read_lattice_color() for object '%s'", label );
    error_hard( gismsg, "the object is not visualised (priority < 0)",
                "check your code to prevent this situation" );
    return -1;
  }
  
  return position->lattice_color;
}

int object::load_data_gis_mat( const char* inputfile, const char* obj_lab, const char* var_lab, int t_update )
{
  /*  Read data points x,y with associated data values val from the inputfile
      and store it at the gis_obj with label obj_lab into variable var_lab
      with lag lag.
  
      The underlying file needs to be in a format cols = x, rows = y with
      (0,0) being the lower left point and (xn-1,yn-1) being the upper right.
  
      The First row and colum ar ignored.
  
      If the object of type obj_lab does not yet exist at this position, it is
      created (from the blueprint) and registered.
  */
#ifndef NO_POINTER_CHECK
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in load_data_gis_mat() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  int elements_added = 0;
  rapidcsv::Document f_in;
  
  try {
    f_in = rapidcsv::Document(inputfile, rapidcsv::LabelParams());
  }
  catch (const std::out_of_range& e) {
    sprintf( gismsg, "failure in %s for file '%s'. \nOut of range error: %s", __func__, inputfile, e.what() );
    error_hard( gismsg, " ... ",
                "check your code to prevent this situation" );
    throw;
  }
  catch ( ... ) {
    sprintf( gismsg, "failure in load_data_gis_mat() for file '%s'", inputfile );
    error_hard( gismsg, "most likely the file cannot be found or it is not comma separated",
                "check your code to prevent this situation" );
    throw;
  }
  
  if (f_in.GetColumnCount() < 2 || f_in.GetRowCount() < 2 ) {
    sprintf( gismsg, "failure in load_data_gis_mat() for file '%s'", inputfile );
    error_hard( gismsg, "most likely the file does not exist",
                "check your code to prevent this situation" );
    return -1;
  }
  else {
    sprintf( gismsg, "\nLoading data from file %s for %i x %i grid to variable %s", inputfile, f_in.GetColumnCount(), f_in.GetRowCount(), var_lab);
    plog(gismsg);
  }
  
  object* obj_parent = search(obj_lab) -> up;
  
  double val;
  
  for (int x = f_in.GetColumnCount() - 1; x >= 0; --x) {
    for (int y = 0; y < f_in.GetRowCount(); ++y) {
      val = f_in.GetCell<double>(x, y);
      object* cur = search_at_position(obj_lab, x, y, true); //true flag: error if more than one option exists.
      
      if (cur == NULL) { //if necessary, create the object
        cur = obj_parent->add_n_objects2( obj_lab, 1 ); //create new object in object parent
        cur->register_at_map(ptr_map(), x, y);//register it in space at given position
        elements_added++;
      }
      
      cur->write( var_lab, val,  t_update); //write value
      // sprintf(gismsg, "\nAdded live cell at pos %g, %g", x_pos, y_pos);
      // plog(gismsg);
    }
  }
  
  return elements_added;
}

int object::load_data_gis( const char* inputfile, const char* obj_lab, const char* var_lab, int t_update )
{
  /*  Read data points x,y with associated data values val from the inputfile
      and store it at the gis_obj with label obj_lab into variable var_lab
      with lag lag.
  
      If the object of type obj_lab does not yet exist at this position, it is
      created (from the blueprint) and registered.
  */
#ifndef NO_POINTER_CHECK
  if (ptr_map() == NULL) {
    sprintf( gismsg, "failure in load_data_gis() for object '%s'", label );
    error_hard( gismsg, "the object is not registered in any map",
                "check your code to prevent this situation" );
    return -1;
  }
  
#endif
  int elements_added = 0;
  rapidcsv::Document f_in(inputfile, rapidcsv::LabelParams(-1, -1));
  
  if (f_in.GetColumnCount() < 3 || f_in.GetRowCount() < 1 ) {
    sprintf( gismsg, "failure in load_data_gis() for file '%s'", inputfile );
    error_hard( gismsg, "most likely the file does not exist",
                "check your code to prevent this situation" );
    return -1;
  }
  
  object* obj_parent = search(obj_lab) -> up;
  
  double x_pos, y_pos, val;
  
  for (int row = 0; row < f_in.GetRowCount(); ++row) {
    x_pos = f_in.GetCell<double>(0, row);
    y_pos = f_in.GetCell<double>(1, row);
    val = f_in.GetCell<double>(2, row);
    object* cur = search_at_position(obj_lab, x_pos, y_pos, true); //error if more than one option exists.
    
    if (cur == NULL) { //if necessary, create the object
      cur = obj_parent->add_n_objects2( obj_lab, 1 ); //create new object in object parent
      cur->register_at_map(ptr_map(), x_pos, y_pos);//register it in space at given position
      elements_added++;
    }
    
    cur->write( var_lab, val,  t_update); //write value
    // sprintf(gismsg, "\nAdded live cell at pos %g, %g", x_pos, y_pos);
    // plog(gismsg);
  }
  
  return elements_added;
}

std::string object::gis_info(bool append)
{
  char buffer1[300];
  sprintf(buffer1, "\n(GIS OBJECT INFO) t=%i: '%s'", t, label);
  
  if (ptr_map() == NULL) {
    return (std::string(buffer1) + " is not part of gis");
  }
  
  char buffer[300];
  const void * address = static_cast<const void*>(position->map);
  std::stringstream ss;
  ss << address; 
  
  if (!append) {
    
    if (true == is_unique() )
    { sprintf(buffer, "%s UID %g position (%g,%g) at map %s", buffer1, unique_id(), position->x, position->y, ss.str().c_str()  ); }
    else
    { 
  const void * address2 = static_cast<const void*>(this);
  std::stringstream ss2;
  ss2 << address2; 
  sprintf(buffer, "%s (%s) position (%g,%g) at map %s", buffer1, ss2.str().c_str(), position->x, position->y, ss.str().c_str() ); }
  }
  else
  { sprintf(buffer, " position (%g,%g) at map %s", position->x, position->y,  ss.str().c_str() ); }
  
  return std::string(buffer);
}


#endif //#ifdef CPP11
