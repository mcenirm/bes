// ServerApp.cc

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

#include <signal.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#include <iostream>
#include <fstream>

using std::cout ;
using std::cerr ;
using std::endl ;
using std::ios ;
using std::ofstream ;

#include "config.h"

#include "ServerApp.h"
#include "ServerExitConditions.h"
#include "TheBESKeys.h"
#include "SocketListener.h"
#include "TcpSocket.h"
#include "UnixSocket.h"
#include "BESServerHandler.h"
#include "BESException.h"
#include "PPTException.h"
#include "PPTServer.h"
#include "PPTException.h"
#include "SocketException.h"
#include "BESMemoryManager.h"
#include "BESDebug.h"
#include "BESServerUtils.h"

#include "BESDefaultModule.h"
#include "BESDefaultCommands.h"

ServerApp::ServerApp()
    : BESModuleApp(),
      _portVal( 0 ),
      _gotPort( false ),
      _unixSocket( "" ),
      _secure( false ),
      _mypid( 0 ),
      _ts( 0 ),
      _us( 0 ),
      _ps( 0 )
{
    _mypid = getpid() ;
}

ServerApp::~ServerApp()
{
}

void
ServerApp::signalTerminate( int sig )
{
    if( sig == SIGTERM )
    {
	BESApp::TheApplication()->terminate( sig ) ;
	exit( SERVER_EXIT_NORMAL_SHUTDOWN ) ;
    }
}

void
ServerApp::signalInterrupt( int sig )
{
    if( sig == SIGINT )
    {
	BESApp::TheApplication()->terminate( sig ) ;
	exit( SERVER_EXIT_NORMAL_SHUTDOWN ) ;
    }
}

void
ServerApp::signalRestart( int sig )
{
    if( sig == SIGUSR1 )
    {
	BESApp::TheApplication()->terminate( sig ) ;
	exit( SERVER_EXIT_RESTART ) ;
    }
}

void
ServerApp::set_group_id()
{
#if !defined(OS2) && !defined(TPF)
    // OS/2 and TPF don't support groups.

    // get group id or name from BES configuration file
    // If BES.Group begins with # then it is a group id,
    // else it is a group name and look up the id.
    BESDEBUG( "server", "ServerApp: Setting group id ... " )
    bool found = false ;
    string key = "BES.Group" ;
    string group_str = TheBESKeys::TheKeys()->get_key( key, found ) ;
    if( !found || group_str.empty() )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Group not specified in BES configuration file"
	     << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
    BESDEBUG( "server", "to " << group_str << " ... " )

    gid_t new_gid = 0 ;
    if( group_str[0] == '#' )
    {
	// group id starts with a #, so is a group id
	const char *group_c = group_str.c_str() ;
	group_c++ ;
	new_gid = atoi( group_c ) ;
    }
    else
    {
	// specified group is a group name
	struct group *ent ;
	ent = getgrnam( group_str.c_str() ) ;
	if( !ent )
	{
	    BESDEBUG( "server", "FAILED" << endl ) ;
	    cerr << "FAILED: Group " << group_str << " does not exist" << endl ;
	    exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
	}
	new_gid = ent->gr_gid ;
    }

    if( new_gid < 1 )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Group id " << new_gid
	     << " not a valid group id for BES" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }

    BESDEBUG( "server", "to id " << new_gid << " ... " )
    if( setgid( new_gid ) == -1 )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: unable to set the group id to " << new_gid << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }

    BESDEBUG( "server", "OK" << endl ) ;
#else
    BESDEBUG( "server", "ServerApp: Groups not supported in this OS" )
#endif
}

void
ServerApp::set_user_id()
{
    BESDEBUG( "server", "ServerApp: Setting user id ... " )

    // Get user name or id from the BES configuration file.
    // If the BES.User value begins with # then it is a user
    // id, else it is a user name and need to look up the
    // user id.
    bool found = false ;
    string key = "BES.User" ;
    string user_str = TheBESKeys::TheKeys()->get_key( key, found ) ;
    if( !found || user_str.empty() )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: User not specified in BES configuration file"
	     << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
    BESDEBUG( "server", "to " << user_str << " ... " )

    uid_t new_id = 0 ;
    if( user_str[0] == '#' )
    {
	const char *user_str_c = user_str.c_str() ;
	user_str_c++ ;
	new_id = atoi( user_str_c ) ;
    }
    else
    {
	struct passwd *ent ;
	ent = getpwnam( user_str.c_str() ) ;
	if( !ent )
	{
	    BESDEBUG( "server", "FAILED" << endl ) ;
	    cerr << "FAILED: Bad user name specified: "
		 << user_str << endl ;
	    exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
	}
	new_id = ent->pw_uid ;
    }

    // new user id can not be root (0)
    if( !new_id )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: BES can not run as root" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }

    BESDEBUG( "server", "to " << new_id << " ... " )
    if( setuid( new_id ) == -1 )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Unable to set user id to "
	     << new_id << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
}

