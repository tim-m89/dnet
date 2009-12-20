// $Id: arraylen.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include "mtype.h"
#include "backend.net/backend.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/arraylen.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/types.h"

using namespace std;


ArrayLen::ArrayLen(Variable& a, elem& e, Type* type)
    : Variable(a.getDecl(), type)
    , op_(NONE)
    , array_(&e)
    , sliceHandled_(false)
{
}


ArrayLen::ArrayLen(Variable& a, Type* type)
    : Variable(a.getDecl(), type)
    , op_(NONE)
    , array_(&a)
    , sliceHandled_(false)
{
}


ArrayLen::ArrayLen(const ArrayLen& other, Operation op)
    : Variable(other)
    , op_(op)
    , array_(other.array_)
    , sliceHandled_(other.sliceHandled_)
{
    assert(op != NONE);
}


TYPE& ArrayLen::getTYPE()
{
    if (Variable* v = DEREF(array_).isVariable())
    {
        return toILType(*v);
    }
    throw logic_error("variable expected in array length expression");
}


elem* ArrayLen::loadArray(block& blk, Loc* loc, bool byAddr)
{
    if (Variable* v = DEREF(array_).isVariable())
    {
        return v->load(blk, loc, byAddr);
    }
    throw logic_error("variable expected in array length expression");
}


elem* ArrayLen::storeArray(block& blk, Loc* loc)
{
    if (Variable* v = DEREF(array_).isVariable())
    {
        return v->store(blk, loc);
    }
    throw logic_error("variable expected in array length expression");
}


elem* ArrayLen::load(block& blk, Loc* loc, bool addr)
{
    bool doLoad = true;

    if (ASlice* slice = DEREF(array_).isASlice())
    {
        // Handle expressions such as: a[low..high].length
        if (slice->lower() && slice->upper())
        {
            doLoad = false;
            slice->removeArray();
            assert(blk.back() == this);
            blk.swapTail();
            blk.pop_back();
            Instruction::create(blk, IL_sub);
            Instruction::create(blk, IL_neg);
        }
        sliceHandled_ = true;
    }
    else if (SliceType* sliceType = getTYPE().isSliceType())
    {
        //slices are implemented as System.ArraySegment(T), which is
        //a valuetype (struct), we have to reference them by address
        if (op_ == NONE)
        {   // assuming that the object is loaded by value, 
            // discard it and re-load it by addr
            assert(blk.back() == this);
            blk.swapTail();
            assert(blk.back() && 
               (blk.back()->isLoad() || blk.back()->isBlock()));
            blk.pop_back();
        }
        loadArray(blk, loc, true);
        blk.add(*PropertyGetter::create(*sliceType, "Count", "int32"));
        doLoad = false;
        sliceHandled_ = true;
    }
    if (op_ == NONE)
    {
        op_ = LOAD;
        return this;
    }
    if (doLoad)
    {
        loadArray(blk, loc, addr);
    }
    return blk.add(*new ArrayLen(*this, LOAD));
}


elem* ArrayLen::store(block& blk, Loc* loc)
{
    SliceType* sliceType = getTYPE().isSliceType();
    if (sliceType)
    {
        assert(blk.back() == this);
        blk.swapTail();
        blk.pop_back();
        loadArray(blk, loc, true);
        blk.swapTail();
    }
    elem* result = this;
    if (op_ == NONE)
    {
        op_ = STORE;
        if (!sliceType)
        {
            storeArray(blk, loc);
        }
    }
    else
    {
        loadArray(blk, loc);
        blk.add(*new ArrayLen(*this, STORE));
        if (!sliceType)
        {
            result = storeArray(blk, loc);
        }
    }
    return result;
}


//Handle array slices at emit time

void ArrayLen::handleSlice(ArrayType& aType, wostream& out)
{
    assert(!sliceHandled_);
    indent(out);

    if (Variable* v = array_->isVariable())
    {
        // assume that the slice is loaded by value on the eval stack,
        // discard it and load its address
        out << IL_pop << '\n';
        Assembly& assembly = BackEnd::instance().getAssembly();
        elem* temp = v->load(assembly, NULL, true);
        indent(out);
        DEREF(temp).emitIL(out);
        indent(out);
        assembly.add(*PropertyGetter::create(aType, "Count", "int32"))->emitIL(out);
    }
    else
    {
        out << "call int32 [dnetlib]runtime.Slice::Size<";
        out << aType.elemTYPE().name();
        out << ">($SLICE<!!0>)\n";
    }
    sliceHandled_ = true;
}


elem* ArrayLen::emitILAsm(wostream& out)
{
    ArrayType& aType = getArrayType(getTYPE());
    switch (op_)
    {
    case LOAD:
        if (aType.isSliceType())
        {
            if (!sliceHandled_)
            {   // this may happen if we changed a variable's type from
                // array to slice AFTER the ArrayLen object was created
                handleSlice(aType, out);
            }
        }
        else if (!DEREF(array_).isASlice())
        {
            indent(out) << IL_ldlen << "\n";
        }
        else
        {
            assert(sliceHandled_);
        }
        break;

    case STORE:
        indent(out);
        if (aType.isSliceType())
        {
            out << "call void [dnetlib]runtime.Slice::Resize<";
            out << aType.elemTYPE().name();
            out << ">(int32, $SLICE<!!0>&)\n";
        }
        else
        {
            out << "call !!0[] [dnetlib]runtime.Array::Resize<";
            out << aType.elemTYPE().name() << ">(int32, !!0[])\n";
        }
        break;
    }
    return this;
}


int32_t ArrayLen::getStackDelta() const
{
    return op_ == STORE ? -1 : 0;
}


int32_t ArrayLen::getMaxStackDelta() const 
{
    return 1;
}
