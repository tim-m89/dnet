// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: variable.cpp 24625 2009-07-31 01:05:55Z unknown $
//
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
#include "declaration.h"
#include "backend.net/type.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h"


Variable::Variable(VarDeclaration& decl, Type* type) 
    : decl_(decl)
    , type_(type)
    , name_(NULL)
{
}


Type* Variable::getType()
{
    return type_ ? type_ : decl_.type;
}


void Variable::setType(Type* t, Loc loc)
{
    // if the type has been set and the variable is in use (refCount != 0)
    // then we cannot change its type because we may have already generated 
    // code that uses the type
    if (type_ && type_ != t && refCount())
    {
        const char* to = "";
        if (t->ctype && t->ctype->isSliceType())
        {
            to = " to array slice";
        }
        error(loc, "cannot change type of %s%s", getName(), to);
    }
    type_ = t;
}


bool Variable::isStatic() const
{
    return decl_.storage_class & STCstatic;
}


const char* Variable::getName()
{
    return name_ ? name_->toChars() : decl_.toChars();
}


StructDeclaration* Variable::isStructMember()
{
    if (Dsymbol* parent = decl_.toParent())
    {
        return parent->isStructDeclaration();
    }
    return NULL;
}


Method* Variable::isMethod()
{
    if (DelegateType* dt = toILType(decl_).isDelegateType())
    {
        return dt->getMethod();
    }
    return NULL;
}


bool Variable::isRefType()
{
    return !decl_.isRef() && (DEREF(getType()).ty == Treference);
}


bool Variable::hasClosureReference()
{
    bool result = false;
    VarDeclaration& decl = getDecl();
    if (decl.nestedrefs.dim &&
        (decl.isRef() || (!decl.isParameter() /* && getType()->isscalar() */))
        )
    {
        decl.storage_class |= STCpinned;
        result = true;
    }
    return result;
}


ArrayElem::ArrayElem(block& blk, VarDeclaration& decl, Type* type) 
    : BaseArrayElem(blk, decl, type)
{
}



elem* ArrayElem::load(block& blk, Loc* loc, bool addr)
{
    loadRefParent(*this, blk, loc);
    return ArrayElemAccess::load(blk, *this, loc, addr);
}


elem* ArrayElem::store(block& blk, Loc* loc)
{
    loadRefParent(*this, blk, loc);
    blk.swapTail();
    if (!loc)
    {
        loc = &blk.loc();
    }
    TYPE& elemTYPE = toILType(*loc, getType());
    return ArrayElemAccess::store(blk, elemTYPE, loc);
}


elem* ArrayElem::emitILAsm(std::wostream& out)
{
    return NULL;
}


#pragma region VarAdapter
VarAdapter::VarAdapter(VarDeclaration& decl, elem& e)
    : Variable(decl)
    , elem_(e)
{
    assert(!e.isVariable());
}


elem* VarAdapter::emitILAsm(std::wostream&)
{
    return NULL;
}


elem* VarAdapter::load(block& blk, Loc*, bool addr)
{
    assert(!addr);
    return blk.add(*new SharedElem(elem_));
}


elem* VarAdapter::store(block& blk, Loc* loc)
{
    if (!loc) loc = &blk.loc();
    BackEnd::fatalError(*loc, "logic error, cannot store VarAdapter");
    return NULL;
}

#pragma endregion VarAdapter