// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: arraytype.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <sstream>
#include <string>
#include "declaration.h"
#include "init.h"
#include "mtype.h"
#include "backend.net/array.h"
#include "backend.net/backend.h"
#include "backend.net/block.h"
#include "backend.net/irstate.h"
#include "backend.net/utils.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/null.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/varargs.h"
#include "backend.net/msil/variable.h"

using namespace std;


static const TYPE& getDim(const ArrayType& aType, Expressions& dim, bool top = true)
{
    if (Expression* exp = aType.getSize()) // is static array?
    {
        if (!top)
        {
            dim.push(exp);
        }
        if (ArrayType* aChildType = aType.elemTYPE().isArrayType())
        {
            return getDim(*aChildType, dim, false);
        }
    }
    return aType.elemTYPE();
}


ArrayTypeBase::ArrayTypeBase(Type& elemType) : elemType_(&elemType)
{
    // IL does not allow arrays of type void
    if (elemType.ty == Tvoid)
    {
        elemType_ = getObjectType();
    }
}


ArrayType::ArrayType(Loc& loc, Type& elem, Expression* dim)
    : ArrayTypeBase(elem)
    , elemTYPE_(toILType(loc, &elemType()))
    , unjaggedElemTYPE_(NULL)
{
    dim_.push(dim);
    name_ = elemTYPE().name();
    name_ += " []";
    unjaggedElemTYPE_ = &getDim(*this, dim_);
    IF_WIN_DEBUG(OutputDebugStringA((name_ + "\n").c_str()));
}


const char* ArrayType::name() const
{
    return name_.c_str();
}


Expression* ArrayType::getSize(unsigned dimension) const
{
    assert(dimension < dim_.dim);
    return ArrayAdapter<Expression*>(dim_)[dimension];
}


const char* ArrayType::getSuffix() const
{
    return name();
}


//add newarr instruction or array ctor to block
elem* newArray(block& blk, Loc& loc, const ArrayType& aType, unsigned size)
{
    if (Expression* dim = aType.getSize())
    {
        IRState temp(blk);
        dim->toElem(&temp);
    }
    else
    {
        Const<long>::create(*TYPE::Int32, size, blk, loc);
    }
    return blk.add(*NewArray::create(aType));
}


namespace {
    /// <summary>
    /// A block of code that contains a newarr instruction.
    /// </summary>
    /// Array initialization and assignment are a bit tricky: a variable
    /// may start its life as an array (int [] foo) but our compiler backend
    /// may later decide to change its type to a slice (System.ArraySegment)
    /// if it detects an assignment where the right hand-side is a potential
    /// slice.
    /// By the time the type change occurs, a "newarr" initialization block
    /// may already have been generated; this is handled by emitting the "newarr" 
    /// into a block that keeps track of the target array.
    class NewArrayBlock : public block
    {
        Variable& array_;

        elem* emitILAsm(wostream& out)
        {
            elem* result = block::emitILAsm(out);
            if (SliceType* sliceType = toILType(array_).isSliceType())
            {                
                auto_ptr<ArrayToSlice> newSlice(new ArrayToSlice(*sliceType, loc()));
                indent(out);
                newSlice->emitIL(out);
            }
            return result;
        }

    public:
        NewArrayBlock(block* parent, Variable& a, Loc& loc, unsigned depth)
            : block(parent, loc, depth)
            , array_(a)
        {
            assert(toILType(loc, a.getType()).isArrayType());
        }
    };
}


