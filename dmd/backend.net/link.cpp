// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: link.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <process.h>
#define unlink _unlink
#endif
#include "mars.h"
#include "backend.net/array.h"
#include "backend.net/backend.h"
#include "backend.net/utils.h"

using namespace std;


/// Replace the functionality of link.c -- call ILASM instead of traditional linker
int runLINK()
{
#if __linux__
    string cmdbuf = "ilasm2 "; // for generics support
#else
    string cmdbuf = "ilasm ";
#endif
//#ifndef DEBUG
    cmdbuf += " /quiet ";
//#endif
    if (global.params.debuglevel || global.params.symdebug)
    {
        cmdbuf += "/debug ";
    }
    if (!g_haveEntryPoint)
    {
        cmdbuf += "/dll ";
    }
    ArrayAdapter<const char*> files(global.params.objfiles);
    assert(!files.empty());
    for (ArrayAdapter<const char*>::const_iterator i = files.begin(); i != files.end(); ++i)
    {
        cmdbuf += *i;
        cmdbuf += ' ';
    }
    FileName* fn = NULL;
    string outFile;
    if (global.params.exefile)
    {
        cmdbuf += "/exe /output=";
        cmdbuf += global.params.exefile;
        outFile = global.params.exefile;
    }
    else if (g_haveEntryPoint)
    {
        fn = FileName::forceExt(files[0], "exe");
        global.params.exefile = strdup(fn->toChars());
    }
    else
    {
        fn = FileName::forceExt(files[0], "dll");
    }
    outFile = fn->name();
    delete fn;
    IF_DEBUG(clog << cmdbuf << endl);

#if _WIN32
    int result = _spawnlp(_P_WAIT, "ilasm", cmdbuf.c_str(), NULL);
    IF_DEBUG(
        // ignore "Return from .ctor when this is uninitialized"
        // ignore "Return type is ByRef, TypedReference, ArgHandle, or ArgIterator."
        string verify = "PEVERIFY /VERBOSE /IGNORE=8013184F /IGNORE=0x80131870 ";
        if (!g_haveUnverifiableCode && (result == 0))
        {
            if (g_havePointerArithmetic)
            {
                //ignore "Expected numeric type on the stack"
                verify += "/IGNORE=8013185D ";
            }
            result = system((verify + outFile).c_str());
        }
    )
#else
    int result = system(cmdbuf.c_str());
#endif
    return result;
}


int runProgram()
{
    int result = 0;
    if (global.params.exefile)
    {
#if _WIN32
        //todo: support passing args to exefile
        result = _spawnlp(_P_WAIT, global.params.exefile, global.params.exefile, NULL);
#else
        result = system(global.params.exefile); 
#endif
    }
    return result;
}


void deleteExeFile()
{
    if (global.params.exefile)
    {
        unlink(global.params.exefile);
    }
}
