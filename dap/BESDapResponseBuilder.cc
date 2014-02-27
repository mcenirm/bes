// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2011 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#include "config.h"

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <uuid/uuid.h>  // used to build CID header value for data ddx

#ifndef WIN32
#include <sys/wait.h>
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <cstring>
#include <ctime>

//#define DODS_DEBUG

#include <DAS.h>
#include <DDS.h>
#include <ConstraintEvaluator.h>
#include <DDXParserSAX2.h>
#include <Ancillary.h>
#include <XDRStreamMarshaller.h>
#include <XDRFileUnMarshaller.h>

#include <DMR.h>
#include <XMLWriter.h>
#include <D4AsyncUtil.h>
#include <D4StreamMarshaller.h>
#include <chunked_ostream.h>
#include <chunked_istream.h>
#include <D4CEDriver.h>

#include <debug.h>
#include <mime_util.h>	// for last_modified_time() and rfc_822_date()
#include <escaping.h>
#include <util.h>
#ifndef WIN32
#include <SignalHandler.h>
#include <EventHandler.h>
#include <AlarmHandler.h>
#endif

#include "BESDapResponseBuilder.h"
#include "BESContextManager.h"
#include "BESDapResponseCache.h"
#include "BESStoredDapResultCache.h"
#include "BESDebug.h"

//#define CRLF "\r\n"             // Change here, expr-test.cc
#define DAP_PROTOCOL_VERSION "3.2"

const std::string CRLF = "\r\n";             // Change here, expr-test.cc
const int chunk_size = 4096;
const int8_t big_endian = 0x01;
const int8_t little_endian = 0x00;

using namespace std;
using namespace libdap;

/** Called when initializing a ResponseBuilder that's not going to be passed
 command line arguments. */
void BESDapResponseBuilder::initialize()
{
    // Set default values. Don't use the C++ constructor initialization so
    // that a subclass can have more control over this process.
    d_dataset = "";
    d_ce = "";
    d_btp_func_ce = "";
    d_timeout = 0;

    d_default_protocol = DAP_PROTOCOL_VERSION;

    d_response_cache = 0;
}

/** Lazy getter for the ResponseCache. */
BESDapResponseCache *
BESDapResponseBuilder::responseCache()
{
	// cerr << "***** BESDapResponseBuilder::responseCache() - BEGIN" << endl ;

	if (!d_response_cache)
		d_response_cache =  BESDapResponseCache::get_instance();

	// cerr << "***** BESDapResponseBuilder::responseCache() Got BESDapResponseCache instance: " << endl << *d_response_cache << endl;
	// cerr << "***** BESDapResponseBuilder::responseCache() - END" << endl ;

	return d_response_cache;
}

BESDapResponseBuilder::~BESDapResponseBuilder()
{
	if (d_response_cache) delete d_response_cache;

	// If an alarm was registered, delete it. The register code in SignalHandler
	// always deletes the old alarm handler object, so only the one returned by
	// remove_handler needs to be deleted at this point.
	delete dynamic_cast<AlarmHandler*>(SignalHandler::instance()->remove_handler(SIGALRM));
}

/** Return the entire constraint expression in a string.  This
 includes both the projection and selection clauses, but not the
 question mark.

 @brief Get the constraint expression.
 @return A string object that contains the constraint expression. */
string BESDapResponseBuilder::get_ce() const
{
    return d_ce;
}

/** Set the constraint expression. This will filter the CE text removing
 * any 'WWW' escape characters except space. Spaces are left in the CE
 * because the CE parser uses whitespace to delimit tokens while some
 * datasets have identifiers that contain spaces. It's possible to use
 * double quotes around identifiers too, but most client software doesn't
 * know about that.
 *
 * @@brief Set the CE
 * @param _ce The constraint expression
 */
void BESDapResponseBuilder::set_ce(string _ce)
{
    d_ce = www2id(_ce, "%", "%20");
}

/** The ``dataset name'' is the filename or other string that the
 filter program will use to access the data. In some cases this
 will indicate a disk file containing the data.  In others, it
 may represent a database query or some other exotic data
 access method.

 @brief Get the dataset name.
 @return A string object that contains the name of the dataset. */
string BESDapResponseBuilder::get_dataset_name() const
{
    return d_dataset;
}

