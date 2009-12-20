// $Id: aggr.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu

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
#include <cstdlib>
#include "aggregate.h"
#include "declaration.h"
#include "backend.net/array.h"
#include "backend.net/type.h"
#include "backend.net/msil/aggr.h"
#include "backend.net/msil/member.h"

#define DEFAULT_MAXSTACK    8
using namespace std;



Aggregate* isAggregateField(VarDeclaration& dsym)
{
    if (AggregateDeclaration* decl = dsym.isMember())
    {
        if (Symbol* s = decl->csym)
        {
            return s->isAggregate();
        }
    }
    return NULL;
}


Aggregate::Aggregate(AggregateDeclaration& decl, unsigned depth)
    : decl_(decl)
    , block_(NULL, decl.loc, depth)
    , staticInit_(NULL, decl.loc, depth)
    , init_(NULL, decl.loc, depth + 1) // initializers block
    , ctorDelegationGuard_(NULL)
{
}


bool Aggregate::isNested() const
{
    if (Dsymbol* parent = decl_.toParent())
    {
        return decl_.isNested() && parent->isAggregateDeclaration();
    }
    return false;
}


void Aggregate::generateMembers()
{
    ArrayAdapter<Dsymbol*> members(decl_.members);
    for (ArrayAdapter<Dsymbol*>::iterator i = members.begin(); i != members.end(); ++i)
    {
        if (!DEREF(*i).csym)
        {
            (*i)->toObjFile(0);
        }
    }
}


void Aggregate::emitDefaultStaticCtor(elem& e, wostream& out)
{
    assert(!staticInit_.empty());
    unsigned margin = e.depth() + 1;
    out << "\n";
    e.indent(out, margin) << "// default class ctor, compiler generated\n";
    e.indent(out, margin) << ".method public static void .cctor()\n";
    e.indent(out, margin) << "{\n";
    {
        ++margin;
        e.indent(out, margin) << ".maxstack " << staticInit_.getMaxStackDelta() << "\n";
        staticInit_.setDepth(margin);
        staticInit_.emitIL(out);
        e.indent(out, margin) << "ret\n";
        --margin;
    }
    e.indent(out, margin) << "}\n";
}


void Aggregate::emitInvariantCall(wostream& out, unsigned margin)
{
    //precondition
    assert (decl_.inv && global.params.useInvariants);
    if (margin == 0)
    {
        margin = this->depth() + 2; //indentation depth
    }
    indent(out, margin) << "ldarg.0\t// load 'this'\n";
    indent(out, margin);
    if (decl_.isStructDeclaration())
    {
        out << "call ";
    }
    else
    {
        out << "callvirt ";
    }
    out << "instance void " << className(decl_) << "::'" << decl_.inv->toChars() << "'()\n";
}


void Aggregate::emitDefaultTors(wostream& out)
{
    if (!decl_.isInterfaceDeclaration())
    {
        if (!decl_.defaultCtor)
        {
            emitDefaultCtor(out);
        }
        if (!decl_.dtor  && decl_.inv && global.params.useInvariants)
        {
            emitDefaultDtor(out);
        }
    }
}


void Aggregate::emitDefaultCtor(wostream& out)
{
    unsigned margin = this->depth() + 1;
    indent(out, margin) << "// default ctor, compiler-generated\n";
    indent(out, margin) << ".method public hidebysig instance void .ctor()\n";
    indent(out, margin) << "{\n";
    //indent(out, margin + 1) << ".maxstack " << DEFAULT_MAXSTACK << "\n";
    assert(getInitBlock().getMaxStackDelta() <= DEFAULT_MAXSTACK);
    getInitBlock().emitIL(out);
    ++margin;
    emitBase(".ctor", out, margin);
    if (decl_.inv && global.params.useInvariants)
    {
        emitInvariantCall(out, margin);
    }
    indent(out, margin) << "ret\n";
    --margin;
    indent(out, margin) << "}\n";
}


void Aggregate::emitDefaultDtor(wostream& out)
{
    unsigned margin = this->depth() + 1;
    indent(out, margin) << "// compiler-generated dtor\n";
    indent(out, margin) << ".method public virtual hidebysig instance void ";
    if (decl_.isClassDeclaration())
    {
        out << "__dtor()\n";
    }
    else
    {
        out << "Finalize()\n";
    }
    indent(out, margin) << "{\n";
    ++margin;
    //indent(out, margin) << ".maxstack " << DEFAULT_MAXSTACK << "\n";
    indent(out, margin) << ".try\n";
    indent(out, margin) << "{\n";
    emitInvariantCall(out, margin + 1);
    Identifier* label = Identifier::generateId("L_dtor_");
    indent(out, margin + 1) << "leave " << label->toChars() << "\n";
    indent(out, margin) << "}\n";
    indent(out, margin) << "finally\n";
    indent(out, margin) << "{\n";
    ++margin;
    emitBase("__dtor", out, margin);
    indent(out, margin) << "endfinally\n";
    --margin;
    indent(out, margin) << "}\n";
    indent(out, margin - 1) << label->toChars() << ":\n";
    indent(out, margin) << "ret\n";
    --margin;
    indent(out, margin) << "}\n";
}


Variable& Aggregate::getCtorDelegationGuard()
{
    if (!ctorDelegationGuard_)
    {
        VarDeclaration* decl = new VarDeclaration(0, Type::tbool, Lexer::idPool("$in_ctor"), 0);
        decl->parent = &getDecl();
        
        Field* f = Field::create(*decl, false);
        getBlock().add(*f);
        ctorDelegationGuard_ = f;
    }
    return *ctorDelegationGuard_;
}