//Generate code that ensures that an array variable is initialized
elem* ArrayType::init(IRState& state,
                      VarDeclaration& decl,
                      Expression* exp,
                      elem* initializer
                      )
{
    assert((decl.init == NULL) == (exp == NULL));
    assert(decl.type);
    assert(decl.type->ctype == this);
    size_t size = 0;

    if (initializer)
    {
        if (Sequence* seq = initializer->isSliceFiller())
        {
            size = seq->length();
        }
    }
    if (size == 0 && dim_.dim)
    {
        if (Expression* sz = getSize())
        {
            size = sz->toInteger();
        }
    }

    Variable* a = DEREF(decl.csym).isVariable();
    if (!a)
    {
        BackEnd::fatalError(decl.loc, "variable expected");
    }
    elem* result = NULL;
    if (size || !decl.type->isString())
    {
        // create new array on the top of the stack
        block* blk = new NewArrayBlock(NULL, *a, decl.loc, state.getBlock().depth());
        state.add(*blk);
        result = newArray(*blk, decl.loc, *this, size);
        // todo: revisit this, as there are cases where the newarr is unnecessary:
        // int a[];
        // a = [1, 2, 3]
    }
    if (!exp)
    {
        if (!result)
        {
            //error(decl.loc, "invalid array initializer");
            return state.add(*new Null);
        }
        initJagged(state, decl);
    }
    else if (!result)
    {
        result = Instruction::create(state.getBlock(), IL_nop);
    }
    else
    {
        block& blk = state.getBlock();

        //code for expression has already been generated,
        //move the newarr stuff in front on the initializer
        blk.swapTail();

        //default array initializer?
        if (isImplicit(DEREF(decl.init)))
        {
            //do not generate it, the VES does it for us anyway
            blk.pop_back();
            //we might have to initialize elems for arrays of arrays
            initJagged(state, decl);
        }
        else if (exp->type == &elemType())
        {
            // SliceFiller generates code that calls a runtime helper

            SliceFiller* fillHelper = new SliceFiller;
            fillHelper->setElemTYPE(elemTYPE());
            state.add(*fillHelper);
            //convert result of Slice::Fill from object to array type
            Conversion::create(blk, *this);
        }
        else if (size)
        {
            //see above, if the size is non-zero the initializer
            //is a slice filler
            //convert result of Slice::Fill from object to array type
            Conversion::create(blk, *this);
        }
    }
    return result;
}


//This overload is called in the context of ArrayInitializer::toDt
elem* ArrayType::init(IRState& state, VarDeclaration& decl, unsigned dim)
{
    assert(decl.type);
    assert(decl.type->ctype == this);

    block& block = state.getBlock();
    elem* result = newArray(block, decl.loc, *this, dim);
    if (decl.csym)
    {
        if (Variable* v = decl.csym->isVariable())
        {
            //the code that initializes the individual elements is expected
            //to follow, duplicate the array ref on top of the stack
            Instruction::create(state.getBlock(), IL_dup);
            v->store(state.getBlock());
            result = v;
        }
    }
    return result;
}


bool ArrayType::initDynamic(block& blk, Loc& loc, Variable& a, size_t size) const
{
    // if size not known at compile-time, it is a D dynamic array
    if (getSize() == 0)
    {
        block* group = blk.addNewBlock(&loc);
        Const<size_t>::create(*TYPE::Uint, size, *group, loc);
        group->add(*NewArray::create(*this));
        
        if (a.isBound())
        {   // compensate for the swapTail inside Field::store
            blk.swapTail();
        }
        a.store(blk);
        a.load(blk);
        return true;
    }
    return false;
}


namespace {
    ///<summary>
    ///Emit code that calls runtime helper
    ///</summary>
    class JaggedArrayInit : public elem
    {
        const TYPE& elemTYPE_;

    public:
        JaggedArrayInit(const TYPE& elemTYPE) : elemTYPE_(elemTYPE)
        {
        }

        //the helper is a generic static method parameterized by type 
        elem* emitILAsm(wostream& out)
        {
            indent(out)
                << "call void [dnetlib]runtime.Array::Init<" << elemTYPE_.name()
                << ">(class [mscorlib]System.Array, unsigned int32 [])\n";
            return this;
        }

        int32_t getStackDelta() const { return -2; }
    };
}


void ArrayType::initJagged(IRState& state, VarDeclaration& decl) const
{
    if (getRank() > 1)
    {
        assert(!DEREF(decl.type).isString());
        Instruction::create(state.getBlock(), IL_dup);
        IRState temp(state.getBlock());
        VarArgs* dim = VarArgs::create(temp, &dim_, decl.loc, 0, false);
        state.getBlock().add(*dim);
        state.getBlock().add(*new JaggedArrayInit(DEREF(unjaggedElemTYPE_)));
    }
}


SliceType* ArrayType::getSliceType(Loc& loc)
{
    return BackEnd::instance().getSliceType(loc, *this);
}


SliceType::SliceType(Loc& loc, Type& elemType)
    : ArrayType(loc, elemType)
    , type_(new TypeDArray(&elemType))
{
    type_->ctype = this;
    name_ = "$SLICE<";
    name_ += elemTYPE().name();
    name_ += ">";
}
