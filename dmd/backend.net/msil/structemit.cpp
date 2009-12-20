// $Id: structemit.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include "aggregate.h"
#include "declaration.h"    // for STCtls
#include "backend.net/backend.h"
#include "backend.net/irstate.h"
#include "backend.net/type.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/structemit.h"


Struct::Struct(StructDeclaration& decl, unsigned depth) : Aggregate(decl, depth + 1)
{
    if (decl.csym)
    {
        BackEnd::instance().getAssembly().add(*decl.csym);
    }
    decl.csym = this;
    IRState irstate(getBlock());
    irstate.setDecl(&decl);
    generateMembers();
    getBlock().generateMethods();
}


elem* Struct::emitILAsm(std::wostream& out)
{
    if (StructDeclaration* decl = getDecl().isStructDeclaration())
    {
        indent(out << "\n");
        // explicit layout is needed for empty classes
        const char* layout = decl->fields.dim ? "auto" : "explicit";

        // Unlike .NET valuetype classes, D structs participate in GC;
        // the destructor is called by the GC and may call __invariant
        // (if defined). To keep these D features in D.NET we implement
        // structs as deriving off [mscorlib]System.Object (which makes
        // them heavier weight; the lightweight can only be achieved if
        // we cripple the features mentioned above.
#if VALUETYPE_STRUCT
        out << ".class value " << decl->prot() << layout << " ansi sealed ";
        out << className(*decl) << " extends [mscorlib]System.ValueType\n";
#else
        out << ".class " << decl->prot() << layout << " sequential sealed ";
        out << className(*decl) << " extends [mscorlib]System.Object\n";
#endif
        if (decl->storage_class & STCtls)
        {
            out << "beforefieldinit ";
        }
        indent(out) << "{\n";
        
        //IL does not allow a zero-sized class;
        //.size requires explicit layout (see above)
        if (decl->fields.dim == 0)
        {
            size_t size = decl->structsize;
            if (!size) size = 1;
            getBlock().indent(out) << ".size " << size << "\n";
        }

        if (!getStaticInitBlock().empty())
        {
            emitDefaultStaticCtor(*this, out);
        }
        emitDefaultTors(out);
        getBlock().emitILAsm(out);
        indent(out) << "} // end of struct " << className(*decl) << "\n\n";
    }
    return this;
}



void Struct::emitBase(const char*       func,
                      std::wostream&    out,
                      unsigned          margin
                      )
{
#if !defined( VALUETYPE_STRUCT )

    indent(out, margin) << "ldarg.0\n";
    indent(out, margin);
    if (strcmp(func, "__dtor") == 0)
    {
        out << "call instance void object::Finalize()\n";
    }
    else
    {
        out << "call instance void object::.ctor()\n";
    
    }
#endif
}
