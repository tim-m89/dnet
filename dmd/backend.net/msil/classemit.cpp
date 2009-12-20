// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: classemit.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <iostream>
#include "aggregate.h"
#include "declaration.h"
#include "identifier.h"
#include "backend.net/block.h"
#include "backend.net/hiddenmethod.h"
#include "backend.net/irstate.h"
#include "backend.net/modemit.h"
#include "backend.net/type.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/member.h"

#define CLASS_ATTR(a) case c_##a: out << STRINGIZE(a); break;

using namespace std;
typedef ArrayAdapter<BaseClass*> Bases;


static wostream& operator<<(wostream& out, ClassAttr attr)
{
    switch (attr)
    {
        CLASS_ATTRIBS
    }
    return out;
}


Class::Class(ClassDeclaration& decl, unsigned depth)
    : Aggregate(decl, depth + 1)
{
    assert(decl.csym == NULL);
    decl.csym = this;

    IRState irstate(getBlock());
    irstate.setDecl(&decl);
#if 0
    Bases bases(decl.baseclasses);
    for (size_t i = 0, n = bases.size(); i != n; ++i)
    {
        DEREF(bases[i]).fillVtbl(&decl, NULL, 1);
    }
#endif
    generateMembers();
    checkHiddenFunctions();
    getBlock().generateMethods();
}



void Class::emitBase(const char* func, wostream& out, unsigned margin)
{
    if (ClassDeclaration* base = getDecl().baseClass)
    {
        indent(out, margin) << "ldarg.0\n";
        indent(out, margin) << "call instance void ";
        out << className(*base) << "::" << func << "()\n";
    }
}


elem* Class::emitILAsm(wostream& out)
{
    ClassDeclaration& decl = getDecl();
    IF_WIN_DEBUG(OutputDebugStringA((string(className(decl)) + "\n").c_str()));
    //emit class attributes and name
    emitHeader(indent(out << "\n") << ".class ");

    indent(out) << "{\n";
    getBlock().emitILAsm(out);

    if (!getStaticInitBlock().empty())
    {
        emitDefaultStaticCtor(*this, out);
    }
    //emit default ctor and dtor if not explicitly defined
    emitDefaultTors(out);
   
    indent(out) << "} // end of " << className(decl) << "\n\n";
    return this;
}


wostream& Class::emitAttr(wostream& out)
{
    ClassAttr layout = c_auto;
    ClassAttr visib = c_private;

    ClassDeclaration& decl = getDecl();
    if (isNested())
    {
        out << "nested ";
    }

    switch (decl.prot())
    {
    case PROTundefined:
    case PROTnone:
    case PROTprivate:
    case PROTpackage:
    case PROTprotected:
        break;

    case PROTpublic:
    case PROTexport:
        visib = c_public;
    }
    out << visib << ' ' << layout << ' ';

    if (decl.isInterfaceDeclaration())
    {
        out << "interface ";
    }
    if (decl.storage_class & STCtls)
    {
        out << "beforefieldinit ";
    }
    return out;
}


static void emitBase(ClassDeclaration* base, wostream& out)
{
    const char* baseObjName = className(DEREF(base));
    out << " extends " << baseObjName;
}


wostream& Class::emitBases(wostream& out)
{
    ClassDeclaration& decl = getDecl();
    Bases bases(decl.baseclasses);
    if (size_t n = bases.size())
    {
        ::emitBase(bases[0]->base, out);
        if (n > 1)
        {
            out << " implements ";
            for (size_t i = 1; i != n; ++i)
            {
                bases[i]->fillVtbl(&decl, NULL, 1);
                if (i > 1)
                {
                    out << ", ";
                }
                out << className(DEREF(bases[i]->base));
            }
        }
    }
    else if (ClassDeclaration* base = getDecl().baseClass)
    {
        ::emitBase(base, out);
    }
    return out << "\n";
}


wostream& Class::emitHeader(wostream& out)
{
    AggregateDeclaration& decl = getDecl();
    emitAttr(out) << (isNested() ? decl.toChars() : className(decl));
    return emitBases(out);
}


void Class::checkHiddenFunctions()
{
    ClassDeclaration& decl = getDecl();
    ArrayAdapter<Dsymbol*> funcs(decl.vtbl);
    for (ArrayAdapter<Dsymbol*>::iterator i = funcs.begin(), end = funcs.end(); i != end; ++i)
    {
        FuncDeclaration* fd = (*i)->isFuncDeclaration();
        if (!fd) continue;
        if (!fd->fbody && fd->isAbstract()) continue;
        if (decl.isFuncHidden(fd))
        {
            for (ArrayAdapter<Dsymbol*>::iterator j = funcs.begin(); j != end; ++j)
            {
                if (j == i)
                {
                    continue;
                }
                FuncDeclaration* other = (*j)->isFuncDeclaration();
                if (!other || !other->ident->equals(fd->ident))
                {
                    continue;
                }
                if (fd->leastAsSpecialized(other) || other->leastAsSpecialized(fd))
                {
                    fd = new FuncDeclaration(*fd);
                    fd->storage_class |= STCoverride;
                    Method* method = new Method(*fd, getBlock().depth());
                    fd->frequire = new HiddenMethodStatement(*method, *other);
                    getBlock().add(*method);
                    WARNING(other->loc, "%s hides %ls\n", fd->toChars(), method->getProto().c_str());
                }
            }
        }
    }
}


Aggregate& getAggregate(AggregateDeclaration& decl)
{
    Aggregate* a = NULL;
    if (!decl.csym)
    {
        decl.toObjFile(0);
    }
    if (Symbol* sym = decl.csym)
    {
        a = sym->isAggregate();
    }
    if (!a)
    {
        BackEnd::fatalError(decl.loc, "null aggregate in declaration");
    }
    return *a;
}


Class& getClass(ClassDeclaration& decl)
{
    Class* c = NULL;
    if (Symbol* sym = decl.csym)
    {
        c = sym->isClass();
    }
    if (!c)
    {
        BackEnd::fatalError(decl.loc, "null class in declaration");
    }
    return *c;
}
