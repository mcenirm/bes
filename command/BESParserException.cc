// BESParserException.cc

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

// 2004 Copyright University Corporation for Atmospheric Research

#include "BESParserException.h"
#include "cgi_util.h"

// Add docs. jhrg 3/29/06
int
BESParserException::handleException( BESException &e,
					 BESDataHandlerInterface &dhi )
{
    BESParserException *pe=dynamic_cast<BESParserException*>(&e);
    // What if pe is null. jhrg 3/29/06
    if(pe)
    {
	bool ishttp = false ;
	if( dhi.transmit_protocol == "HTTP" )
	    ishttp = true ;
	if( ishttp ) set_mime_text( stdout, dods_error ) ;
	fprintf( stdout, "There is a parse error!\n" ) ;
	fprintf( stdout, "%s\n", e.get_error_description().c_str() ) ;
	return OPENDAP_PARSER_ERROR;
    }
    return 0 ;
}
