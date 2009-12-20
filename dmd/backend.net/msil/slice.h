#pragma once

// $Id: slice.h 24625 2009-07-31 01:05:55Z unknown $
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
#include "backend.net/msil/instruction.h"

/****************************************************
 * Array Slice related classes
 */
struct SliceExp;

///<summary>
/// Emit a slice inside of a block, preparing the 
/// arguments on the evaluation stack for fill operations.
///</summary>
class ASlice : public elem
{
    elem* e_;
    Expression* lwr_;
    Expression* upr_;
    bool upperAdjusted_;
    block blk_;

    elem* emitILAsm(std::wostream&);
    int32_t getStackDelta() const { return blk_.getStackDelta(); }
    int32_t getMaxStackDelta() const { return blk_.getMaxStackDelta(); }

public:
    ASlice(block*, SliceExp&, unsigned indentationDepth);
    void setElem(elem* e) { e_ = e; }

    block& getBlock() { return blk_; }
    ASlice* isASlice() { return this; }
    Variable* isVariable() { return e_ ? e_->isVariable() : NULL; }
    Expression* lower() const { return lwr_; }
    Expression* upper() const { return upr_; }
    bool removeUpperAdjustment();
    void setUpperAdjusted() { assert(blk_.size() > 2); upperAdjusted_ = true; }
    void removeArray();
};


///<summary>
/// Emit calls to [dnetlib]core.Slice::Fill runtime helper
///</summary>
class SliceFiller : public Sequence
{
    const size_t size_; // how many elements to fill out 
    ASlice* slice_;     // for left-hand array slices
    const bool arrayCopy_;
    TYPE* elemTYPE_;

    elem* emitILAsm(std::wostream& out);
    int32_t getStackDelta() const;
    SliceFiller* isSliceFiller() { return this; }

public:
    explicit SliceFiller(size_t size = 0, ASlice* slice = 0, bool arrayCopy = false);
    void setSlice(ASlice* aslice) { slice_ = aslice; }
    size_t length() const { return size_; }
    void setElemTYPE(TYPE& t) { assert(!elemTYPE_); elemTYPE_ = &t; }
    TYPE* elemTYPE() const { return elemTYPE_; }
};


///<summary>
/// Emit code to create new array on the evaluation stack.
///</summary>
class NewArray : public elem
{
    const TYPE& elemTYPE_;

    elem* emitILAsm(std::wostream&);

    explicit NewArray(const ArrayType& type);
    explicit NewArray(const TYPE& type) : elemTYPE_(type)
    {
    }

public:
    static NewArray* create(const ArrayType& type)
    {
        return new NewArray(type);
    }
    static NewArray* create(const TYPE& type)
    {
        return new NewArray(type);
    }
};


class NewSlice : public elem
{
    const ArrayType& type_;

    elem* emitILAsm(std::wostream&);
    int32_t getStackDelta() const;

    explicit NewSlice(const ArrayType& type);

public:
    static NewSlice* create(const ArrayType& type)
    {
        return new NewSlice(type);
    }
};


//generates ctor call to convert from array to slice
class ArrayToSlice : public elem
{
    SliceType* sliceType_;

    elem* emitILAsm(std::wostream& out);
    
public:
    ArrayToSlice(ArrayType& aType, Loc& loc);
};

