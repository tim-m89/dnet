//
// $Id: pragma.cpp 24923 2009-08-06 04:26:28Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu
//
#include <cassert>
#include <string>
#include <vector>
#include <sstream>
#include "attrib.h"
#include "expression.h"
#include "lexer.h"
#include "module.h"
#include "backend.net/array.h"
#include "backend.net/backend.h"
#include "backend.net/utils.h"
#include "frontend.net/externassembly.h"
#include "frontend.net/pragma.h"

using namespace std;

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}



PragmaScope::PragmaScope(PragmaDeclaration* decl, Dsymbol* parent, StringExp* exp)
    : module(NULL)
{
    this->loc = decl->loc;
    members = decl->decl;

    const size_t len = exp->length();
    string s(len, ' ');
    for (size_t i = 0; i != len; ++i)
    {
        s[i] = exp->charAt(i);
    }
	string assemblyName("");
	string assemblyVersion("");
	string assemblyToken("");
	vector<string> kv = split(s, ',');
	vector<string>::iterator itr;
	for ( itr = kv.begin(); itr != kv.end(); ++itr )
	{
		size_t pos = itr->find('=');
		if(pos==string::npos)
		{
			assemblyName = *itr;
		}
		else
		{
			string name = itr->substr(0,pos);
			string value = itr->substr(pos+1);
			if(name==" Version")
			{
				assemblyVersion = value;
			}
			else if(name==" PublicKeyToken")
			{
				assemblyToken = value;
			}
		}
	}
    if (decl->ident == Lexer::idPool("assembly"))
    {
        whatKind = pragma_assembly;
        if (!parent || !parent->isModule())
        {
            error(loc, "nested assembly pragma not allowed");
        }

		BackEnd::instance().addExtern(ExternAssembly(assemblyName, assemblyVersion, assemblyToken));
        assert(!c_ident);
        c_ident = Lexer::idPool(assemblyName.c_str());

        module = parent->isModule();

        string tmp;
        for (Dsymbol* sym = parent; sym; sym = sym->parent)
        {
            if (!tmp.empty())
                tmp = '.' + tmp;
            tmp = sym->toChars() + tmp;
        }
        assemblyName = "[" + assemblyName + "]" + tmp;
    }
    ident = Lexer::idPool(assemblyName.c_str());
}


void PragmaScope::toObjFile(int multiobj)
{
}


void PragmaScope::setScope(Scope* sc)
{
    ArrayAdapter<Dsymbol*> syms(members);
    for (ArrayAdapter<Dsymbol*>::iterator i = syms.begin(); i != syms.end(); ++i)
    {
        DEREF(*i).setScope(sc);
    }
}


static void semantic(PragmaScope* pragma, Scope* sc, Array* dsymbols)
{
    ArrayAdapter<Dsymbol*> syms(dsymbols);

    for (ArrayAdapter<Dsymbol*>::iterator i = syms.begin(); i != syms.end(); ++i)
    {
        Dsymbol& dsym = DEREF(*i);
        dsym.parent = pragma;
        if (AttribDeclaration* attr = dsym.isAttribDeclaration())
        {
            semantic(pragma, sc, attr->decl);
        }
        else 
        {
            dsym.semantic(sc);
        }
    }
}


void PragmaScope::semantic(Scope* sc)
{
    ::semantic(this, sc, members);
}


PragmaScope* inPragmaAssembly(Dsymbol* sym)
{
    PragmaScope* pragmaScope = NULL;
    if (sym)
    {
        pragmaScope = sym->isPragmaScope();
        if (!pragmaScope)
        {
            return inPragmaAssembly(sym->toParent());
        }
        if (pragmaScope->kind() != PragmaScope::pragma_assembly)
        {
            pragmaScope = NULL;
        }
    }
    return pragmaScope;
}
