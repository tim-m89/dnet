//
// $Id: partialresult.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu
//
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
#include "declaration.h"
#include "backend.net/elem.h"
#include "backend.net/irstate.h"
#include "backend.net/partialresult.h"
#include "backend.net/type.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/deref.h"
#include "backend.net/msil/instruction.h"

using namespace std;


namespace {
    ///<summary>
    /// Create a temporary variable for storing partial results.
    /// For example, in an expression like f().setX(x); the return
    /// of the function is saved into a temp, then the code for
    /// $tmp.setX(x) is generated.
    ///</summary>
    class Result : public PartialResult, protected EmitHooks
    {
        Variable* v_;
        Variable* parent_;
        bool bound_ : 1;
        bool optimize_ : 1;


        Result(const Result&); //non-copyable
        Result& operator=(const Result&); //non-assignable


        //create a temporary "shadow" copy of the result
        void shadow(block& blk, IRState* irstate = NULL)
        {
            if (!irstate)
            {
                irstate = BackEnd::getModuleScope(blk.loc()).getCurrentIRState();
            }
            v_ = irstate->createTempVar(getDecl(), getType());
            v_->setEmitHooks(this);
            elem* store = v_->store(blk);
            store->setEmitHooks(this);
        }

        elem* emitILAsm(wostream& out)
        {
            if (optimize_ && (refCount() == 0))
            {
                indent(out) << "pop\n";
                return this;
            }
            return NULL;
        }

    protected:
        virtual void bind(Variable* parent, bool bound)
        {
            parent_ = parent;
            bound_ = bound;
        }
        virtual void unbind(bool resetParent = false)
        {
            bound_ = false;
            if (resetParent)
            {
                parent_ = false;
            }
        }
        virtual bool isBound() const
        {
            return bound_;
        }
#pragma region EmitHooks
        bool preEmit(elem& e)
        {
            return optimize_ ? (refCount() > 1) : true;
        }

        void postEmit(elem*)
        {
        }

        int32_t stackDelta(const elem& e, bool maxValue)
        {
            if (optimize_ && (refCount() <= 1))
            {
                return 0;
            }
            if (maxValue)
            {
                return e.getMaxStackDelta();
            }
            else
            {
                return e.getStackDelta();
            }
        }

        virtual bool isReferenced(const elem& e) const
        {
            if (optimize_ && (refCount() <= 1))
            {                
                while (DEREF(v_).decRefCount() > 0)
                {   // force the ref count of v_ down to zero
                    // to release the local var slot
                }
                return false;
            }
            return true;
        }
#pragma endregion
        bool canDelete() { return false; }

        int32_t getStackDelta() const
        {
            if (optimize_ && (refCount() == 0))
            {
                return -1; // pops one
            }
            return 0;
        }
    public:
        Result(block& blk, VarDeclaration& decl, Type* type, bool optimize)
            : PartialResult(decl, type)
            , v_(NULL)
            , parent_(NULL)
            , bound_(false)
            , optimize_(optimize && !decl.isRef())
        {
            shadow(blk);
        }
        
        Result(IRState& irstate, VarDeclaration& decl, Type* type, bool optimize)
            : PartialResult(decl, type)
            , v_(NULL)
            , parent_(NULL)
            , bound_(false)
            , optimize_(optimize && !decl.isRef())
        {
            shadow(irstate.getBlock(), &irstate);
        }

        virtual elem* load(block& blk, Loc* loc, bool addr)
        {
            incRefCount();
            if (addr)
            {
                optimize_ = false; // cannot optimize out the temporary
            }
            elem* load = NULL;
            if (getDecl().isRef())
            {
                block* b = blk.addNewBlock(loc);
                DEREF(v_).load(*b, loc);
                load = b->add(*new LoadIndirect(toILType(*this)));
            }
            else
            {
                load = DEREF(v_).load(blk, loc, addr);
            }
            load->setEmitHooks(this);
            return load;
        }

