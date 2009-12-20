// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: instruction.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008 Ionut-Gabriel Burete

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
#include <cassert>
#include <sstream>
#include <stdexcept> // for std::logic_error
#include "declaration.h"
#include "id.h" // for Id::dtor
#include "backend.net/type.h"
#include "backend.net/msil/aggr.h"
#include "backend.net/msil/deref.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/label.h"
#include "backend.net/msil/member.h"

using namespace std;


static wstring replaceUnderscores(const char* op)
{
    wstring s;
    for (; op && *op; ++op)
    {        
        if (*op == '_')
        {
            s += '.';
        }
        else
        {
            s += *op;
        }
    }
    return s;
}

/********************************************************/
#pragma region IL Instruction Set

struct Inst
{ 
    const wstring name;
    int32_t stackDelta;
};

#define IL_INST(op, delta) { replaceUnderscores(STRINGIZE(op)), delta },
static const struct Inst il_inst[] = {
#include "backend.net/msil/instrset.h"
};
#undef IL_INST

wostream& operator<<(wostream& out, Mnemonic m)
{
    out << il_inst[m].name.c_str();
    return out;
}

#pragma endregion
/********************************************************/


Instruction::Instruction(block& blk, Mnemonic mnemonic, Loc* loc)
    : AutoManaged<elem>(blk)
    , loc_(loc)
    , mnemo_(mnemonic)
{
}


Instruction::~Instruction()
{
}


int32_t Instruction::getStackDelta() const
{
    assert(mnemo_ != IL_call);
    assert(mnemo_ != IL_newobj);
    return il_inst[mnemo_].stackDelta;
}


elem* Instruction::emitILAsm(wostream& out)
{
    emitLineInfo(out);
    indent(out) << mnemo_ << '\n';
    return this;
}


bool Instruction::isBlockEnd() const
{
    switch (mnemo_)
    {
    case IL_ret: return true;
    }
    // IL_leave and branch instructions are handled by the Branch object
    return false;
}



CallInstruction::CallInstruction(block& blk, 
                                 Mnemonic mnemo,
                                 Method& m,
                                 Loc* loc
                                 )
    : Instruction(blk, mnemo, loc)
    , callee_(m)
    , result_(NULL)
{
    assert(mnemo == IL_call 
        || mnemo == IL_callvirt
        || mnemo == IL_newobj);

    if (m.isCtor())
    {
        return;
    }
    if (Type* type = m.getRetType())
    {
        if (type->ty != Tvoid)
        {
            result_ = savePartialResult(blk, DEREF(m.getResult()).getDecl(), type);
        }
    }
}


bool CallInstruction::isStructDtorCall() const
{
    return ((callee_.getDecl().ident == Id::dtor) && !callee_.getDecl().isClassMember());
}


elem* CallInstruction::emitILAsm(wostream& out)
{
    emitLineInfo(out);
#if !VALUETYPE_STRUCT
    // The front-end inserts explicit calls to the struct's destructor;
    // if structs are implemented as non value type classes (i.e. they
    // live on the GC-ed heap), we suppress garbage collection after the
    // __dtor call.
    if (isStructDtorCall())
    {
        indent(out) << "dup\n";
    }
#endif
    indent(out) << getMnemonic() << ' ';
    if (callee_.getDecl().isMember() && !callee_.getDecl().isStatic())
    {
        out << "instance ";
    }
    callee_.emitILProto(out, true /* fully qualified */);
    out << '\n';

#if !VALUETYPE_STRUCT
    if (isStructDtorCall())
    {
        indent(out) << "call void [mscorlib]System.GC::SuppressFinalize(object)\n";
    }
#endif
    return this;
}


Instruction* CallInstruction::create(
    block& block,   // block that will own the instr    
    Method& method,
    Loc* loc        // optional source code locus of call
    )
{
    return new CallInstruction(block, method.getCallInstr(), method, loc);
}


Variable* CallInstruction::isVariable()
{
    return result_ ? result_ : NULL;
}


int32_t CallInstruction::getStackDelta() const
{
    int32_t delta = callee_.getCallStackDelta();
#if !VALUETYPE_STRUCT
    if (isStructDtorCall())
    {
        ++delta; // we emit a DUP instruction to prep the GC.SuppressFinalize call
    }
#endif
    return delta;
}


BranchInstruction::BranchInstruction(block& blk, Mnemonic mnemonic, Loc* loc)
    : Instruction(blk, mnemonic, loc)
{
}


elem* BranchInstruction::emitILAsm(wostream& out)
{
    emitLineInfo(out);
    return NULL;
}


Branch::Branch(block& source, block* dest, bool isBlockEnd, Instruction* i)
    : AutoManaged<elem>(source)
    , label_(NULL)
    , endsBlock_(isBlockEnd)
    , inst_(i)
{
    if (!dest)
    {
        dest = &source;
    }
    label_ = &dest->getLabel();
}


