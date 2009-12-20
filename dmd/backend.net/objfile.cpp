// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: objfile.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <fstream>
#include <sstream>
#include "aggregate.h"
#include "declaration.h"
#include "enum.h"
#include "init.h"
#include "backend.net/backend.h"
#include "backend.net/modemit.h"
#include "backend.net/msil/assembly.h"

using namespace std;


void ClassDeclaration::toObjFile(int)
{
    assert(!this->csym);
    BackEnd::getModuleScope(loc).createClass(*this);
}


void Dsymbol::toObjFile(int /* multiobj */) 
{
}


void EnumDeclaration::toObjFile(int /* multiobj */)
{
    assert(!this->csym);
    BackEnd::getModuleScope(loc).createEnum(*this);
}


void FuncDeclaration::toObjFile(int /* multiobj */)
{
    if (!this->csym)
    {
        BackEnd::getModuleScope(loc).createFunction(*this);
    }
}


void InterfaceDeclaration::toObjFile(int)
{
    assert(!this->csym);
    BackEnd::getModuleScope(loc).createClass(*this);
}


void StructDeclaration::toObjFile(int)
{
    assert(!this->csym);
    if (isUnionDeclaration())
    {
        error(loc, ": unions are not supported in %s", BackEnd::name());
    }
    else
    {
        BackEnd::getModuleScope(loc).createStruct(*this);
    }
}


void TypedefDeclaration::toObjFile(int)
{
}


void TypeInfoDeclaration::toObjFile(int)
{
    if (!csym)
    {
        if (!init)
        {
            NewExp* newExp = new NewExp(loc, NULL, NULL, type, NULL);
            init = new ExpInitializer(loc, newExp);
            init->semantic(scope, type);
        }
        BackEnd::getModuleScope(loc).createVar(*this);
    }
}


void VarDeclaration::toObjFile(int /* multiobj */)
{
    if (!csym)
    {
        BackEnd::getModuleScope(loc).createVar(*this);
    }
}


void obj_start(char *srcfile)
{
    //IF_DEBUG(if (srcfile) clog << __FUNCSIG__ << ": " << srcfile << endl;)
    //NOT_IMPLEMENTED();
}


void obj_end(Library* library, File* objfile)
{
    if (global.params.oneobj)
    {
        const char* filename = DEREF(objfile).toChars();
        wofstream f(filename, ios_base::app);
        if (!f)
        {
            throw runtime_error(string("could not open file ") + filename);
        }
#if defined(DEBUG_TO_SCREEN)
        wostringstream buf;
        BackEnd::instance().getAssembly().emitEnd(buf);
        f << buf.str();
        wclog << buf.str();
        wclog.flush();
#else
        BackEnd::instance().getAssembly().emitEnd(f);
#endif
    }
    else
    {
        NOT_IMPLEMENTED();
    }
}


void obj_append(Dsymbol *s) { NOT_IMPLEMENTED(); }
void obj_write_deferred(Library *library) { NOT_IMPLEMENTED(); }
void obj_includelib(const char*) { NOT_IMPLEMENTED(); }
void obj_startaddress(Symbol*) { NOT_IMPLEMENTED(); }
