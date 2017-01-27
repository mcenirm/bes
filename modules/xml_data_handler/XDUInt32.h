
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of xml_data_handler, software which can return an XML data
// representation of the data read from a DAP server.

// Copyright (c) 2010 OPeNDAP, Inc.
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

// Authors:
//      jhrg,jimg       James Gallagher <jgallagher@gso.uri.edu>

// XDUInt32 interface. See XDByte.h for more info.
//
// 3/12/98 jhrg

#ifndef _xduint32_h
#define _xduint32_h 1

#include "UInt32.h"
#include "XDOutput.h"

class XDUInt32: public libdap::UInt32, public XDOutput {
public:
    XDUInt32(const string &n) : UInt32( n ) {}
    XDUInt32( UInt32 *bt ) : UInt32( bt->name() ), XDOutput( bt ) {
        set_send_p(bt->send_p());
    }
    virtual ~XDUInt32() {}

    virtual BaseType *ptr_duplicate();
};

#endif

