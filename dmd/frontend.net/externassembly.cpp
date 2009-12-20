//
// $Id: externassembly.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2009 Cristian L. Vlasceanu
//
#include "frontend.net/externassembly.h"


bool operator<(const ExternAssembly& lhs, const ExternAssembly& rhs)
{
    if (lhs.name() < rhs.name())
    {
        return true;
    }
    return lhs.version() < rhs.version();
}

/**************************************
 * Output an extern block for the il assembly
 * in a format like:

 .assembly extern 'mscorlib'
 {
  .publickeytoken = (b7 7a 5c 56 19 34 e0 89 )
  .ver 2:0:0:0
 }
 */
std::string ExternAssembly::toString() const
{
	std::string vers(version_);
	size_t pos = vers.find(".");
	while(pos != std::string::npos)
	{
		vers.replace(pos, 1, ":");
		pos = vers.find(".");
	}

	std::string tok("");
	for(int i = 0; i<=14; i++)
	{
		tok += token_.substr(i, 2) + " ";
	}

	std::string s(".assembly extern '" + name_ + "'\n{\n  .publickeytoken = (" + tok + ")" +
		"\n  .ver " + vers + "\n}\n");

	return s;
}
