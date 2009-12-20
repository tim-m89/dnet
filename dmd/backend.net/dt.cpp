// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: dt.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include "backend.net/backend.h"
#include "backend.net/type.h"
#include "declaration.h"
#include "expression.h"
#include "init.h"

using namespace std;


dt_t* ArrayInitializer::toDt()
{
    return BackEnd::toDt(*this);
}


dt_t** ArrayLiteralExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** ComplexExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }


dt_t* ExpInitializer::toDt()
{
    TYPE& type = toILType(loc, DEREF(exp).type);
    if (DEREF(exp).op == TOKblit) // default init?
    {
        if (StructType* sType = type.isStructType())
        {
#if VALUETYPE_STRUCT
            return BackEnd::construct(loc, *sType);
#else
            return NULL;
#endif
        }
    }
    // Do not generate default initializers (op == TOKblit)
    // for primitive types,
    // since we always generate MSIL "locals init" blocks.

    // A side benefit of not generating default initializers is
    // that the refcount of the variable is not bumped up; local
    // variables with a zero refcount are not generated.
    if (!type.isPrimitiveType()
        || (DEREF(exp).op != TOKblit)
        || BackEnd::inNestedVarScope(loc)
        )
    {
        elem* e = BackEnd::toElem(DEREF(exp));
        return new dt_t(e, exp->op);
    }
    return NULL;
}



// In the D language void initializers are the way to explicitly specify
// uninitialized variables; variables are initialized implicitly.
dt_t* VoidInitializer::toDt()
{
    return NULL;
}


dt_t** Expression::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t* Initializer::toDt() { NOT_IMPLEMENTED(0); }
dt_t** IntegerExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** NullExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** RealExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** StringExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t* StructInitializer::toDt() { NOT_IMPLEMENTED(0); }
dt_t** StructLiteralExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** SymOffExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** TypeSArray::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** TypeStruct::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** Type::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** TypeTypedef::toDt(dt_t**) { NOT_IMPLEMENTED(0); }
dt_t** VarExp::toDt(dt_t**) { NOT_IMPLEMENTED(0); }

void TypeInfoDeclaration::toDt(dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoArrayDeclaration::toDt(dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoAssociativeArrayDeclaration::toDt(dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoClassDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoConstDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoDelegateDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoEnumDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoFunctionDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoInterfaceDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoInvariantDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoPointerDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoSharedDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoStaticArrayDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoStructDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoTupleDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
void TypeInfoTypedefDeclaration::toDt (dt_t**) { NOT_IMPLEMENTED(); }