int
ServerApp::initialize( int argc, char **argv )
{
    // must be root to run this app and to set user id and group id later
    uid_t curr_euid = geteuid() ;
    if( curr_euid )
    {
	cerr << "FAILED: Must be root to run BES" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }

    int c = 0 ;
    bool needhelp = false ;
    string dashi ;
    string dashc ;

    // If you change the getopt statement below, be sure to make the
    // corresponding change in daemon.cc and besctl.in
    while( ( c = getopt( argc, argv, "hvsd:c:p:u:i:r:" ) ) != EOF )
    {
	switch( c )
	{
	    case 'i':
		dashi = optarg ;
		break ;
	    case 'c':
		dashc = optarg ;
		break ;
	    case 'r':
		break ; // we can ignore the /var/run directory option here
	    case 'p':
		_portVal = atoi( optarg ) ;
		_gotPort = true ;
		break ;
	    case 'u':
		_unixSocket = optarg ;
		break ;
	    case 'd':
		BESDebug::SetUp( optarg ) ;
		break ;
	    case 'v':
		BESServerUtils::show_version( BESApp::TheApplication()->appName() ) ;
		break ;
	    case 's':
		_secure = true ;
		break ;
	    case 'h':
	    case '?':
	    default:
		needhelp = true ;
		break ;
	}
    }

    // If the -c optiion was passed, set the config file
    // name in TheBESKeys
    if( !dashc.empty() )
    {
	TheBESKeys::ConfigFile = dashc ;
    }

    // If the -c option was not passed, but the -i option
    // was passed, then use the -i option to construct
    // the path to the config file
    if( dashc.empty() && !dashi.empty() )
    {
	if( dashi[dashi.length()-1] != '/' )
	{
	    dashi += '/' ;
	}
	string conf_file = dashi + "etc/bes/bes.conf" ;
	TheBESKeys::ConfigFile = conf_file ;
    }

    bool found = false ;
    string port_key = "BES.ServerPort" ;
    if( !_gotPort )
    {
	string sPort = TheBESKeys::TheKeys()->get_key( port_key, found ) ;
	if( found )
	{
	    _portVal = atoi( sPort.c_str() ) ;
	    if( _portVal != 0 )
	    {
		_gotPort = true ;
	    }
	}
    }

    found = false ;
    string socket_key = "BES.ServerUnixSocket" ;
    if( _unixSocket == "" )
    {
	_unixSocket = TheBESKeys::TheKeys()->get_key( socket_key, found ) ;
    }

    if( !_gotPort && _unixSocket == "" )
    {
	cout << endl << "Must specify either a tcp port"
	     << " or a unix socket or both" << endl ;
	cout << "Please specify on the command line with"
	     << " -p <port> -u <unix_socket> "
	     << endl
	     << "Or specify in the bes configuration file with "
	     << port_key << " and/or " << socket_key
	     << endl << endl ;
	BESServerUtils::show_usage( BESApp::TheApplication()->appName() ) ;
    }

    found = false ;
    if( _secure == false )
    {
	string key = "BES.ServerSecure" ;
	string isSecure = TheBESKeys::TheKeys()->get_key( key, found ) ;
	if( isSecure == "Yes" || isSecure == "YES" || isSecure == "yes" )
	{
	    _secure = true ;
	}
    }

    BESDEBUG( "server", "ServerApp: Registering signal SIGTERM ... " )
    if( signal( SIGTERM, signalTerminate ) == SIG_ERR )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Can not register SIGTERM signal handler" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
    BESDEBUG( "server", "OK" << endl ) ;

    BESDEBUG( "server", "ServerApp: Registering signal SIGINT ... " )
    if( signal( SIGINT, signalInterrupt ) == SIG_ERR )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Can not register SIGINT signal handler" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
    BESDEBUG( "server", "OK" << endl ) ;

    BESDEBUG( "server", "ServerApp: Registering signal SIGUSR1 ... " )
    if( signal( SIGUSR1, signalRestart ) == SIG_ERR )
    {
	BESDEBUG( "server", "FAILED" << endl ) ;
	cerr << "FAILED: Can not register SIGUSR1 signal handler" << endl ;
	exit( SERVER_EXIT_FATAL_CAN_NOT_START ) ;
    }
    BESDEBUG( "server", "OK" << endl ) ;

    BESDEBUG( "server", "ServerApp: initializing default module ... " )
    BESDefaultModule::initialize( argc, argv ) ;
    BESDEBUG( "server", "OK" << endl ) ;

    BESDEBUG( "server", "ServerApp: initializing default commands ... " )
    BESDefaultCommands::initialize( argc, argv ) ;
    BESDEBUG( "server", "OK" << endl ) ;

    int ret = BESModuleApp::initialize( argc, argv ) ;

    BESDEBUG( "server", "ServerApp: initialized settings:" << *this ) ;

    if( needhelp )
    {
	BESServerUtils::show_usage( BESApp::TheApplication()->appName() ) ;
    }

    // Now that we have loaded all modules and given them the chance to initialize
    // set the user id and the group id to what is specified in the BES
    // configuration file.
    set_group_id() ;
    set_user_id() ;

    return ret ;
}

