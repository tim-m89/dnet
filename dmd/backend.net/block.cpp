// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: block.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <sstream>
#include <stdexcept>
#include "mars.h"
#include "declaration.h"
#include "backend.net/backend.h"
#include "backend.net/block.h"
#include "backend.net/utils.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/label.h"
#include "backend.net/msil/except.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/types.h" //for ArrayType
#include "backend.net/msil/variable.h"

using namespace std;


#if 0
// emit brackets in the IL textual output to show where blocks start/end.
 static bool showBlocks = true; 
#else
 static bool showBlocks = false;
#endif

#if 0
  #define IF_DEBUG_DELTA(x) x
#else
  #define IF_DEBUG_DELTA(x)
#endif


///<summary>
/// Additional information about control flow for a block:
/// optional top label, parallel control flow (for if/else blocks)
/// optional return code, optional exception handling.
/// Most blocks do not need it, so it is allocated lazily.
///</summary>
struct BlockCFInfo
{
    Label* label_;
    block* parallelCF_;     // used in stack delta computation
    block* ret_;
    auto_ptr<ExceptionHandler> eh_;
    deque<Method*> funcs_; // functions in this block

    BlockCFInfo() : label_(NULL), parallelCF_(NULL), ret_(NULL) 
    {
    }

    ~BlockCFInfo() 
    {
        delete ret_;
    }
};


block::block(block* parent, Loc& loc, unsigned depth)
    : delta_(UNKNOWN_STACK_DELTA)
    , maxDelta_(0)
    , loc_(loc)
    , hasLeave_(false)
    , hasRet_(false)
    , useBrackets_(false)
    , hasTls_(false)
    , depth_(depth)
    , parent_(parent)
{
}


block::~block()
{
    deleteElems(true);
    if (ExceptionHandler* eh = hasEH())
    {
        eh->setOwner(NULL);
    }
}


void block::clear()
{
    deleteElems(true);
    elems_.clear();
}


void block::ensureCFInfo()
{
    if (cfInfo_.get() == NULL)
    {
        cfInfo_.reset(new BlockCFInfo);
    }
}


block* block::addNewBlock(Loc* loc)
{
    block* blk = new block(this, (loc ? *loc : loc_), depth());
    add(*blk);
    return blk;
}


ExceptionHandler* block::getEH()
{
    if (ExceptionHandler* eh = hasEH())
    {
        return eh;
    }
    if (parent())
    {
        return parent()->getEH();
    }
    return NULL;
}


elem* block::add(elem& e, bool front)
{
    assert(e.getOwner() == NULL); // ownership is exclusive
    assert(&e != this);
    if (block* blk = e.isBlock())
    {
        assert(blk->parent() == NULL || blk->parent() == this);
        blk->parent_ = this;
    }
#if 1
    if ((maxDelta_ != 0) || (delta_ != UNKNOWN_STACK_DELTA))
    {
        throw logic_error("cannot add to block after stack deltas are computed");
    }
#else
    if (e.stackDelta())
    {
        maxDelta_ = 0;
        delta_ = UNKNOWN_STACK_DELTA;
    }
#endif
    if (front)
    {
        elems_.push_front(&e);
    }
    else
    {
        elems_.push_back(&e);
    }
    e.setOwner(this);
    if (Method* method = e.isMethod())
    {
        ensureCFInfo();

        //collect functions, to be used by generateMethods()
        cfInfo_->funcs_.push_back(method);
    }
    return &e;
}



// Add a "ret" instruction, making sure to convert arrays to slices
// where needed
static void addRetInstruction(block& blk,
                              Variable* ret,
                              Loc* loc,
                              int32_t delta
                              )
{
    if (ret)
    {
        if (!loc) loc = &ret->getDecl().loc;
        if (!blk.empty())
        {
            TYPE& retType = toILType(*loc, ret->getType());
            //Handle cases where the function returns a slice
            if (SliceType* sliceType = retType.isSliceType())
            {   //the array and the lower and upper bounds are on the stack,
                //need to construct a slice?
                elem* back = blk.back();
                if (DEREF(back).isASlice())
                {
                    blk.add(*NewSlice::create(*sliceType));
                }
                else if (StringLiteral* slit = DEREF(back).isStringLiteral())
                {
                    blk.add(*new ArrayToSlice(*sliceType, *loc));
                }
                else 
                {
                    Variable* v = DEREF(back).isVariable();
                    if (!v)
                    {
                        v = DEREF(back).isLoad();
                    }
                    if (v)
                    {
                        if (!toILType(*loc, v->getType()).isSliceType())
                        {
                            // assume that an array ref is on top of the stack
                            blk.add(*new ArrayToSlice(*sliceType, *loc));
                        }
                    }
                }
            }
        }
    }
    new RetInstruction(blk, loc, delta);
}


