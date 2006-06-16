// BESVersionInfo.cc

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
 
// (c) COPYRIGHT University Corporation for Atmostpheric Research 2004-2005
// Please read the full copyright statement in the file COPYRIGHT_UCAR.
//
// Authors:
//      pwest       Patrick West <pwest@ucar.edu>
//      jgarcia     Jose Garcia <jgarcia@ucar.edu>

#ifdef __GNUG__
#pragma implementation
#endif

#include <sstream>

using std::ostringstream ;

#include "BESVersionInfo.h"

/** @brief constructs a basic text information response object.
 *
 * Uses the default OPeNDAP.Info.Buffered key in the dods initialization file to
 * determine whether the information should be buffered or not.
 *
 * @see BESXMLInfo
 * @see DODSResponseObject
 */
BESVersionInfo::BESVersionInfo()
    : BESXMLInfo(),
      _firstDAPVersion( true ),
      _DAPstrm( 0 ),
      _firstBESVersion( true ),
      _BESstrm( 0 ),
      _firstHandlerVersion( true ),
      _Handlerstrm( 0 )
{
    _buffered = true ;
}

/** @brief constructs a version text/xml information response object.
 *
 * Uses the default OPeNDAP.Info.Buffered key in the dods initialization file to
 * determine whether the information should be buffered or not.
 *
 * @param is_http whether the response is going to a browser
 * @see BESXMLInfo
 * @see DODSResponseObject
 */
BESVersionInfo::BESVersionInfo( bool is_http )
    : BESXMLInfo( is_http ),
      _firstDAPVersion( true ),
      _DAPstrm( 0 ),
      _firstBESVersion( true ),
      _BESstrm( 0 ),
      _firstHandlerVersion( true ),
      _Handlerstrm( 0 )
{
    _buffered = true ;
    _strm = new ostringstream ;
    add_data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" ) ;
    add_data( "<showVersion>\n" ) ;
    add_data( "    <response>\n" ) ;
}

BESVersionInfo::~BESVersionInfo()
{
    if( _DAPstrm ) delete _DAPstrm ;
    if( _BESstrm ) delete _BESstrm ;
    if( _Handlerstrm ) delete _Handlerstrm ;
}

void
BESVersionInfo::print( FILE *out )
{
    if( !_firstDAPVersion && _DAPstrm ) (*_DAPstrm) << "        </DAP>\n" ;
    if( !_firstBESVersion && _BESstrm ) (*_BESstrm) << "        </BES>\n" ;
    if( !_firstHandlerVersion && _Handlerstrm ) (*_Handlerstrm) << "        </Handlers>\n" ;
    if( _DAPstrm ) add_data( ((ostringstream *)_DAPstrm)->str() ) ;
    if( _BESstrm ) add_data( ((ostringstream *)_BESstrm)->str() ) ;
    if( _Handlerstrm ) add_data( ((ostringstream *)_Handlerstrm)->str() ) ;
    add_data( "    </response>\n" ) ;
    add_data( "</showVersion>\n" ) ;
    BESInfo::print( out ) ;
}

void
BESVersionInfo::addDAPVersion( const string &v )
{
    if( _firstDAPVersion )
    {
	_DAPstrm = new ostringstream ;
	_firstDAPVersion = false ;
	(*_DAPstrm) << "        <DAP>\n" ;
    }
    (*_DAPstrm) << "            <version> " << v << " </version>\n" ;
}

void
BESVersionInfo::addBESVersion( const string &n, const string &v )
{
    if( _firstBESVersion )
    {
	_BESstrm = new ostringstream ;
	_firstBESVersion = false ;
	(*_BESstrm) << "        <BES>\n" ;
    }
    (*_BESstrm) << "            <lib>\n" ;
    (*_BESstrm) << "                <name> " << n << " </name>\n" ;
    (*_BESstrm) << "                <version> " << v << " </version>\n" ;
    (*_BESstrm) << "            </lib>\n" ;
}

void
BESVersionInfo::addHandlerVersion( const string &n, const string &v )
{
    if( _firstHandlerVersion )
    {
	_Handlerstrm = new ostringstream ;
	_firstHandlerVersion = false ;
	(*_Handlerstrm) << "        <Handlers>\n" ;
    }
    (*_Handlerstrm) << "            <lib>\n" ;
    (*_Handlerstrm) << "                <name> " << n << " </name>\n" ;
    (*_Handlerstrm) << "                <version> " << v << " </version>\n" ;
    (*_Handlerstrm) << "            </lib>\n" ;
}

// $Log: BESVersionInfo.cc,v $