int
ServerApp::run()
{
    try
    {
	BESDEBUG( "server", "ServerApp: initializing memory pool ... " )
	BESMemoryManager::initialize_memory_pool() ;
	BESDEBUG( "server", "OK" << endl ) ;

	SocketListener listener ;

	if( _portVal )
	{
	    _ts = new TcpSocket( _portVal ) ;
	    listener.listen( _ts ) ;
	    BESDEBUG( "server", "ServerApp: listening on port (" << _portVal << ")" << endl )
	}

	if( !_unixSocket.empty() )
	{
	    _us = new UnixSocket( _unixSocket ) ;
	    listener.listen( _us ) ;
	    BESDEBUG( "server", "ServerApp: listening on unix socket (" << _unixSocket << ")" << endl )
	}

	BESServerHandler handler ;

	_ps = new PPTServer( &handler, &listener, _secure ) ;
	_ps->initConnection() ;
    }
    catch( SocketException &se )
    {
	cerr << "caught SocketException" << endl ;
	cerr << se.getMessage() << endl ;
	return 1 ;
    }
    catch( PPTException &pe )
    {
	cerr << "caught PPTException" << endl ;
	cerr << pe.getMessage() << endl ;
	return 1 ;
    }
    catch( ... )
    {
	cerr << "caught unknown exception" << endl ;
	return 1 ;
    }

    return 0 ;
}

int
ServerApp::terminate( int sig )
{
    pid_t apppid = getpid() ;
    if( apppid == _mypid )
    {
	if( _ps )
	{
	    _ps->closeConnection() ;
	    delete _ps ;
	}
	if( _ts )
	{
	    _ts->close() ;
	    delete _ts ;
	}
	if( _us )
	{
	    _us->close() ;
	    delete _us ;
	}

	BESDEBUG( "server", "ServerApp: terminating default module ... " )
	BESDefaultModule::terminate( ) ;
	BESDEBUG( "server", "OK" << endl ) ;

	BESDEBUG( "server", "ServerApp: terminating default commands ... " )
	BESDefaultCommands::terminate( ) ;
	BESDEBUG( "server", "OK" << endl ) ;

	BESModuleApp::terminate( sig ) ;
    }
    return sig ;
}

/** @brief dumps information about this object
 *
 * Displays the pointer value of this instance
 *
 * @param strm C++ i/o stream to dump the information to
 */
void
ServerApp::dump( ostream &strm ) const
{
    strm << BESIndent::LMarg << "ServerApp::dump - ("
			     << (void *)this << ")" << endl ;
    BESIndent::Indent() ;
    strm << BESIndent::LMarg << "got port? " << _gotPort << endl ;
    strm << BESIndent::LMarg << "port: " << _portVal << endl ;
    strm << BESIndent::LMarg << "unix socket: " << _unixSocket << endl ;
    strm << BESIndent::LMarg << "is secure? " << _secure << endl ;
    strm << BESIndent::LMarg << "pid: " << _mypid << endl ;
    if( _ts )
    {
	strm << BESIndent::LMarg << "tcp socket:" << endl ;
	BESIndent::Indent() ;
	_ts->dump( strm ) ;
	BESIndent::UnIndent() ;
    }
    else
    {
	strm << BESIndent::LMarg << "tcp socket: null" << endl ;
    }
    if( _us )
    {
	strm << BESIndent::LMarg << "unix socket:" << endl ;
	BESIndent::Indent() ;
	_us->dump( strm ) ;
	BESIndent::UnIndent() ;
    }
    else
    {
	strm << BESIndent::LMarg << "unix socket: null" << endl ;
    }
    if( _ps )
    {
	strm << BESIndent::LMarg << "ppt server:" << endl ;
	BESIndent::Indent() ;
	_ps->dump( strm ) ;
	BESIndent::UnIndent() ;
    }
    else
    {
	strm << BESIndent::LMarg << "ppt server: null" << endl ;
    }
    BESModuleApp::dump( strm ) ;
    BESIndent::UnIndent() ;
}

int
main( int argc, char **argv )
{
    try
    {
	ServerApp app ;
	return app.main( argc, argv ) ;
    }
    catch( BESException &e )
    {
	cerr << "Caught unhandled exception: " << endl ;
	cerr << e.get_message() << endl ;
	return 1 ;
    }
    catch( ... )
    {
	cerr << "Caught unhandled, unknown exception" << endl ;
	return 1 ;
    }
    return 0 ;
}