/** Set the dataset name, which is a string used to access the dataset
 * on the machine running the server. That is, this is typically a pathname
 * to a data file, although it doesn't have to be. This is not
 * echoed in error messages (because that would reveal server
 * storage patterns that data providers might want to hide). All WWW-style
 * escapes are replaced except for spaces.
 *
 * @brief Set the dataset pathname.
 * @param ds The pathname (or equivalent) to the dataset.
 */
void BESDapResponseBuilder::set_dataset_name(const string ds)
{
    d_dataset = www2id(ds, "%", "%20");
}

/** Set the server's timeout value. A value of zero (the default) means no
 timeout.

 @see To establish a timeout, call establish_timeout(ostream &)
 @param t Server timeout in seconds. Default is zero (no timeout). */
void BESDapResponseBuilder::set_timeout(int t)
{
    d_timeout = t;
}

/** Get the server's timeout value. */
int BESDapResponseBuilder::get_timeout() const
{
    return d_timeout;
}

/** Use values of this instance to establish a timeout alarm for the server.
 If the timeout value is zero, do nothing.
*/
void BESDapResponseBuilder::establish_timeout(ostream &stream) const
{
#ifndef WIN32
    if (d_timeout > 0) {
        SignalHandler *sh = SignalHandler::instance();
        EventHandler *old_eh = sh->register_handler(SIGALRM, new AlarmHandler(stream));
        delete old_eh;
        alarm(d_timeout);
    }
#endif
}

void BESDapResponseBuilder::remove_timeout() const
{
	alarm(0);
}

static string::size_type
find_closing_paren(const string &ce, string::size_type pos)
{
	// Iterate over the string finding all ( or ) characters until the matching ) is found.
	// For each ( found, increment count. When a ) is found and count is zero, it is the
	// matching closing paren, otherwise, decrement count and keep looking.
	int count = 1;
	do {
		pos = ce.find_first_of("()", pos + 1);
		if (pos == string::npos)
			throw Error(malformed_expr, "Expected to find a matching closing parenthesis in " + ce);

		if (ce[pos] == '(')
			++count;
		else
			--count;	// must be ')'

	} while (count > 0);

	return pos;
}

/**
 *  Split the CE so that the server functions that compute new values are
 *  separated into their own string and can be evaluated separately from
 *  the rest of the CE (which can contain simple and slicing projection
 *  as well as other types of function calls).
 */
void
BESDapResponseBuilder::split_ce(ConstraintEvaluator &eval, const string &expr)
{
	DBG(cerr << "Entering ResponseBuilder::split_ce" << endl);
    string ce;
    if (!expr.empty())
        ce = expr;
    else
        ce = d_ce;

    string btp_function_ce = "";
    string::size_type pos = 0;
    DBG(cerr << "ce: " << ce << endl);

    // This hack assumes that the functions are listed first. Look for the first
    // open paren and the last closing paren to accommodate nested function calls
    string::size_type first_paren = ce.find("(", pos);
    string::size_type closing_paren = string::npos;
    if (first_paren != string::npos)
    	closing_paren = find_closing_paren(ce, first_paren); //ce.find(")", pos);


    while (first_paren != string::npos && closing_paren != string::npos) {
        // Maybe a BTP function; get the name of the potential function
        string name = ce.substr(pos, first_paren-pos);
        DBG(cerr << "name: " << name << endl);
        // is this a BTP function
        btp_func f;
        if (eval.find_function(name, &f)) {
            // Found a BTP function
            if (!btp_function_ce.empty())
                btp_function_ce += ",";
            btp_function_ce += ce.substr(pos, closing_paren+1-pos);
            ce.erase(pos, closing_paren+1-pos);
            if (ce[pos] == ',')
                ce.erase(pos, 1);
        }
        else {
            pos = closing_paren + 1;
            // exception?
            if (pos < ce.length() && ce.at(pos) == ',')
                ++pos;
        }

        first_paren = ce.find("(", pos);
        closing_paren = ce.find(")", pos);
    }

    DBG(cerr << "Modified constraint: " << ce << endl);
    DBG(cerr << "BTP Function part: " << btp_function_ce << endl);

    d_ce = ce;
    d_btp_func_ce = btp_function_ce;
}

/** This function formats and prints an ASCII representation of a
 DAS on stdout.  This has the effect of sending the DAS object
 back to the client program.

 @note This is the DAP2 attribute response.

 @brief Send a DAS.

 @param out The output stream to which the DAS is to be sent.
 @param das The DAS object to be sent.
 @param with_mime_headers If true (the default) send MIME headers.
 @return void
 @see DAS
 @deprecated */
