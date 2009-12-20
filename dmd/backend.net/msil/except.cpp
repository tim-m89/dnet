// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: except.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <iostream>
#include <string>
#include "statement.h"
#include "backend.net/irstate.h"
#include "backend.net/type.h"
#include "backend.net/msil/except.h"
#include "backend.net/msil/label.h"
#include "backend.net/msil/variable.h"

using namespace std;



elem* AssertError::emitILAsm(wostream& out)
{
    //construct exception object on the evaluation stack
    indent(out) << "newobj instance void [dnetlib]core.AssertError::.ctor(string)\n";
    return this;
}


elem* IndexOutOfRangeException::emitILAsm(wostream& out)
{
    indent(out) << "newobj instance void [mscorlib]System.IndexOutOfRangeException::.ctor()\n";
    return this;
}


ExceptionHandler::ExceptionHandler(block* parent, Loc& loc, unsigned depth)
    : loc_(loc)
    , end_(parent, loc, depth)
{
}


ExceptionHandler::~ExceptionHandler()
{
}


block& ExceptionHandler::getRetBlock()
{
    block* blk = DEREF(parent()).getRetBlock();
    if (!blk)
    {
        BackEnd::fatalError(loc_, "null ret block");
    }
    return *blk;
}


//end the block with a LEAVE instruction (if necessary)
//then emit handlers (catch or finally blocks)
elem* ExceptionHandler::emitILAsm(wostream& out)
{
    assert(DEREF(getOwner()).getEH() == this);
    wstring label;
    if (!DEREF(getOwner()).hasLeaveInstruction())
    {
        label = end_.getLabel().getName();
        indent(out) << "leave " << label << "\n";
    }
    unsigned depth = this->depth();
    if (depth) --depth;
    indent(out, depth) << "}\n";
    emitILAsmImpl(out, label);
    end_.emitIL(out);
    return this;
}


TryCatchHandler::TryCatchHandler(block* parent, Statement& stat, unsigned depth)
    : ExceptionHandler(parent, stat.loc, depth)
    , handlers_(parent, stat.loc, depth)
{
}


TryCatchHandler::TryCatchHandler(block* parent, TryCatchStatement& stat, unsigned depth)
    : ExceptionHandler(parent, stat.loc, depth)
    , catches_(stat.catches)
    , handlers_(parent, stat.loc, depth)
{
}



namespace {
    /// <summary>
    /// Helper class for emitting "catch" blocks
    /// </summary>
    class CatchBlock : public block
    {
        ExceptionHandler& eh_;
        Catch& catch_;

    public:
        CatchBlock(block* parent, ExceptionHandler& eh, unsigned depth, Catch& c)
            : block(parent, c.loc, depth)
            , eh_(eh)
            , catch_(c)
        {
        }

        elem* emitILAsm(wostream& out)
        {
            unsigned depth = this->depth();
            if (depth) --depth;
            TYPE& exType = toILType(catch_.loc, catch_.type);
            if (exType.isObjectType())
            {
                indent(out, depth) << "catch " << exType.getSuffix() << "\n";
            }
            else
            {
                indent(out, depth) << "catch " << exType.name() << "\n";
            }
            indent(out, depth) << "{\n";
            BackEnd::instance().emitLineInfo(*this, catch_.loc, out);

            block::emitILAsm(out);

            if (!hasLeaveInstruction())
            {
                const std::wstring& label = eh_.getEndBlock().getLabel().getName(); 
                indent(out) << "leave " << label << '\n';
            }
            indent(out, depth) << "}\n";
            return this;
        }
    };


    class FinalBlock : public block
    {
        ExceptionHandler& eh_;

    public:
        FinalBlock(ExceptionHandler& eh, Loc& loc, unsigned depth)
            : block(NULL, loc, depth)
            , eh_(eh)
        {
        }

        elem* emitILAsm(wostream& out)
        {
            unsigned depth = this->depth();
            if (depth) --depth;

            indent(out, depth) << "finally\n";
            indent(out, depth) << "{\n";

            block::emitILAsm(out);

            indent(out) << "endfinally\n";
            indent(out, depth) << "}\n";
            return this;
        }
    };
}


void TryCatchHandler::emitILAsmImpl(wostream& out, const wstring&)
{
    handlers_.emitIL(out);
}


void TryCatchHandler::genCatchHandlers(IRState& irstate)
{
    const unsigned depth = handlers_.depth();
    for (ArrayAdapter<Catch*>::iterator i = catches_.begin(); i != catches_.end(); ++i)
    {
        Catch& c = DEREF(*i);
        CatchBlock* block = new CatchBlock(&handlers_, *this, depth + 1, c);
        handlers_.add(*block);
        if (VarDeclaration* varDecl = c.var)
        {
            // store exception ref in local variable
            Variable* v = irstate.createVar(*varDecl);
            v->store(*block, &c.loc);
        }
        if (c.handler)
        {
            irstate.emitInBlock(*block, *c.handler);
        }
    }
}


block* TryCatchHandler::genFinally(IRState& irstate, Statement* stat)
{
    Loc& loc = stat ? stat->loc : handlers_.loc();
    FinalBlock* finalBlock = new FinalBlock(*this, loc, handlers_.depth() + 1);
    handlers_.add(*finalBlock);
    if (stat)
    {
        irstate.emitInBlock(*finalBlock, *stat);
    }
    return finalBlock;
}