void block::addRet(Loc* loc, Variable* ret)
{
    const int32_t delta = ret ? -1 : 0;
    // MSIL mandates to not return directly from within a block
    // with exception handling; instead, a "leave" instruction
    // must be used
    if (ExceptionHandler* eh = getEH())
    {
        block& retBlock = eh->getRetBlock();
        if (ret)
        {
            ret->store(*this, loc);
        }
        if (retBlock.size() <= 1)
        {
            if (ret)
            {
                ret->load(retBlock);
            }
            addRetInstruction(retBlock, ret, loc, delta);
            retBlock.hasRet_ = true;
        }
        elem* inst = BranchInstruction::create(*this, IL_leave, loc);
        Branch::create(inst, *this, &retBlock);
    }
    else
    {
        addRetInstruction(*this, ret, loc, delta);
        hasRet_ = true;
    }
}


bool block::isReferenced() const
{
    if (Label* label = hasLabel())
    {
        return label->refCount() > 0;
    }
    return false;
}


static inline bool isReachableBlock(elem& e)
{
    if (block* blk = e.isBlock())
    {
        return blk->empty() || blk->isReferenced();
    }
    return false;
}


bool block::isEmitable()
{
    if (hooks_)
    {
        return hooks_->preEmit(*this);
    }
    return owner_ ? owner_->isEmitable() : true;
}


elem* block::emitILAsm(wostream& out)
{
    //visualize the beginning of the block for debugging purposes
    IF_DEBUG(if (showBlocks) indent(out) << "// { " << this << "\n");

    unsigned unreachable = 0;
    Variable* loadVar = NULL;
    Label* lastLabel = NULL;
    elem* lastEmitted = NULL;

    if (empty())
    {
        useBrackets_ = false;
    }
    if (useBrackets_)
    {
        unindent(out) << "{\n";
    }
    IF_DEBUG_DELTA(int32_t delta = 0);

    //Using an index rather than iterators prevents crashes if insertions
    //occur while we're iterating -- the assumption here is that IF
    //insertions occur, they are always AT THE END (the only case where
    //we use push_front is getLabel).
    for (size_t i = 0; i != elems_.size(); ++i)
    {
        elem& e = DEREF(elems_[i]);
        assert(e.getOwner() == this);

        if (unreachable)
        {            
            // blocks may still be reachable via branching
            // (all other elems cannot be branched to)
            if (!isReachableBlock(e))
            {
                IF_DEBUG(e.emitIL(e.indent(out) << "/* unreachable:\n");
                         e.indent(out) << " */\n";)
                continue;
            }
        }
        // optimize consecutive, identical load operations
        // by replacing them with a (shorter) "dup" instr.
        Variable* v = e.isLoad();
        if (v && (v == loadVar))
        {
            assert(e.stackDelta() == 1);
            e.indent(out) << "dup\n";
        }
        else
        {
            Label* label = e.isBranching();
            if (label && (label == lastLabel))
            {
                out << '\n'; // optimize out redundant branching instructions
            }
            else if (elem* emitted = e.emitIL(out))
            {
                lastEmitted = emitted;
                // memorize last var load on the stack
                loadVar = emitted->isLoad();
                lastLabel = emitted->isBranching();
            }
        }
        IF_DEBUG_DELTA(delta += e.stackDelta());
        IF_DEBUG_DELTA(indent(out) << "/* delta=" << delta << " */\n\n");

        if (e.isBlockEnd())
        {
            ++unreachable;
        }
    }
    emitTail(out, lastEmitted);
    if (useBrackets_)
    {
        unindent(out) << "}\n";
    }
    IF_DEBUG(if (showBlocks)indent(out) << "// } " << this << "\n");
    //if this block has exception handling, do not propagate the last
    //emitted instruction up to the parent -- the "logical context" of
    //the block ends here
    if (hasEH())
    {
        lastEmitted = NULL;
    }
    return lastEmitted;
}


void block::emitTail(wostream& out, elem* lastEmitted)
{
    if (Instruction* inst = lastEmitted ? lastEmitted->isInstruction() : 0)
    {
        switch (inst->getMnemonic())
        {
        case IL_leave:
            hasLeave_ = true;
            break;
        }
    }
    if (ExceptionHandler* eh = hasEH())
    {
        eh->emitIL(out);
    }
    if (block* ret = hasRetBlock()) // emit return block if any
    {
        if (ret->isReferenced())
        {
            lastEmitted = ret->emitIL(out);
        }
    }
}


