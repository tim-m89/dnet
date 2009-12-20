// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: expr.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008 Cristian L. Vlasceanu

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
#include "backend.net/irstate.h"
#include "backend.net/utils.h"


int binary(const char *p , const char **tab, int high)
{
    NOT_IMPLEMENTED(0);
}

#define TO_ELEM_IMPL(E) \
    elem* E::toElem(IRState* state) { return DEREF(state).toElem(*this); }

TO_ELEM_IMPL(AddAssignExp)
TO_ELEM_IMPL(AddExp)
TO_ELEM_IMPL(AddrExp)
TO_ELEM_IMPL(AndAndExp)
TO_ELEM_IMPL(AndAssignExp)
TO_ELEM_IMPL(AndExp)
TO_ELEM_IMPL(ArrayLengthExp)
TO_ELEM_IMPL(ArrayLiteralExp)
TO_ELEM_IMPL(AssertExp)
TO_ELEM_IMPL(AssignExp)
TO_ELEM_IMPL(AssocArrayLiteralExp)
TO_ELEM_IMPL(BoolExp)
TO_ELEM_IMPL(CallExp)
TO_ELEM_IMPL(CastExp)
TO_ELEM_IMPL(CatAssignExp)
TO_ELEM_IMPL(CatExp)
TO_ELEM_IMPL(CmpExp)
TO_ELEM_IMPL(ComExp)
TO_ELEM_IMPL(CommaExp)
TO_ELEM_IMPL(ComplexExp)
TO_ELEM_IMPL(CondExp)
TO_ELEM_IMPL(DeclarationExp)
TO_ELEM_IMPL(DelegateExp)
TO_ELEM_IMPL(DeleteExp)
TO_ELEM_IMPL(DivAssignExp)
TO_ELEM_IMPL(DivExp)
TO_ELEM_IMPL(DotTypeExp)
TO_ELEM_IMPL(DotVarExp)
TO_ELEM_IMPL(EqualExp)
TO_ELEM_IMPL(Expression)
TO_ELEM_IMPL(FuncExp)
TO_ELEM_IMPL(HaltExp)
TO_ELEM_IMPL(IdentityExp)
TO_ELEM_IMPL(IndexExp)
TO_ELEM_IMPL(InExp)
TO_ELEM_IMPL(IntegerExp)
TO_ELEM_IMPL(MinAssignExp)
TO_ELEM_IMPL(MinExp)
TO_ELEM_IMPL(ModAssignExp)
TO_ELEM_IMPL(ModExp)
TO_ELEM_IMPL(MulAssignExp)
TO_ELEM_IMPL(MulExp)
TO_ELEM_IMPL(NegExp)
TO_ELEM_IMPL(NewExp)
TO_ELEM_IMPL(NotExp)
TO_ELEM_IMPL(NullExp)
TO_ELEM_IMPL(OrAssignExp)
TO_ELEM_IMPL(OrExp)
TO_ELEM_IMPL(OrOrExp)
TO_ELEM_IMPL(PostExp)
TO_ELEM_IMPL(PtrExp)
TO_ELEM_IMPL(RealExp)
TO_ELEM_IMPL(RemoveExp)
TO_ELEM_IMPL(ScopeExp)
TO_ELEM_IMPL(ShlAssignExp)
TO_ELEM_IMPL(ShlExp)
TO_ELEM_IMPL(ShrAssignExp)
TO_ELEM_IMPL(ShrExp)
TO_ELEM_IMPL(SliceExp)
TO_ELEM_IMPL(StringExp)
TO_ELEM_IMPL(StructLiteralExp)
TO_ELEM_IMPL(SymbolExp)
TO_ELEM_IMPL(ThisExp)
TO_ELEM_IMPL(TupleExp)
TO_ELEM_IMPL(TypeExp)
TO_ELEM_IMPL(UshrAssignExp)
TO_ELEM_IMPL(UshrExp)
TO_ELEM_IMPL(XorAssignExp)
TO_ELEM_IMPL(XorExp)
