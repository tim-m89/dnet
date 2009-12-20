#ifndef BLOCK_H__656F4873_FA51_477B_9508_85F191FC5376
#define BLOCK_H__656F4873_FA51_477B_9508_85F191FC5376
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: block.h 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008 - 2009 Cristian L. Vlasceanu

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
#include <string>
#include <deque>
#include <memory>
#include "backend.net/elem.h"

template<typename T> class AutoManaged;

struct Loc; // front-end, a location in the source code

class Assembly;
class ExceptionHandler;
class Label;
struct BlockCFInfo;


///<summary>
/// A block of elements. Manages the memory for all contained elems.
///</summary>
/// The Dragon Book, 1st edition, Code Generation, page 528:
///"A basic block is a sequence of consecutive statements in which
/// flow of control enters at the beginning and leaves at the end
/// without halt or posibility of branching except at the end."
///
/// Another way to think about a block is in terms of the Composite
/// design pattern; do not confuse a back-end block as defined here
/// with a structured block of source code.
struct block : public elem
{
public:
    typedef std::deque<elem*> container_type;
    typedef container_type::iterator iterator;
    typedef container_type::const_iterator const_iterator;

private:
    mutable int32_t delta_;
    mutable int32_t maxDelta_;
    Loc& loc_;
    bool hasLeave_ : 1;
    bool hasRet_ : 1;
    bool useBrackets_ : 1;  // emit block in { } brackets?
    bool hasTls_ : 1;
    unsigned short depth_;  // indentation depth

    //the parent of this block or NULL, for traversing
    //blocks -- do not confuse with element's owner_, which
    //refers to memory ownership 
    block* parent_;

    std::auto_ptr<BlockCFInfo> cfInfo_;
    container_type elems_;  // managed elems

    //private on purpose, the AutoManaged ctor
    //automatically adds the element to a block
    template<typename T> void add(AutoManaged<T>&);

    // should never add a label, use getLabel() instead
    void add(Label&);

    void emitTail(std::wostream&, elem* lastEmitted);
    void deleteElems(bool checkIfDeletable);
    void ensureCFInfo();

public:
    block(block* parent, Loc& loc, unsigned depth);
    ~block();

    block* parent() const { return parent_; }

    //control indentation depth of generated IL
    unsigned depth() const { return depth_; }
    void setDepth(unsigned depth) { depth_ = depth; }
    void incDepth() { ++depth_; }

    // Location in the source code that corresponds
    // to the beginning of this block
    Loc& loc() { return loc_; }

    Label& getLabel(bool incRef = true);
    Label* hasLabel() const;
    block* hasRetBlock() const;
    block* getRetBlock();
    block* isBlock() { return this; }

    // create a new block as a child of this one
    block* addNewBlock(Loc* = NULL);
    block* addNewBlock(Loc& loc) 
    {
        return addNewBlock(&loc);
    }

    virtual Assembly* isAssembly() { return NULL; }

    bool isReferenced() const;

    ///return false if it has EmitHooks that return false from preEmit,
    ///or if owner_ != NULL and !owner_->isEmitable()
    bool isEmitable();

    //****************************************************************
    //The block also acts as a container of elem-s, and
    //provides an STL-like interface
    size_t size() const { return elems_.size(); }
    bool empty() const { return elems_.empty(); }
    const_iterator begin() const { return elems_.begin(); }
    const_iterator end() const { return elems_.end(); }
    iterator begin() { return elems_.begin(); }
    iterator end() { return elems_.end(); }
    void clear();
    //NOTE: this does not delete the elem
    iterator erase(iterator i)
    {
        assert((*i)->getOwner() == this);
        (*i)->setOwner(NULL); 
        return elems_.erase(i);
    }
    /// return last element or NULL
    elem* back() { return elems_.empty() ? NULL : elems_.back(); }
    /// Remove last elem. Caller must make sure block is not empty
    void pop_back(bool deleteElem = true);

    elem* front() { return elems_.empty() ? NULL : elems_.front(); }
    void pop_front(bool deleteElem = true);

    elem* operator[] (size_t n) const { return elems_.at(n); }

    //****************************************************************

    /// Swap last two elems.
    void swapTail();

    void moveElemsTo(block&);

    /// Add element to block, and take ownership of it
    elem* add(elem& e, bool front = false);

    template<typename T>
    elem* add(std::auto_ptr<T> e)
    {
        elem* result = add(*e);
        e.release();
        return result;
    }

    /// Add a return instruction
    void addRet(Loc*, Variable* ret = NULL);
   
    void addDefaultEH();
    void addEHTranslator();
    void setEH(ExceptionHandler*);

    elem* emitILAsm(std::wostream& out);

    int32_t getStackDelta() const;
    int32_t getMaxStackDelta() const;

    void setParallelCF(block& block);
    block* hasParallelCF() const;

    ExceptionHandler* hasEH() const;
    ExceptionHandler* getEH();

    bool hasLeaveInstruction() const { return hasLeave_; }
    bool hasRetInstruction() const { return hasRet_; }

    bool hasTls() const { return hasTls_; }
    void setHasTls() { hasTls_ = true; }

    void generateMethods();

    /// Call this with "true" if the block should
    /// be emitted within enclosing curly brackets
    void setUseBrackets(bool useBrackets) { useBrackets_ = useBrackets; }
};


///<summary>
/// A Block of Local Variables
///</summary>
class LocalsBlock : public block
{
    elem* emitILAsm(std::wostream&);

public:
    LocalsBlock(block* parent, Loc& loc, unsigned depth)
        : block(parent, loc, depth)
    { }
    bool empty() const;
};


template<typename T>
class AutoManaged : public T
{
protected:
    ~AutoManaged()
    {
        if (block* blk = T::getOwner())
        {
            if (blk->back() == this)
            {
                blk->pop_back(false);
            }
        }
    }
    explicit AutoManaged(block& blk)
    {
        blk.add(static_cast<elem&>(*this));
    }
    template<typename U>
    AutoManaged(block& blk, U& arg) : T(arg)
    {
        blk.add(static_cast<elem&>(*this));
    }
    template<typename U, typename V>
    AutoManaged(block& blk, U& arg0, V arg1) : T(arg0, arg1)
    {
        blk.add(static_cast<elem&>(*this));
    }
    template<typename U, typename V>
    AutoManaged(block* blk, U& arg0, V arg1) : T(blk, arg0, arg1)
    {
        DEREF(blk).add(static_cast<elem&>(*this));
    }
};

#endif // BLOCK_H__656F4873_FA51_477B_9508_85F191FC5376