void BESDapResponseBuilder::send_das(ostream &out, DAS &das, bool with_mime_headers) const
{
    if (with_mime_headers)
        set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), "2.0");

    das.print(out);

    out << flush;
}

/** Send the DAP2 DAS response to the given stream. This version of
 * send_das() uses the DDS object, assuming that it contains attribute
 * information. If there is a constraint expression associated with this
 * instance of ResponseBuilder, then it will be applied. This means
 * that CEs that contain server functions will populate the response cache
 * even if the server's initial request is for a DAS. This is different
 * from the older behavior of libdap where CEs were never evaluated for
 * the DAS response. This does not actually change the resulting DAS,
 * just the behavior 'under the covers'.
 *
 * @param out Send the response to this ostream
 * @param dds Use this DDS object
 * @param eval A Constraint Evaluator to use for any CE bound to this
 * ResponseBuilder instance
 * @param constrained Should the result be constrained
 * @param with_mime_headers Should MIME headers be sent to out?
 */
void BESDapResponseBuilder::send_das(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool constrained, bool with_mime_headers)
{
    // Set up the alarm.
    establish_timeout(out);
    dds.set_timeout(d_timeout);

    if (!constrained) {
        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), "2.0");

        dds.print_das(out);
        out << flush;

        return;
    }

    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        DDS *fdds = 0;
        string cache_token = "";

        if (responseCache()) {
            DBG(cerr << "Using the cache for the server function CE" << endl);
            fdds = responseCache()->cache_dataset(dds, d_btp_func_ce, this, &eval, cache_token);
        }
        else {
            DBG(cerr << "Cache not found; (re)calculating" << endl);
            eval.parse_constraint(d_btp_func_ce, dds);
            fdds = eval.eval_function_clauses(dds);
        }

        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdds->print_das(out);

        if (responseCache())
        	responseCache()->unlock_and_close(cache_token);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print_das(out);
    }

    out << flush;
}

/** This function formats and prints an ASCII representation of a
 DDS on stdout. Either an entire DDS or a constrained DDS may be sent.
 This function looks in the local cache and uses a DDS object there
 if it's valid. Otherwise, if the request CE contains server functions
 that build data for the response, the resulting DDS will be cached.

 @brief Transmit a DDS.
 @param out The output stream to which the DAS is to be sent.
 @param dds The DDS to send back to a client.
 @param eval A reference to the ConstraintEvaluator to use.
 @param constrained If this argument is true, evaluate the
 current constraint expression and send the `constrained DDS'
 back to the client.
 @param constrained If true, apply the constraint bound to this instance
 of ResponseBuilder
 @param with_mime_headers If true (default) send MIME headers.
 @return void
 @see DDS */
void BESDapResponseBuilder::send_dds(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool constrained,
        bool with_mime_headers)
{
    if (!constrained) {
        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print(out);
        out << flush;
        return;
    }

    // Set up the alarm.
    establish_timeout(out);
    dds.set_timeout(d_timeout);

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        string cache_token = "";
        DDS *fdds = 0;

        if (responseCache()) {
            DBG(cerr << "Using the cache for the server function CE" << endl);
            fdds = responseCache()->cache_dataset(dds, d_btp_func_ce, this, &eval, cache_token);
        }
        else {
            DBG(cerr << "Cache not found; (re)calculating" << endl);
            eval.parse_constraint(d_btp_func_ce, dds);
            fdds = eval.eval_function_clauses(dds);
        }

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdds->mark_all(false);

        eval.parse_constraint(d_ce, *fdds);

        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdds->print_constrained(out);

        if (responseCache())
        	responseCache()->unlock_and_close(cache_token);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print_constrained(out);
    }

    out << flush;
}


