#ifndef EXCEPT_H__E8785776_4445_4764_B8DD_4709E1DDED39
#define EXCEPT_H__E8785776_4445_4764_B8DD_4709E1DDED39
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: except.h 24625 2009-07-31 01:05:55Z unknown $
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
#include "backend.net/array.h"
#include "backend.net/block.h"
#include "backend.net/elem.h"


class AssertError : public AutoManaged<elem>
{
    AssertError(block& block) : AutoManaged<elem>(block)
    {
    }
    elem* emitILAsm(std::wostream&);

public:
    static AssertError* create(block& block)
    {
        AssertError* ex = new AssertError(block);
        assert(ex->getOwner() == &block);
        return ex;
    }
};

class IndexOutOfRangeException : public AutoManaged<elem>
{
    IndexOutOfRangeException(block& blk) : AutoManaged<elem>(blk) { }
    int32_t getStackDelta() const { return 1; }

    elem* emitILAsm(std::wostream&);

public:
    static IndexOutOfRangeException* create(block& block)
    {
        IndexOutOfRangeException* ex = new IndexOutOfRangeException(block);
        assert(ex->getOwner() == &block);
        return ex;
    }
};


class ExceptionHandler : public elem
{
    Loc loc_;

protected:
    block end_;

public:
    ExceptionHandler(block*, Loc& loc, unsigned depth);
    ~ExceptionHandler();

    block& getRetBlock();
    block& getEndBlock() { return end_; }
    block* parent() const { return end_.parent(); }

    elem* emitILAsm(std::wostream&);
    virtual void emitILAsmImpl(std::wostream&, const std::wstring& label) = 0;

    //default max is 1 because the catch blocks are enterred
    //with the exception object on the stack
    virtual int32_t getMaxStackDelta() const { return 1; }
};


struct Catch;
struct IRState;
struct Statement;
struct TryCatchStatement;
struct TryFinallyStatement;


class TryCatchHandler : public ExceptionHandler
{
    ArrayAdapter<Catch*> catches_;
    block handlers_;

public:
    TryCatchHandler(block*, TryCatchStatement&, unsigned depth);
    TryCatchHandler(block*, Statement&, unsigned depth);

    void emitILAsmImpl(std::wostream&, const std::wstring&);

    void genCatchHandlers(IRState&);
    block* genFinally(IRState&, Statement*);

    virtual int32_t getMaxStackDelta() const
    {
        //add one because the block is entered
        //with the exception on the stack
        return handlers_.getMaxStackDelta() + 1;
    }
};

#endif // EXCEPT_H__E8785776_4445_4764_B8DD_4709E1DDED39

