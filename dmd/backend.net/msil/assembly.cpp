// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: assembly.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu

/*****
This license governs use of the accompanying software. If you use the software, you
accept this license. If you do not accept the license, do not use the software.

1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the
same meaning here as under U.S. copyright law.
A "contribution" is the original software, or any additions or changes to the software.
A "contributor" is any person that distributes its contribution under this license.
"Licensed patents" are a contributor's patent claims that read directly on its contribution.

2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, 
prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, 
each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made,
use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the 
contribution in the software.

3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software,
your patent license from such contributor to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark,
and attribution notices that are present in the software.
(D) If you distribute any portion of the software in source code form, you may do so only under this license by 
including a complete copy of this license with your distribution. If you distribute any portion of the software 
in compiled or object code form, you may only do so under a license that complies with this license.
(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties,
guarantees or conditions. You may have additional consumer rights under your local laws which this license cannot change.
To the extent permitted under your local laws, the contributors exclude the implied warranties of merchantability,
fitness for a particular purpose and non-infringement.
*****/
#include<iostream>
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/typedef.h"

using namespace std;


Assembly::Assembly(const char* name)
    : block(NULL, loc_, 0)
    , typedefs_(NULL, loc_, 0)
    , cctor_(NULL, loc_, 1)
    , hasSliceTypedef_(false)
{
    loc_.linnum = 0;
    if (name)
    {
        name_.assign(name);
        // Loc is not const-correct!
        loc_.filename = const_cast<char*>(name_.c_str());
    }
}


Assembly::~Assembly()
{
}


void Assembly::emitEnd(wostream& out)
{
    emitDelegateTypes(out);

    out << '\n';
    if (!cctor_.empty())
    {
        cctor_.setUseBrackets(true);
        cctor_.addRet(&loc_);
        indent(out) << ".method static private void .cctor()\n";
        cctor_.emitIL(out);
    }
}


elem* Assembly::emitILAsm(wostream& out)
{
    for (set<ExternAssembly>::const_iterator i = extern_.begin(); i != extern_.end(); ++i)
    {
        //todo: version, guid, etc
		out << i->toString().c_str(); //".assembly extern '" << i->name().c_str() << "' {}\n";
    }
    out << ".assembly extern dnetlib {}\n";
    out << ".assembly ";
    emitILAttribs(out);
    if (name_.empty())
    {
        out << "__anonymous";
    }
    else
    {
        out << '\'' << name_.c_str() << '\'';
    }
    out << " {";
    emitILDecls(out);
    out << "}\n";
    typedefs_.emitIL(out);
    return this;
}


void Assembly::emitILAttribs(wostream&)
{
}


void Assembly::emitILDecls(wostream&)
{
}


//introduce a shorthand for array slices, to improve the readability of the 
//generated IL code; it also provides us with some flexibility should we need 
//to change the implementation of slices

void Assembly::addSliceTypedef()
{
    if (!hasSliceTypedef_)
    {
        hasSliceTypedef_ = true;

        //typedefs_.add(*new Typedef("valuetype [mscorlib]System.ArraySegment`1", "$SLICE"));
        //PEVERIFY is not happy when using the .typedef TODO: figure out why
        typedefs_.add(*new DefineDirective("valuetype [mscorlib]System.ArraySegment`1", "$SLICE"));
    }
}


void Assembly::addModuleStaticCtor(Method& method)
{
    //assert(method.funcDecl().isStaticConstructor());
    CallInstruction::create(cctor_, IL_call, method);
}


bool Assembly::addExtern(const ExternAssembly& externAssembly)
{
    return extern_.insert(externAssembly).second;
}
