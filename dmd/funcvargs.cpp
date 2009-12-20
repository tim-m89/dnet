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
#include <stdlib.h>
#include "mars.h"
#include "declaration.h"
//
// $Id: funcvargs.cpp 357 2009-04-29 09:25:53Z cristiv $
//
// Copyright (c) 2009 Cristian L. Vlasceanu
//
#include "id.h"
#include "aggregate.h"
#include "scope.h"


namespace {
    class TypeObject : public TypeClass
    {
    public:
        explicit TypeObject(ClassDeclaration* sym) : TypeClass(sym)        
        {
        }

        MATCH implicitConvTo(Type* toType)
        {
            if (toType == this)
            {
                return MATCHexact;
            }
            return MATCHconvert;
        }
    };
}


static Type* objType = NULL;


void FuncDeclaration::varArgs(Scope* sc2,
                              TypeFunction*,
                              VarDeclaration*& /* argptr */,
                              VarDeclaration*& /* arguments */
                              )
{
    if (!objType)
    {
        ClassDeclaration* cd = new ClassDeclaration(0, Id::object, NULL);
        objType = new TypeObject(cd);
    }
    Type* t = objType->arrayOf();
	v_arguments = new VarDeclaration(0, t, Id::_arguments, NULL);
    v_arguments->semantic(sc2);
    sc2->insert(v_arguments);
    v_arguments->parent = this;
    v_arguments->storage_class = STCparameter;
    if (!parameters)
    {
        parameters = new Dsymbols;
    }
}
