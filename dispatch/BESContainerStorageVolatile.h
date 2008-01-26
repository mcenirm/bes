// BESContainerStorageVolatile.h

// This file is part of bes, A C++ back-end server implementation framework
// for the OPeNDAP Data Access Protocol.

// Copyright (c) 2004,2005 University Corporation for Atmospheric Research
// Author: Patrick West <pwest@ucar.edu> and Jose Garcia <jgarcia@ucar.edu>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact University Corporation for Atmospheric Research at
// 3080 Center Green Drive, Boulder, CO 80301
 
// (c) COPYRIGHT University Corporation for Atmospheric Research 2004-2005
// Please read the full copyright statement in the file COPYRIGHT_UCAR.
//
// Authors:
//      pwest       Patrick West <pwest@ucar.edu>
//      jgarcia     Jose Garcia <jgarcia@ucar.edu>

#ifndef BESContainerStorageVolatile_h_
#define BESContainerStorageVolatile_h_ 1

#include <map>
#include <string>

using std::map ;
using std::string ;

#include "BESContainerStorage.h"

/** @brief implementation of BESContainerStorage that stores containers
 * in memory for the duration of this process.
 *
 * This implementation of BESContainerStorage stores volatile
 * containers in memory for the duration of this process. A list of
 * containers is stored in the object. The look_for method simply looks for
 * the specified symbolic name in the list of containers and returns if a
 * match is found. Containers can be added to this instance as long as the
 * symbolic name doesn't already exist.
 *
 * @see BESContainerStorage
 * @see BESContainer
 */
class BESContainerStorageVolatile : public BESContainerStorage
{
private:
    map< string, BESContainer * > _container_list ;
protected:
    string			_root_dir ;
    bool			_follow_sym_links ;

    virtual void		add_container( BESContainer *c ) ;
public:
    				BESContainerStorageVolatile( const string &n ) ;
    virtual			~BESContainerStorageVolatile() ;

    typedef map< string, BESContainer * >::const_iterator Container_citer ;
    typedef map< string, BESContainer * >::iterator Container_iter ;
    virtual BESContainer *	look_for( const string &sym_name ) ;
    virtual void		add_container( const string &sym_name,
					       const string &real_name,
					       const string &type ) ;
    virtual bool		del_container( const string &s_name ) ;
    virtual bool		del_containers( ) ;

    virtual void		show_containers( BESInfo &info ) ;

    virtual void		dump( ostream &strm ) const ;
};

#endif // BESContainerStorageVolatile_h_

