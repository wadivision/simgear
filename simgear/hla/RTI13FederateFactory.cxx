// Copyright (C) 2011 - 2012  Mathias Froehlich - Mathias.Froehlich@web.de
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#ifdef HAVE_CONFIG_H
#  include <simgear_config.h>
#endif

#include <simgear/compiler.h>

#include "RTI13FederateFactory.hxx"

#include "RTI13Federate.hxx"

namespace simgear {

RTI13FederateFactory::RTI13FederateFactory()
{
    _registerAtFactory();
}

RTI13FederateFactory::~RTI13FederateFactory()
{
}

RTIFederate*
RTI13FederateFactory::create(const std::string& name, const std::list<std::string>& stringList) const
{
    if (name != "RTI13")
        return 0;
    return new RTI13Federate(stringList);
}

const SGSharedPtr<RTI13FederateFactory>&
RTI13FederateFactory::instance()
{
    static SGSharedPtr<RTI13FederateFactory> federateFactory = new RTI13FederateFactory;
    return federateFactory;
}

}

