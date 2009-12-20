// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: toir.cpp 367 2009-05-07 05:20:50Z cristiv $
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
#include <cstdlib>
#include "expression.h"
#include "statement.h"
#include "backend.net/array.h"
#include "backend.net/irstate.h"
#include "backend.net/hiddenmethod.h"


#define TO_IR_IMPL(S) \
    void S::toIR(IRState* state) { DEREF(state).toIR(*this); }

#define TO_IR_NOT_IMPL(S) \
    void S::toIR(IRState*) { ::error(loc, STRINGIZE(S) ": not implemented"); }

using namespace std;


void CompoundStatement::toIR(IRState* irstate)
{
    ArrayAdapter<Statement*> stats(statements);
    for (ArrayAdapter<Statement*>::iterator i = stats.begin(); i != stats.end(); ++i)
    {
        (*i)->toIR(irstate);
    }
}


void ScopeStatement::toIR(IRState* state)
{
    if (statement)
    {
        statement->toIR(state);
    }
}


TO_IR_IMPL(AsmStatement)
TO_IR_IMPL(BreakStatement)
TO_IR_IMPL(CaseStatement)
TO_IR_IMPL(ContinueStatement)
TO_IR_IMPL(DefaultStatement)
TO_IR_IMPL(DoStatement)
TO_IR_IMPL(ExpStatement)
TO_IR_IMPL(ForeachStatement)
TO_IR_IMPL(ForeachRangeStatement)
TO_IR_IMPL(ForStatement)
TO_IR_IMPL(GotoCaseStatement)
TO_IR_IMPL(GotoDefaultStatement)
TO_IR_IMPL(HiddenMethodStatement)
TO_IR_IMPL(IfStatement)
TO_IR_IMPL(LabelStatement)
TO_IR_IMPL(ReturnStatement)
TO_IR_IMPL(OnScopeStatement)
TO_IR_IMPL(SynchronizedStatement)
TO_IR_IMPL(SwitchErrorStatement)
TO_IR_IMPL(SwitchStatement)
TO_IR_IMPL(ThrowStatement)
TO_IR_IMPL(TryCatchStatement)
TO_IR_IMPL(TryFinallyStatement)
TO_IR_IMPL(UnrolledLoopStatement)
TO_IR_IMPL(WhileStatement)
TO_IR_IMPL(WithStatement)


TO_IR_NOT_IMPL(PragmaStatement)
TO_IR_NOT_IMPL(GotoStatement)
TO_IR_NOT_IMPL(Statement)
TO_IR_NOT_IMPL(VolatileStatement) // deprecated in D 2.0