bool BESDapResponseBuilder::store_dap2_result(ostream &out, DDS &dds, ConstraintEvaluator &eval) {
    bool isStoreResultRequest = false;
    string serviceUrl = BESContextManager::TheManager()->get_context("store_result", isStoreResultRequest);
	BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - isStoreResultRequest="<< (isStoreResultRequest?"true":"false") << endl);

	if(isStoreResultRequest){

		D4AsyncUtil d4au;
		XMLWriter xmlWrtr;

		bool asyncAccepted = false;
	    string async_acceptable_delay = BESContextManager::TheManager()->get_context("async_accepted", asyncAccepted);
		BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - async_accepted="<< (asyncAccepted?"true":"false") << endl);

		if(asyncAccepted){

			/**
			 * Client accepts async responses so, woot! lets store this thing and tell them where to find it.
			 */
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - serviceUrl="<< serviceUrl << endl);

			BESStoredDapResultCache *resultCache = BESStoredDapResultCache::get_instance();
			string storedResultId="";
			storedResultId = resultCache->store_dap2_result(dds, get_ce(), this, &eval);

			BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - storedResultId='"<< storedResultId << "'" << endl);

			string targetURL = resultCache->assemblePath(serviceUrl,storedResultId);
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - targetURL='"<< targetURL << "'" << endl);

			D4AsyncUtil d4au;
			XMLWriter xmlWrtr;
			d4au.writeD4AsyncAccepted(xmlWrtr, 0, 0, targetURL);
			out << xmlWrtr.get_doc();
			out << flush;
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - sent DAP4 AsyncAccepted response" << endl);

		}
		else {
			/**
			 * Client didn't indicate a willingness to accept an async response
			 * So - we tell them that async is required.
			 */
			d4au.writeD4AsyncRequired(xmlWrtr, 0, 0);
			out << xmlWrtr.get_doc();
			out << flush;
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap2_result() - sent DAP4 AsyncRequired  response" << endl);
		}



	}
	return isStoreResultRequest;
}




/**
 * Build/return the BLOB part of the DAP2 data response.
 */
void BESDapResponseBuilder::serialize_dap2_data_dds(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool ce_eval)
{
	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap2_data_dds() - BEGIN" << endl);

	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap2_data_dds() - Serializing DataDDS to stream..." << endl);

	dds.print_constrained(out);
	out << "Data:\n";
	out << flush;

	XDRStreamMarshaller m(out);

	// Send all variables in the current projection (send_p())
	for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++){
		if ((*i)->send_p()) {
			(*i)->serialize(eval, dds, m, ce_eval);
		}
	}
	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap2_data_dds() - END" << endl);
}

/**
 * Serialize a DAP3.2 DataDDX to the stream "out".
 * This was originally intended to be used for DAP4.
 */
void BESDapResponseBuilder::serialize_dap2_data_ddx(ostream &out, DDS &dds, ConstraintEvaluator &eval,
        const string &boundary, const string &start, bool ce_eval)
{

	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap2_data_ddx() - BEGIN" << endl);

	// Write the MPM headers for the DDX (text/xml) part of the response
	libdap::set_mime_ddx_boundary(out, boundary, start, dods_ddx, x_plain);

	// Make cid
	uuid_t uu;
	uuid_generate(uu);
	char uuid[37];
	uuid_unparse(uu, &uuid[0]);
	char domain[256];
	if (getdomainname(domain, 255) != 0 || strlen(domain) == 0)
		strncpy(domain, "opendap.org", 255);

	string cid = string(&uuid[0]) + "@" + string(&domain[0]);
	// Send constrained DDX with a data blob reference
	dds.print_xml_writer(out, true, cid);

	// write the data part mime headers here
	set_mime_data_boundary(out, boundary, cid, dods_data_ddx /* old value dap4_data*/, x_plain);

	XDRStreamMarshaller m(out);

	// Send all variables in the current projection (send_p()). In DAP4,
	// all of the top-level variables are serialized with their checksums.
	// Internal variables are not.
	for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++) {
		if ((*i)->send_p()) {
			(*i)->serialize(eval, dds, m, ce_eval);
		}
	}
	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap2_data_ddx() - END" << endl);
}

/** Send the data in the DDS object back to the client program. The data is
 encoded using a Marshaller, and enclosed in a MIME document which is all sent
 to \c data_stream.

 @note This is the DAP2 data response.

 @brief Transmit data.
 @param dds A DDS object containing the data to be sent.
 @param eval A reference to the ConstraintEvaluator to use.
 @param data_stream Write the response to this stream.
 @param anc_location A directory to search for ancillary files (in
 addition to the CWD).  This is used in a call to
 get_data_last_modified_time().
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true.
 @return void */