void block::swapTail()
{
    //assert(elems_.size() >= 2);
    if (elems_.size() >= 2) 
    {
        elem* last = elems_.back();
        elems_.pop_back();
        elem* next = elems_.back();
        elems_.pop_back();
        elems_.push_back(last);
        elems_.push_back(next);
    }
}


///<summary>
/// Compute the stack delta of a basic block by summing up the deltas of its elems.
///</summary>
/// To understand the stack delta and why it is needed see:
/// "Compiling for the .NET Common Language Runtime (CLR)" by John Gough, page 221
/// (a slightly different algorithm is used here than what's presented in the book).
///
/// See also getMaxStackDelta below.
int32_t block::getStackDelta() const
{
    if (delta_ == UNKNOWN_STACK_DELTA)
    {
        int32_t delta = 0;

        for (const_iterator i = elems_.begin(); i != elems_.end(); ++i)
        {
            elem& e = DEREF(*i);
            assert(e.stackDelta() != UNKNOWN_STACK_DELTA);
            delta += e.stackDelta();
            if (e.isBlockEnd())
            {
                break;
            }
        }
        // Example of "parallel control flow": the IF and ELSE branches;
        // the stack delta of either branch MUST be the same.

         // is there a parallel control flow block? 
        if (block* parallelCF = hasParallelCF())
        {
            if (parallelCF->stackDelta() != delta)
            {
                throw logic_error("converging blocks have different stack deltas");
            }
            delta = 0;
        }
        delta_ = delta;
    }
    return delta_;
}


///<summary>
/// Compute the max stack delta of a basic block.
///</summary>
int32_t block::getMaxStackDelta() const
{
    if (maxDelta_ == 0)
    {
        int32_t delta = 0;

        for (const_iterator i = elems_.begin(); i != elems_.end(); ++i)
        {
            elem& e = DEREF(*i);

            const int32_t local = delta + e.maxStackDelta();
            if (local > maxDelta_)
            {
                maxDelta_ = local;
            }
            delta += e.stackDelta();

            if (delta > maxDelta_)
            {
                maxDelta_ = delta;
            }
            if (e.maxStackDelta() > maxDelta_)
            {
                maxDelta_ = e.maxStackDelta();
            }
            if (e.isBlockEnd())
            {
                break;
            }
        }
        if (block* parallelCF = hasParallelCF())
        {
            if (parallelCF->maxStackDelta() > maxDelta_)
            {
                maxDelta_ = parallelCF->maxStackDelta();
            }
        }
        if (ExceptionHandler* eh = hasEH())
        {
            int32_t delta = eh->maxStackDelta();
            if (delta > maxDelta_)
            {
                maxDelta_ = delta;
            }
        }
    }    
    assert(maxDelta_ >= 0);
    return maxDelta_;
}


Label& block::getLabel(bool incRef)
{
    ensureCFInfo();
    Label*& label = cfInfo_->label_;
    if (!label)
    {
        label = new Label(BackEnd::instance().newLabel(), loc_);
        label->setOwner(this);
        elems_.push_front(label);
    }
    if (incRef)
    {
        label->incRefCount();
    }
    return *label;
}



namespace {
    ///<summary>
    /// Default exception handling
    ///</summary>
    class DefaultEH : public ExceptionHandler
    {
        bool xlat_; //translate System.Exception-s into core.Exception-s

        DefaultEH(block* parent, Loc& loc, bool xlat)
            : ExceptionHandler(parent, loc, 1)
            , xlat_(xlat)
        { }

        void emitILAsmImpl(wostream& out, const wstring& label)
        {
            unsigned depth = this->depth();
            if (depth) --depth;

            indent(out, depth) << "catch [mscorlib]System.Exception\n";
            indent(out, depth) << "{\n";
            if (xlat_)
            {
                indent(out, depth + 1) << "call object [dnetlib]core.ForeignError::Wrap("
                                          "class [mscorlib]System.Exception)\n";
                indent(out, depth + 1) << IL_throw << '\n'; // rethrow translated exception
            }
            else
            {
                indent(out, depth + 1) << "callvirt instance string [mscorlib]System.Exception::get_Message()\n";
                indent(out, depth + 1) << "call void [mscorlib]System.Console::WriteLine(string)\n";
            }
            indent(out, depth + 1) << "leave ";
            if (label.empty())
            {
                out << getEndBlock().getLabel().getName();
            }
            else
            {
                out << label;
            }
            out << '\n';
            indent(out, depth) << "}\n";
        }

