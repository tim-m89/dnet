#pragma once
//
// $Id: irstateutil.h 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2009 Cristian L. Vlasceanu
//
// Misc. utilities used in IRState implementation

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

#include <memory>
#include "statement.h"
#include "backend.net/irstate.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/slice.h"

std::auto_ptr<block> 
makeNestedLocalsBlock(IRState& irState, Loc& loc, bool makeNewBlock = true);


elem& exprToElem(IRState&, Expression&);

elem& loadExp(IRState&, Expression&, Variable&, bool loadAddr = false);

elem& loadExp(IRState& irState,
              Expression& exp,
              Variable** var = NULL,
              bool loadAddr = false,
              bool inAssignment = false
              );
bool isSysProperty(Method& method);
void loadClosure(block&, Loc&, Method* method);
SliceType* loadArray(IRState& irstate, Variable& v, bool inSliceExp = false);


///<summary>
/// Generates the code that constructs a delegate of the given type
///</summary>
class Delegate : public elem
{
    int32_t getStackDelta() const { return -1; } // pops two, returns new obj

    elem* emitILAsm(std::wostream& out)
    {
        indent(out) << "newobj instance void " << delegateTYPE_.name()
                    << "::.ctor(object, native int)\n";
        return this;
    }

    TYPE& delegateTYPE_;

    Delegate* isDelegate() { return this; }

public:
    explicit Delegate(TYPE& delegateTYPE) : delegateTYPE_(delegateTYPE)
    {
    }
    DelegateType* getType()
    {
        return delegateTYPE_.isDelegateType();
    }
};


///<summary>
/// Uses RAII to automatically save and restore the current insertion block.
///</summary>
class EmitInBlockBase
{
    IRState& irstate_;
    Temporary<block*> oldOwner_;

protected:
    explicit EmitInBlockBase(IRState& irstate) : irstate_(irstate)
    {
        irstate.push(); // save current insert position
    }

    EmitInBlockBase(IRState& irstate, block& blk)
        : irstate_(irstate)
    {
        irstate.push(); // save current insert position
    }

    // restore the insert position
    ~EmitInBlockBase() { irstate_.pop(); }
};


///<summary>
/// Helper for generating code for a given statement inside a specified block.
/// IRState's current block (i.e. the block in which elems are to be inserted)
/// is set to 'block', then restored using RAII
///</summary>
class EmitInBlock : EmitInBlockBase
{
public:
    EmitInBlock(IRState& irstate, Statement& stat, block& blk, block* merge = NULL)
        : EmitInBlockBase(irstate, blk)
    {
        irstate.setBlock(blk);      // further elem inserts go into block
        stat.toIR(&irstate);        // generate code for the statement
        
        if (merge)
        {   // end by jumping unconditionally to a merging point
            Branch::create(irstate.getBlock(), merge);
        }
    }

    EmitInBlock(IRState& irstate, block& blk) 
        : EmitInBlockBase(irstate, blk)
    {
        irstate.setBlock(blk);      // further elem inserts go into block
    }

    explicit EmitInBlock(IRState& irstate) : EmitInBlockBase(irstate)
    {
    }
};


///<summary>
/// Use RAII to set and restore the var scope of the current method.
///</summary>
class NestedVarScope
{
    block&  vars_;
    Method& method_;
    block*  oldScope_;

public:
    explicit NestedVarScope(block& vars)
        : vars_(vars)
        , method_(BackEnd::getCurrentMethod(vars.loc()))
        , oldScope_(NULL)
    {
        oldScope_ = method_.getVarScope();
        method_.setVarScope(&vars_);
    }

    ~NestedVarScope()
    {
        method_.setVarScope(oldScope_);
        for (block::iterator i = vars_.begin(); i != vars_.end(); ++i)
        {
            if (Variable* v = (*i)->isVariable())
            {
                v->releaseSlot();
            }
        }
    }

    Method& getMethod() { return method_; }
};


class VarBlockScope : public EmitInBlockBase
{
    //Do not change declaration order, vars_
    //MUST be initialized before scope_
    std::auto_ptr<block> vars_;
    NestedVarScope scope_;

public:
    VarBlockScope(IRState& irstate, Loc& loc) 
        : EmitInBlockBase(irstate)
        , vars_(makeNestedLocalsBlock(irstate, loc))
        , scope_(*vars_)
    {
        irstate.getBlock().add(vars_);
        assert(vars_.get() == NULL);
    }

    Method& getMethod() { return scope_.getMethod(); }
};