void BESDapResponseBuilder::send_data(ostream &data_stream, DDS &dds, ConstraintEvaluator &eval, bool with_mime_headers)
{
	BESDEBUG("dap", "BESDapResponseBuilder::send_data() - BEGIN"<< endl);

	// Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        BESDEBUG("dap", "BESDapResponseBuilder::send_data() - Found function(s) in CE: " << d_btp_func_ce << endl);
        string cache_token = "";
        DDS *fdds = 0;

        if (responseCache()) {
        	BESDEBUG("dap", "BESDapResponseBuilder::send_data() - Using the cache for the server function CE" << endl);
            fdds = responseCache()->cache_dataset(dds, d_btp_func_ce, this, &eval, cache_token);
        }
        else {
        	BESDEBUG("dap", "BESDapResponseBuilder::send_data() - Cache not found; (re)calculating" << endl);
            eval.parse_constraint(d_btp_func_ce, dds);
            fdds = eval.eval_function_clauses(dds);
        }

        BESDEBUG("dap", "constrained DDS: " << endl; fdds->print_constrained(cerr));

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdds->mark_all(false);

        eval.parse_constraint(d_ce, *fdds);

        fdds->tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

        if (fdds->get_response_limit() != 0 && fdds->get_request_size(true) > fdds->get_response_limit()) {
            string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                    + "KB is too large; requests for this user are limited to "
                    + long_to_string(dds.get_response_limit() / 1024) + "KB.";
            throw Error(msg);
        }

        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        BESDEBUG("dap", cerr << "BESDapResponseBuilder::send_data() - About to call dataset_constraint" << endl);
    	if(!store_dap2_result(data_stream,dds,eval)){
			serialize_dap2_data_dds(data_stream, *fdds, eval, false);
    	}

        if (responseCache())
        	responseCache()->unlock_and_close(cache_token);

        delete fdds;
    }
    else {
    	BESDEBUG("dap", "BESDapResponseBuilder::send_data() - Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

        if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
            string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                    + "KB is too large; requests for this user are limited to "
                    + long_to_string(dds.get_response_limit() / 1024) + "KB.";
            throw Error(msg);
        }

        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

    	if(!store_dap2_result(data_stream,dds,eval)){
			serialize_dap2_data_dds(data_stream, dds, eval);
    	}
    }

    data_stream << flush;


	BESDEBUG("dap", "BESDapResponseBuilder::send_data() - END"<< endl);

}

