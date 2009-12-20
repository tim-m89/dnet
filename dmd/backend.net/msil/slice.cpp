// $Id: slice.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <cstdlib>
#include "expression.h"
#include "mtype.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h"

using namespace std;


ASlice::ASlice(block* parent, SliceExp& exp, unsigned depth)
    : e_(NULL)
    , lwr_(exp.lwr)
    , upr_(exp.upr)
    , upperAdjusted_(false)
    , blk_(parent, exp.loc, depth)
{
}


bool ASlice::removeUpperAdjustment()
{
    if (upperAdjusted_)
    {
        upperAdjusted_ = false;
        blk_.pop_back();
        blk_.pop_back();
        blk_.pop_back();
        return true;
    }
    return false;
}


elem* ASlice::emitILAsm(std::wostream& out)
{
    return blk_.emitILAsm(out);
}


void ASlice::removeArray()
{
    assert(blk_.front() && blk_.front()->isBlock());
    blk_.pop_front();
}



/****************************************************/
#pragma region SliceFiller
SliceFiller::SliceFiller(size_t size, ASlice* slice, bool arrayCopy)
    : size_(size)
    , slice_(slice)
    , arrayCopy_(arrayCopy)
    , elemTYPE_(NULL)
{
} 


static inline bool hasSliceBounds(ASlice* slice)
{
    return slice && (slice->upper() || slice->lower());
}


elem* SliceFiller::emitILAsm(std::wostream& out)
{
    indent(out) << "call object [dnetlib]runtime.Slice::Fill";
    if (elemTYPE_)
    {
        out << "<" << elemTYPE_->name() << ">";
    }
    out << "(class [mscorlib]System.Array, ";
    if (hasSliceBounds(slice_))
    {
        out << "int32, int32, ";
    }
    if (arrayCopy_)
    {   //copy elements from another array
        assert(size_ == 0);
        out << "class [mscorlib]System.Array)\n";
    }
    else
    {
        const char* typeName = "object";
        if (elemTYPE_)
        {
            typeName = "!!0";
        }
        out << typeName;
        if (size_)
        {
            out << " []";
        }
        out << ")\n";
    }
    return this;
}


int32_t SliceFiller::getStackDelta() const 
{
    if (hasSliceBounds(slice_))
    {
        return -3; // pops 3 args, pushes result
    }
    return -1; // pops 2 args, pushes result
}
#pragma endregion


/****************************************************/
#pragma region NewArray
NewArray::NewArray(const ArrayType& at) : elemTYPE_(at.elemTYPE())
{
}


elem* NewArray::emitILAsm(wostream& out)
{
    indent(out) << "newarr " << elemTYPE_.name() << "\n";
    return this;
}
#pragma endregion


NewSlice::NewSlice(const ArrayType& type) : type_(type)
{
}


elem* NewSlice::emitILAsm(wostream& out)
{
    indent(out) << "call $SLICE<!!0> [dnetlib]runtime.Slice::Create<"
                << type_.elemTYPE().name()
                << ">(!!0[], int32, int32)\n";
    return this;
}


int32_t NewSlice::getStackDelta() const 
{
    return -2; // pops 3 args, pushes new obj
}


ArrayToSlice::ArrayToSlice(ArrayType& aType, Loc& loc)
    : sliceType_(aType.getSliceType(loc))
{
}


elem* ArrayToSlice::emitILAsm(std::wostream& out)
{
    if (!sliceType_) return NULL;
    if (sliceType_->elemTYPE().isStringType())
    {
        indent(out) << "call unsigned int8[][] [dnetlib]runtime.dnet::strArrayToByteArrayArray(string[])\n";
        indent(out) << "newobj instance void $SLICE<unsigned int8[]>::.ctor(!0[])\n";
    }
    else
    {
        indent(out) << "newobj instance void " << sliceType_->name() << "::.ctor(!0[])\n";
    }
    return this;
}
