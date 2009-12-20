// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: varargs.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include "arraytypes.h"
#include "mtype.h"
#include "backend.net/irstate.h"
#include "backend.net/type.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/varargs.h"
#include "backend.net/msil/variable.h"



VarArgs::VarArgs(IRState& irstate,
                 Expressions* expressions, 
                 Loc& loc,
                 unsigned offset,
                 bool box,
                 TYPE* elemTYPE
                 )
    : block(&irstate.getBlock(), loc, irstate.getBlock().depth())
    , offset_(offset)
    , args_(expressions)
{
    IRState::StateSaver autoState(irstate);
    irstate.setBlock(*this);
    assert(offset <= args_.size());
    const int32_t size = args_.size() - offset;
    Const<size_t>::create(*TYPE::Int32, size, *this, loc);

    for (size_t i = offset_, index = 0; i != args_.size(); ++i, ++index)
    {
        TYPE* type = elemTYPE ? elemTYPE : &toILType(loc, args_[i]->type);
        if (index == 0)
        {
            add(*NewArray::create(box ? *TYPE::Object : *type));
        }
        Instruction::create(*this, IL_dup, &loc);
        Const<size_t>::create(*TYPE::Int32, index, *this, loc);
        elem* arg = DEREF(args_[i]).toElem(&irstate);
        if (Variable* v = DEREF(arg).isVariable())
        {
            loadArray(*this, *v);
            type = &toILType(loc, v->getType());
            if (PointerType* pt = type->isReferenceType())
            {
                type = &pt->getPointedType();
            }
        }
        if (box && !type->isObjectType())
        {
            Box::create(*this, *type, &loc);
            Instruction::create(*this, IL_stelem_ref);
        }
        else
        {
            ArrayElemAccess::store(*this, *type);
        }
    }
}


elem* VarArgs::emitILAsm(std::wostream& out)
{
    IF_DEBUG(indent(out) << "// BEGIN varargs\n");
    elem* res = block::emitILAsm(out);
    IF_DEBUG(indent(out) << "// END varargs\n");
    return res;
}


int32_t VarArgs::getMaxStackDelta() const
{
    int32_t delta = block::getMaxStackDelta();
    return delta;
}



int32_t VarArgs::getStackDelta() const
{
    int32_t delta = block::getStackDelta();
    // At the end of the day there should be one entry 
    // left on the stack: the object array
    assert(delta == 1);

    return delta;
}
