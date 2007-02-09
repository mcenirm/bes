// SayCommand.cc

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

#include "SayCommand.h"
#include "BESTokenizer.h"
#include "BESResponseHandlerList.h"
#include "BESParserException.h"
#include "SayResponseNames.h"

/** @brief parses the request to say something to someone
 *
 * The syntax for a request handled by this response handler is:
 *
 * say &lt;what&gt; to &lt;whom&gt;;
 *
 * The request must end with a semicolon and must contain the context name
 * and the context value.
 *
 * @param tokenizer holds on to the list of tokens to be parsed
 * @param dhi structure that holds request and response information
 * @throws BESParserException if there is a problem parsing the request
 * @see BESTokenizer
 * @see _BESDataHandlerInterface
 */
BESResponseHandler *
SayCommand::parse_request( BESTokenizer &tokenizer,
                                           BESDataHandlerInterface &dhi )
{
    string my_token ;

    /* No sub command, so proceed with the default set command
     */
    dhi.action = say_RESPONSE ;
    BESResponseHandler *retResponse =
	BESResponseHandlerList::TheList()->find_handler( say_RESPONSE ) ;
    if( !retResponse )
    {
	string s = (string)"No response handler for command " + say_RESPONSE ;
	throw BESParserException( s, __FILE__, __LINE__ ) ;
    }

    // Next token should what saying
    my_token = tokenizer.get_next_token() ;
    if( my_token == ";" )
    {
	tokenizer.parse_error( my_token + " not expected\n" ) ;
    }
    dhi.data[SAY_WHAT] = my_token ;

    // Next token should be the token "to"
    my_token = tokenizer.get_next_token() ;
    if( my_token != "to" )
    {
	tokenizer.parse_error( my_token + " not expected\n" ) ;
    }

    // Next token should be what is being said
    my_token = tokenizer.get_next_token() ;
    if( my_token == ";" )
    {
	tokenizer.parse_error( my_token + " not expected\n" ) ;
    }
    dhi.data[SAY_TO] = my_token ;

    // Last token should be the terminating semicolon (;)
    my_token = tokenizer.get_next_token() ;
    if( my_token != ";" )
    {
	tokenizer.parse_error( my_token + " not expected\n" ) ;
    }

    return retResponse ;
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance
 *
 * @param strm C++ i/o stream to dump the information to
 */
void
SayCommand::dump( ostream &strm ) const
{
    strm << BESIndent::LMarg << "SayCommand::dump - ("
			     << (void *)this << ")" << endl ;
    BESIndent::Indent() ;
    BESCommand::dump( strm ) ;
    BESIndent::UnIndent() ;
}