/** Send the DDX response. The DDX never contains data, instead it holds a
 reference to a Blob response which is used to get the data values. The
 DDS and DAS objects are built using code that already exists in the
 servers.

 @note This is the DAP3.x metadata response; it is supported by most DAP2
 servers as well. The DAP4 DDX will contain types not present in DAP2 or 3.x

 @param dds The dataset's DDS \e with attributes in the variables.
 @param eval A reference to the ConstraintEvaluator to use.
 @param out Destination
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true. */
void BESDapResponseBuilder::send_ddx(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool with_mime_headers)
{
    if (d_ce.empty()) {
        if (with_mime_headers)
            set_mime_text(out, dods_ddx, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dds.print_xml_writer(out, false /*constrained */, "");
        //dds.print(out);
        out << flush;
        return;
    }

    // Set up the alarm.
    establish_timeout(out);
    dds.set_timeout(d_timeout);

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        string cache_token = "";
        DDS *fdds = 0;

        if (responseCache()) {
            DBG(cerr << "Using the cache for the server function CE" << endl);
            fdds = responseCache()->cache_dataset(dds, d_btp_func_ce, this, &eval, cache_token);
        }
        else {
            DBG(cerr << "Cache not found; (re)calculating" << endl);
            eval.parse_constraint(d_btp_func_ce, dds);
            fdds = eval.eval_function_clauses(dds);
        }

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdds->mark_all(false);

        eval.parse_constraint(d_ce, *fdds);

        if (with_mime_headers)
            set_mime_text(out, dods_ddx, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdds->print_constrained(out);

        if (responseCache())
        	responseCache()->unlock_and_close(cache_token);

        delete fdds;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dods_ddx, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        //dds.print_constrained(out);
        dds.print_xml_writer(out, true, "");
    }

    out << flush;
}


void BESDapResponseBuilder::send_dmr(ostream &out, DMR &dmr, ConstraintEvaluator &/*eval*/, bool with_mime_headers, bool constrained)
{
#if 0
    // earlier code, before the DAP4 CE parser was added to libdap. jhrg 11/28/13
	if (!constrained) {
        if (with_mime_headers)
            set_mime_text(out, dap4_dmr, x_plain, last_modified_time(d_dataset), dmr.dap_version());

        XMLWriter xml;
        dmr.print_dap4(xml, constrained);
        out << xml.get_doc();
        out << flush;
        return;
    }
#endif

	// If the CE is not empty, parse it. The projections, etc., are set as a side effect.
    // If the parser returns false, the expression did not parse. The parser may also
    // throw Error
    if (constrained && !d_ce.empty()) {
        D4CEDriver parser(&dmr);
        bool parse_ok = parser.parse(d_ce);
        if (!parse_ok)
            throw Error("Constraint Expression failed to parse.");
    }
    // with an empty CE, send everything. Even though print_dap4() and serialize()
    // don't need this, other code may depend on send_p being set. This may change
    // if DAP4 has a separate function evaluation phase. jhrg 11/25/13
    else if (constrained) {
        dmr.root()->set_send_p(true);
    }

    if (with_mime_headers) set_mime_text(out, dap4_dmr, x_plain, last_modified_time(d_dataset), dmr.dap_version());

    XMLWriter xml;
    dmr.print_dap4(xml, constrained && !d_ce.empty() /* true == constrained */);
    out << xml.get_doc() << flush;

	// FIXME Add support for constraints
#if 0
    // Set up the alarm.
    establish_timeout(out);
   // dds.set_timeout(d_timeout);

    // Split constraint into two halves
    split_ce(eval);

    // If there are functions, parse them and eval.
    // Use that DDS and parse the non-function ce
    // Serialize using the second ce and the second dds
    if (!d_btp_func_ce.empty()) {
        string cache_token = "";
        DMR *fdmr = 0;

        if (responseCache()) {
            DBG(cerr << "Using the cache for the server function CE" << endl);
            fdmr = responseCache()->cache_dataset(dmr, d_btp_func_ce, this, &eval, cache_token);
        }
        else {
            DBG(cerr << "Cache not found; (re)calculating" << endl);
            eval.parse_constraint(d_btp_func_ce, dmr);
            fdmr = eval.eval_function_clauses(dmr);
        }

        // Server functions might mark variables to use their read()
        // methods. Clear that so the CE in d_ce will control what is
        // sent. If that is empty (there was only a function call) all
        // of the variables in the intermediate DDS (i.e., the function
        // result) will be sent.
        fdmr->mark_all(false);

        eval.parse_constraint(d_ce, *fdmr);

        if (with_mime_headers)
            set_mime_text(out, dap4_dmr, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        fdmr->print_constrained(out);

        if (responseCache())
        	responseCache()->unlock_and_close(cache_token);

        delete fdmr;
    }
    else {
        DBG(cerr << "Simple constraint" << endl);

        eval.parse_constraint(d_ce, dmr); // Throws Error if the ce doesn't parse.

        if (with_mime_headers)
            set_mime_text(out, dap4_dmr, x_plain, last_modified_time(d_dataset), dmr.dap_version());

        dmr.print_constrained(out);
    }
#endif

    out << flush;
}

void BESDapResponseBuilder::send_dap4_data(ostream &out, DMR &dmr, ConstraintEvaluator &/*eval*/, bool with_mime_headers, bool filter)
{
#if 0
    // early code, before the D4 CE parser was added. jhrg 11/28/13
	try {
		// Set up the alarm.
		establish_timeout(out);

#if 0
		bool filter = eval.parse_constraint(d_ce, dmr); // Throws Error if the ce doesn't parse.
#endif
		if (dmr.response_limit() != 0 && dmr.request_size(true) > dmr.response_limit()) {
			string msg = "The Request for " + long_to_string(dmr.request_size(true) / 1024)
					+ "MB is too large; requests for this user are limited to "
					+ long_to_string(dmr.response_limit() / 1024) + "MB.";
			throw Error(msg);
		}

		if (with_mime_headers)
			set_mime_binary(out, dap4_data, x_plain, last_modified_time(d_dataset), dmr.dap_version());

	    // Write the DMR
	    XMLWriter xml;
	    dmr.print_dap4(xml, filter);

	    // the byte order info precedes the start of chunking
	    char byte_order = is_host_big_endian() ? big_endian : little_endian; // is_host_big_endian is in util.cc
	    out << byte_order << flush;

	    // now make the chunked output stream
	    chunked_ostream cos(out, chunk_size);
	    // using flush means that the DMR and CRLF are in the first chunk (if it is smaller than chunk_size)
	    cos << xml.get_doc() << CRLF << flush;

	    // Write the data, chunked with checksums
	    D4StreamMarshaller m(cos);
	    dmr.root()->serialize(m, dmr, /*eval,*/ filter);

		out << flush;

		remove_timeout();
	}
	catch (...) {
		remove_timeout();
		throw;
	}
#endif

	try {
        // Set up the alarm.
        establish_timeout(out);

        // If the CE is not empty, parse it. The projections, etc., are set as a side effect.
        // If the parser returns false, the expression did not parse. The parser may also
        // throw Error
        if (!d_ce.empty()) {
            D4CEDriver parser(&dmr);
            bool parse_ok = parser.parse(d_ce);
            if (!parse_ok)
                throw Error("Constraint Expression failed to parse.");
        }
        // with an empty CE, send everything. Even though print_dap4() and serialize()
        // don't need this, other code may depend on send_p being set. This may change
        // if DAP4 has a separate function evaluation phase. jhrg 11/25/13
        else {
            dmr.root()->set_send_p(true);
        }

        if (dmr.response_limit() != 0 && dmr.request_size(true) > dmr.response_limit()) {
            string msg = "The Request for " + long_to_string(dmr.request_size(true) / 1024)
                    + "MB is too large; requests for this user are limited to "
                    + long_to_string(dmr.response_limit() / 1024) + "MB.";
            throw Error(msg);
        }

        serialize_dap4_data(out, dmr, with_mime_headers, filter);

        remove_timeout();
    }
    catch (...) {
        remove_timeout();
        throw;
    }

}

/**
 * Serialize the DAP4 data response to the passed stream
 */
void BESDapResponseBuilder::serialize_dap4_data(std::ostream &out, libdap::DMR &dmr, bool with_mime_headers, bool filter)
{
	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap4_data() - BEGIN" << endl);

    if (with_mime_headers)
        set_mime_binary(out, dap4_data, x_plain, last_modified_time(d_dataset), dmr.dap_version());

    // Write the DMR
    XMLWriter xml;
    dmr.print_dap4(xml, !d_ce.empty());

#if BYTE_ORDER_PREFIX
    // the byte order info precedes the start of chunking
    char byte_order = is_host_big_endian() ? big_endian : little_endian; // is_host_big_endian is in util.cc
    out << byte_order << flush;
#endif
    // now make the chunked output stream; set the size to be at least chunk_size
    // but make sure that the whole of the xml plus the CRLF can fit in the first
    // chunk. (+2 for the CRLF bytes).
    chunked_ostream cos(out, max((unsigned int)CHUNK_SIZE, xml.get_doc_size()+2));

    // using flush means that the DMR and CRLF are in the first chunk.
    cos << xml.get_doc() << CRLF << flush;

    // Write the data, chunked with checksums
    D4StreamMarshaller m(cos);
    dmr.root()->serialize(m, dmr, !d_ce.empty());

    out << flush;


	BESDEBUG("dap", "BESDapResponseBuilder::serialize_dap4_data() - END" << endl);
}

bool BESDapResponseBuilder::store_dap4_result(ostream &out, libdap::DMR &dmr) {
    bool isStoreResultRequest = false;
    string serviceUrl = BESContextManager::TheManager()->get_context("store_result", isStoreResultRequest);
	BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - isStoreResultRequest="<< (isStoreResultRequest?"true":"false") << endl);

	if(isStoreResultRequest){
		D4AsyncUtil d4au;
		XMLWriter xmlWrtr;

		bool asyncAccepted = false;
	    string async_acceptable_delay = BESContextManager::TheManager()->get_context("async_accepted", asyncAccepted);
		BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - async_accepted="<< (asyncAccepted?"true":"false") << endl);

		if(asyncAccepted){

			/**
			 * Client accepts async responses so, woot! lets store this thing and tell them where to find it.
			 */
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - serviceUrl="<< serviceUrl << endl);

			BESStoredDapResultCache *resultCache = BESStoredDapResultCache::get_instance();
			string storedResultId="";
			storedResultId = resultCache->store_dap4_result(dmr, get_ce(), this);

			BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - storedResultId='"<< storedResultId << "'" << endl);

			string targetURL = resultCache->assemblePath(serviceUrl,storedResultId);
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - targetURL='"<< targetURL << "'" << endl);

			d4au.writeD4AsyncAccepted(xmlWrtr, 0, 0, targetURL);
			out << xmlWrtr.get_doc();
			out << flush;
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - sent AsyncAccepted" << endl);

		}
		else {
			/**
			 * Client didn't indicate a willingness to accept an async response
			 * So - we tell them that async is required.
			 */
			d4au.writeD4AsyncRequired(xmlWrtr, 0, 0);
			out << xmlWrtr.get_doc();
			out << flush;
			BESDEBUG("dap", "BESDapResponseBuilder::store_dap4_result() - sent AsyncAccepted" << endl);
		}

	}
	return isStoreResultRequest;
}




#if 0
void BESDapResponseBuilder::send_dap4_data(ostream &out, DMR &dmr, ConstraintEvaluator &/*eval*/,
        bool with_mime_headers, bool constrained)
{
// FIXME Fill in this placeholder - get DMR working first

	// Set up the alarm.
    establish_timeout(out);

    if (with_mime_headers)
        set_mime_binary(out, dap4_data, x_plain, last_modified_time(d_dataset), dmr.dap_version());

    out << flush;

#if 0
    eval.parse_constraint(d_ce, dmr); // Throws Error if the ce doesn't parse.
#endif
#if 0
    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                + "KB is too large; requests for this user are limited to "
                + long_to_string(dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }
#endif
    // Start sending the response...
#if 0
    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        // We could unique_ptr<DDS> here to avoid memory leaks if
        // dataset_constraint_ddx() throws an exception.
        DDS *fdds = eval.eval_function_clauses(dds);
        try {
            if (with_mime_headers)
                set_mime_multipart(out, boundary, start, dap4_data_ddx, x_plain, last_modified_time(d_dataset));
            out << flush;
            dataset_constraint_ddx(out, *fdds, eval, boundary, start);
        }
        catch (...) {
            delete fdds;
            throw;
        }
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_multipart(out, boundary, start, dap4_data_ddx, x_plain, last_modified_time(d_dataset));
        out << flush;
        dataset_constraint_ddx(out, dds, eval, boundary, start);
    }

    out << flush;

    if (with_mime_headers)
        data_stream << CRLF << "--" << boundary << "--" << CRLF;
#endif

    // FIXME Write a DMR version
    // dataset_constraint_ddx(out, dds, eval, boundary, start);
    out << flush;
}
#endif


#if 0
/** Send the data in the DDS object back to the client program. The data is
 encoded using a Marshaller, and enclosed in a MIME document which is all sent
 to \c data_stream.

 @note This is not the DAP4 data response, although that was the original
 intent.

 @brief Transmit data.
 @param dds A DDS object containing the data to be sent.
 @param eval A reference to the ConstraintEvaluator to use.
 @param data_stream Write the response to this stream.
 @param anc_location A directory to search for ancillary files (in
 addition to the CWD).  This is used in a call to
 get_data_last_modified_time().
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true.
 @return void */
void BESDapResponseBuilder::send_data_ddx(ostream & data_stream, DDS & dds, ConstraintEvaluator & eval, const string &start,
        const string &boundary, bool with_mime_headers)
{
	// Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                + "KB is too large; requests for this user are limited to "
                + long_to_string(dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        // We could unique_ptr<DDS> here to avoid memory leaks if
        // dataset_constraint_ddx() throws an exception.
        DDS *fdds = eval.eval_function_clauses(dds);
        try {
            if (with_mime_headers)
                set_mime_multipart(data_stream, boundary, start, dods_data_ddx, x_plain, last_modified_time(d_dataset));
            data_stream << flush;
            dataset_constraint_ddx(data_stream, *fdds, eval, boundary, start);
        }
        catch (...) {
            delete fdds;
            throw;
        }
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, dods_data_ddx, x_plain, last_modified_time(d_dataset));
        data_stream << flush;
        dataset_constraint_ddx(data_stream, dds, eval, boundary, start);
    }

    data_stream << flush;

    if (with_mime_headers)
        data_stream << CRLF << "--" << boundary << "--" << CRLF;
}
#endif