        virtual elem* store(block& blk, Loc* loc)
        {
            incRefCount();
            if (bound_)
            {
                DEREF(parent_).store(blk, loc);
            }
            if (getDecl().isRef())
            {
                return blk.add(*new StoreIndirect(toILType(*this)));
            }
            return this;
        }
    };


    ///<summary>
    /// Temporary variable for storing associative array elements.
    ///</summary>
    ///
    /// It works in conjunction with postCall in irstate.cpp: for assoc arrays of structs,
    /// code such as  MyStruct[int] a; a [i] = MyStruct();
    /// may result in calls to MyStruct.opAssign -- and we generate something like:
    ///     dictionary.get_Item(i);
    ///     store tmp;
    ///     ...
    ///     tmp.opAssign();
    ///     ...
    ///     dictionary.set_Item(i);
    /// We need to retire the get_Item call because 1) it is useless 2) it may throw;
    /// and that's what this class takes care of.
    class AssocArrayElemTemp : public Result
    {
        bool suppress_;
        Instruction* inst_;
        Variable* keyVar_;
        block* blk_;

        virtual bool preEmit(elem& e)
        {
            if ( suppress_ && (/*&e == blk_ || */ &e == inst_ || e.isStore()) )
            {
                return false;
            }
            return Result::preEmit(e);
        }

        int32_t stackDelta(const elem& e, bool maxValue)
        {
            if (!preEmit(const_cast<elem&>(e)))
            {
                return 0; // suppressed elements have zero stack delta
            }
            return Result::stackDelta(e, maxValue);
        }

        virtual void suppress()
        {
            suppress_ = true;
            DEREF(inst_).setEmitHooks(this);
            if (blk_)
            {
#if 0
                Instruction::create(*blk_, IL_pop);
                Instruction::create(*blk_, IL_pop);
#else
                //remove the elements that are not needed
                for (block::iterator i = blk_->begin(); i != blk_->end(); )
                {
                    bool erase = false;
                    
                    if ((*i)->isConst())
                    {
                        erase = true;
                    }
                    else if (Variable* v = (*i)->isLoad())
                    {
                        if ((v->getType()->ty == Taarray) || (v == keyVar_) || v->isNested())
                        {
                            erase = true;
                        }
                    }
                    
                    if (erase)
                    {
                        elem* e = *i;
                        i = blk_->erase(i);
                        BackEnd::instance().getAssembly().add(*e);
                    }
                    else
                    {
                        ++i;
                    }
                }
                keyVar_ = NULL;
#endif
            }
        }

    public:
        AssocArrayElemTemp( block& blk,
                            VarDeclaration& decl,
                            Type* type,
                            Instruction* inst,
                            Variable* keyVar,
                            block* b
                            )
            : Result(blk, decl, type, true)
            , suppress_(false)
            , inst_(inst)
            , keyVar_(keyVar)
            , blk_(b)
        {
            //DEREF(b).setEmitHooks(this);
        }
    };
} //namespace


PartialResult::PartialResult(VarDeclaration& decl, Type* type) 
    : Variable(decl, type)
{ 
}


Variable* savePartialResult(block& blk,
                            VarDeclaration& decl,
                            Type* type,
                            bool optimize
                            )
{
    Result* result = new Result(blk, decl, type, optimize);
    blk.add(*result);
    return result;
}


Variable* savePartialResult(IRState& irstate,
                            VarDeclaration& decl,
                            Type* type,
                            bool optimize
                            )
{
    Result* result = new Result(irstate, decl, type, optimize);
    irstate.add(*result);
    return result;
}


Variable* savePartialResult(block& blk, 
                            VarDeclaration& decl,
                            Type* type,
                            AssocArrayElemAccess& inst,
                            Variable* keyVar,
                            block* b
                            )
{
    Result* result = new AssocArrayElemTemp(blk, decl, type, &inst, keyVar, b);
    blk.add(*result);
    return result;
}