Branch* Branch::create(block& source, block* dest, Loc* loc)
{
    Mnemonic op = IL_br;

    //if branching out of a try/catch block generate a
    //leave instruction instead of a branch opcode
    if (dest)
    {
        //find the inner-most block that has exception handling
        block* eh = &source;
        for (; eh; eh = eh->parent())
        {
            if (eh->hasEH())
            {
                break;
            }
        }
        if (eh)
        {
            op = IL_leave;
            //if the destination of the branching is inside
            //the try/catch block then it is okay to emit a BR
            for (block* b = dest; b; b = b->parent())
            {
                if (b == eh)
                {
                    op = IL_br;
                    break;
                }
            }
        }
    }

    Instruction* i = BranchInstruction::create(source, op, loc);
    Branch* br = new Branch(source, dest, true, i);
    assert(br->getOwner() == &source);
    return br;
}


void Branch::setLabel(Label& label)
{
    label_ = &label;
}


elem* Branch::emitILAsm(wostream& out)
{
    indent(out) << DEREF(inst_).getMnemonic() << ' ' << DEREF(label_).getName() << '\n';
    return this;
}
    

static Instruction* optimizeBranchCond(block& block, Instruction& inst)
{
    if (&inst == block.back())
    {
        switch (inst.getMnemonic())
        {
        case IL_clt:
            block.pop_back();
            return BranchInstruction::create(block, IL_blt);

        case IL_cgt:
            block.pop_back();
            return BranchInstruction::create(block, IL_bgt);

        case IL_ceq:
            block.pop_back();
            if (!block.empty() && block.back()->isNull())
            {
                block.pop_back();
                return BranchInstruction::create(block, IL_brnull);
            }
            return BranchInstruction::create(block, IL_beq);

        case IL_cgt_un:
            block.pop_back();
            block.pop_back();
            return BranchInstruction::create(block, IL_brtrue);

        default:
            if (inst.isLogicalNegation())
            {
                block.swapTail();
                if (Instruction* i = block.back()->isInstruction())
                {
                    switch (i->getMnemonic())
                    {
                    case IL_clt:
                        block.pop_back();
                        block.pop_back();
                        return BranchInstruction::create(block, IL_bge);

                    case IL_cgt:
                        block.pop_back();
                        block.pop_back();
                        return BranchInstruction::create(block, IL_ble);

                    case IL_cgt_un:
                        block.pop_back();
                        block.pop_back();
                        block.pop_back();
                        return BranchInstruction::create(block, IL_brnull);

                    case IL_ceq:
                        block.pop_back();
                        block.pop_back();
                        if (!block.empty() && block.back()->isNull())
                        {
                            block.pop_back();
                            return BranchInstruction::create(block, IL_brtrue);
                        }
                        return BranchInstruction::create(block, IL_bne_un);
                    }
                }
                block.swapTail();
            }
            break;
        }
    }
    return NULL;
}


//create conditional branch
Branch* Branch::create(elem*& cond, block& source, block* dest, Loc* loc)
{
    bool isBlockEnd = false;
    Instruction* inst = DEREF(cond).isInstruction();
    if (inst)
    {
        if (inst->isBranchInstruction())
        {
            isBlockEnd = (inst->getMnemonic() == IL_leave);
        }
        else
        {
            inst = optimizeBranchCond(source, *inst);
            if (inst)
            {
                // optimizeBranchCond may delete the cond 
                cond = NULL;
            }
        }
    }
    if (!inst)
    {
        assert(!source.empty());
        if (LogicalExpTail* tail = source.back()->isLogicalExpTail())
        {
            inst = tail->optimizeBranch(source, dest, loc);
        }
        else
        {
            inst = BranchInstruction::create(source, IL_brtrue, loc);
        }
    }
    return new Branch(source, dest, isBlockEnd, inst);
}


Box::Box(block& block, const TYPE& type, Loc* loc)
    : Instruction(block, IL_box, loc)
    , type_(type)
{
}


elem* Box::emitILAsm(wostream& out)
{
    emitLineInfo(out);
    indent(out) << getMnemonic() << ' ' << type_.name() << "\n";
    return this;
}



Conversion::Conversion(block& blk, TYPE& toType, Loc* loc)
    : Instruction(blk, IL_nop, loc)
    , toType_(toType)
{
}


Conversion::Conversion(block& blk, Mnemonic op, TYPE& toType, Loc* loc)
    : Instruction(blk, op, loc)
    , toType_(toType)
{
}


elem* Conversion::emitILAsm(wostream& out)
{
    if (getMnemonic() == IL_nop)
    {
        toType_.emitConversion(indent(out));
    }
    else
    {
        indent(out) << getMnemonic() << ' ' << toType_.name();
    }
    out << '\n';
    return this;
}