    public:
        static ExceptionHandler* create(block& blk, bool xlat = false) 
        {
            return new DefaultEH(&blk, blk.loc(), xlat);
        }
    };
}


void block::addDefaultEH()
{
    incDepth();
    setEH(DefaultEH::create(*this));
}


void block::addEHTranslator()
{
    incDepth();
    setEH(DefaultEH::create(*this, true));
}


void block::setEH(ExceptionHandler* eh)
{
    assert(eh);
    assert(eh->parent() == NULL || eh->parent() == this);
    eh->setOwner(this);
    getLabel(false); // force label; labels emit the ".try {" instruction
    ensureCFInfo();
    cfInfo_->eh_.reset(eh);
}


block* block::hasRetBlock() const
{
    if (cfInfo_.get())
    {
        return cfInfo_->ret_;
    }
    return NULL;
}


block* block::getRetBlock()
{
    if (block* ret = hasRetBlock())
    {
        return ret;
    }
    if (parent())
    {
        return parent()->getRetBlock();
    }

    // if no parent, block must be at function level,
    // create a return block
    ensureCFInfo();
    return cfInfo_->ret_ = new block(NULL, loc_, depth());
}


block* block::hasParallelCF() const
{
    if (cfInfo_.get())
    {
        return cfInfo_->parallelCF_;
    }
    return NULL;
}


void block::setParallelCF(block &block)
{
    assert(!hasParallelCF());
    ensureCFInfo();
    cfInfo_->parallelCF_ = &block;
}


Label* block::hasLabel() const
{
    if (BlockCFInfo* cfInfo = cfInfo_.get())
    {
        return cfInfo->label_;
    }
    return NULL;
}


ExceptionHandler* block::hasEH() const
{
    if (BlockCFInfo* cfInfo = cfInfo_.get())
    {
        return cfInfo->eh_.get();
    }
    return NULL;
}


void block::moveElemsTo(block& other)
{
    assert(&other != this);
    while (!elems_.empty())
    {
        elem* e = elems_.front();
        other.elems_.push_back(e);
        e->setOwner(&other);
        elems_.pop_front();
    }
}



void block::pop_back(bool deleteElem)
{
    assert(!elems_.empty());
    elem* e = elems_.back();
    assert(e != hasLabel());
    elems_.pop_back();
    e->setOwner(NULL);
    if (deleteElem)
    {
        if (e->canDelete())
        {
            delete e;
        }
        else
        {
            assert(false);
        }
    }
    else if (block* b = e->isBlock())
    {
        b->parent_ = NULL;
    }
}


void block::pop_front(bool deleteElem)
{
    assert(!elems_.empty());
    elem* e = elems_.front();
    assert(e != hasLabel());
    elems_.pop_front();
    e->setOwner(NULL);
    if (deleteElem)
    {
        delete e;
    }
}


void block::generateMethods()
{
    if (cfInfo_.get())
    {
        deque<Method*>& funcs = cfInfo_->funcs_;
        while (!funcs.empty())
        {
            Method* m = funcs.front();
            funcs.pop_front();
            if (m->isNested())
            {
                assert(!m->getBody().empty());
            }
            else
            {
                m->generateBody();
            }
        }
    }
}


void block::deleteElems(bool checkIfDeletable)
{
    while (!elems_.empty())
    {
        elem* e = elems_.front();
        elems_.erase(elems_.begin());
        e->setOwner(NULL);
        if (checkIfDeletable && !e->canDelete())
        {
            Assembly& assembly = BackEnd::instance().getAssembly();
            if (&assembly == this)
            {
                delete e;
            }
            else
            {
                assembly.add(*e);
            }
        }
        else
        {
            delete e;
        }
    }
}


elem* LocalsBlock::emitILAsm(wostream& out)
{
    if (!empty())
    {
        wostringstream buf;
        bool emit = false;

        for (iterator i = begin(); i != end(); ++i)
        {
            if ((*i)->isSlot())
            {
                continue;
            }
            else if ((*i)->emitIL(buf))
            {
                emit = true;
            }
        }
        if (emit)
        {
            indent(out) << ".locals init (\n" << buf.str() << '\n';
            indent(out) << ")\n";
        }
    }
    return this;
}


bool LocalsBlock::empty() const
{
    bool empty = block::empty();
    if (!empty)
    {
        empty = true;
        for (const_iterator i = begin(), e = end(); i != e; ++i)
        {
            if (!(*i)->isSlot())
            {
                empty = false;
                break;
            }
        }
    }
    return empty;
}

