//
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: test.cpp 367 2009-05-07 05:20:50Z cristiv $
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
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "backend.net/test.h"
#include "backend.net/utils.h"
#if _WIN32
 #include <windows.h>
 #include <io.h>
 #define unlink _unlink
#endif
#include "root.h"

using namespace std;


CompilationTest::CompilationTest(const char* name, const char* cmd, const char* desc)
    : expectedResult_(0)
{
    if (name) name_ = name;
    if (desc) desc_ = desc;
    assert(cmd);
    cmd_ = cmd;
}


CompilationTest::~CompilationTest()
{
    //remove the binary, but leave sources behind, for post-mortem troubleshooting
    unlink(cmd_.c_str());
}


auto_ptr<ostream> CompilationTest::createFile(const char* name)
{
    auto_ptr<ostream> of (new ofstream(name));
    if (!*of)
    {
        throw runtime_error(string("could not create file: ") + name);
    }
    files_.push_back(name);
    return of;
}


void CompilationTest::cleanup()
{
    for (vector<string>::const_iterator i = files_.begin(); i != files_.end(); ++i)
    {
        unlink(i->c_str());
        FileName* fn = FileName::forceExt(i->c_str(), "il");
        unlink(fn->name());
        delete fn;
        fn = FileName::forceExt(i->c_str(), "pdb");
        unlink(fn->name());
        delete fn;
    }
}


bool CompilationTest::run(const char* flags)
{
#if _WIN32    
    string compilation = "dnet -I../../druntime/import ";
#else
    string compilation = "./dnet -I../druntime/import ";
#endif
    if (!flags_.empty())
    {
        compilation += flags_;
    }
    if (flags)
    {
        compilation += flags;
    }
    for (vector<string>::iterator i = files_.begin(); i != files_.end(); ++i)
    {
        compilation += *i + ' ';
    }
    clog << compilation << endl;
    bool result = false;
    
    if (system(compilation.c_str()) == 0)
    {
        clog << cmd_ << endl;
        if (cmd_.empty())
        {
            IF_WIN_DEBUG(OutputDebugStringA((string("ok: ") + name() + "\n").c_str()));
            result = true;
        }
        else
        {
            string cmd = 
        #if linux
                "mono " + cmd_;
        #else
                cmd_;
        #endif
            const int ret = system(cmd.c_str());
            IF_DEBUG(clog << cmd << " returned: " << hex << ret << dec << endl);
            result = (ret == expectedResult_);
            IF_WIN_DEBUG(cmd = (result ? "ok: " : "failed: ") + name() + "\n";
                         OutputDebugStringA(cmd.c_str());
            )
        }
    }
    return result;
}