// switch
SwitchInstruction::SwitchInstruction(block& block,
                                     const LabelsArray& labels,
                                     Mnemonic mnemonic, Loc* loc
                                     )
    : Instruction(block, mnemonic, loc), labels_(labels) 
{
}



elem* SwitchInstruction::emitILAsm(wostream& out)
{
    indent(out) << il_inst[IL_switch].name;
    indent(out) << "(\n";

    LabelsArray::const_iterator it = labels_.begin();
    LabelsArray::const_iterator end = labels_.end();
    LabelsArray::size_type labelsCount = labels_.size();

    for ( ; it != end; ++it, --labelsCount)
    {
        indent(out, this->depth() + 1) << (*it)->getName();
        if (labelsCount > 1) out << ",";
        out << "\n";
	}
    indent(out) << ")\n";
    return this;
}


ArrayElemAccess::ArrayElemAccess(block& block, Op op, const TYPE& elemTYPE, Loc* loc, bool addr)
    // the opcode passed to the base class is just for the getStackDelta() computation
    : Instruction(block, (op == STORE ? IL_stelem_ref : IL_ldelem_ref), loc)
    , op_(op)
    , elemTYPE_(elemTYPE)
    , byAddr_(addr)
{
}


elem* ArrayElemAccess::emitILAsm(wostream& out)
{
    if (elemTYPE_.getElemSuffix()[0] == 0)
    {
        throw logic_error("cannot access array elements of type " + string(elemTYPE_.name()));
    }
    indent(out);
    if (byAddr_)
    {
        out << "ldelema " << elemTYPE_.name() << '\n';
    }
    else
    {
#if 0
        switch(op_)
        {
        case LOAD:  out << "ldelem."; break;
        case STORE: out << "stelem."; break;
        }
        out << elemTYPE_.getElemSuffix() << "\n";

#else
        switch(op_)
        {
        case LOAD:
            out << "ldelem." << elemTYPE_.getElemSuffix();
            break;

        case STORE:
            out << "stelem.any " << elemTYPE_.name();
            break;
        }
        out << '\n';
#endif
    }
    return this;
}


elem* LogicalNegation::emitILAsm(wostream& out)
{
    indent(out) << "ldc.i4 1\n";
    indent(out) << IL_xor << '\n';
    return this;
}


elem* PropertyGetter::emitILAsm(wostream& out)
{
    indent(out) << "call instance " << propType_.c_str() << " ";
    out << type_.name() << "::get_" << name_.c_str() << "()\n";
    return this;
}


LoadFunc::LoadFunc(block& blk, Method& method, Loc* loc) 
    : Instruction(blk, IL_ldftn, loc)
    , func_(method)
{
}


elem* LoadFunc::emitILAsm(wostream& out)
{
    Mnemonic opcode = func_.funcDecl().isVirtual() ? IL_ldvirtftn : IL_ldftn;
    indent(out) << opcode << ' ';
    if (func_.getDecl().isMember() && !func_.getDecl().isStatic())
    {
        out << "instance ";
    }
    func_.emitILProto(out, true /* fully qualified */);
    out << '\n';
    return this;
}


LogicalExpTail::LogicalExpTail(Branch* shortCircuit, elem* lhs, elem* rhs) 
    : shortCircuit_(shortCircuit)
    , lhs_(lhs ? lhs->isLogicalExpTail() : NULL)
    , rhs_(rhs)
    , optimized_(false)
{
}


elem* LogicalExpTail::emitILAsm(wostream& out)
{
    return NULL;
}


bool LogicalExpTail::preEmit(elem&)
{
    return !optimized_;
}


void LogicalExpTail::postEmit(elem*)
{
}


bool LogicalExpTail::isReferenced(const elem&) const
{
    return true;
}


int32_t LogicalExpTail::stackDelta(const elem& e, bool maxDelta)
{
    if (optimized_)
    {
        return 0;
    }
    return maxDelta ? e.getMaxStackDelta() : e.getStackDelta();
}


Instruction* LogicalExpTail::optimizeBranch(block& source, block* dest, Loc* loc)
{
#if 0
    DEREF(shortCircuit_).setLabel(dest->getLabel());
    optimized_ = true;
    if (lhs_)
    {
        lhs_->optimizeBranch(source, dest, loc);
    }

    if (Instruction* inst = DEREF(rhs_).isInstruction())
    {
        if (!inst->isBranchInstruction())
        {
            inst = optimizeBranchCond(DEREF(inst->getOwner()), *inst);
        }
        if (inst)
        {
            rhs_ = inst;
            return inst;
        }
    }
#endif
    return BranchInstruction::create(source, IL_brtrue, loc);
}
