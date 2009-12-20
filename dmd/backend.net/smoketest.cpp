//
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: smoketest.cpp 370 2009-05-09 19:45:40Z cristiv $
//
// Copyright (c) 2008 - 2009 Cristian L. Vlasceanu
// Copyright (c) 2008, 2009 Ionut-Gabriel Burete

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
#if _WIN32
#pragma warning (disable : 4996)
#endif
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "backend.net/utils.h"
#include "backend.net/test.h"

using namespace std;

#define INPUT(t, n, s) *t.createFile(n) << (s);

static deque<Test*> smokeTest;
static size_t passedCount = 0;


static CompilationTest& cTest(const char* name, const char* cmd, const char* desc = 0)
{
    CompilationTest* test = new CompilationTest(name, cmd, desc);
    smokeTest.push_back(test);
    return *test;
}


static bool run(const char* flags = "")
{
    bool result = true;
    const size_t count = smokeTest.size();

    for (size_t i = 0; !smokeTest.empty(); )
    {
        Test* t = smokeTest.front();
        if (strcmp(flags, "-cleanup") == 0)
        {
            t->cleanup();
            cout << "cleaned: " << t->name() << endl;
            ++passedCount;
        }
        else
        {
            IF_WIN_DEBUG({
                ostringstream progress; 
                progress << ++i << "/" << count << " ";
                OutputDebugStringA(progress.str().c_str());
            })
            if (t->run(flags))
            {
                cout << "OK: " << t->name() << endl;
                ++passedCount;
            }
            else
            {
                clog << "*** Test Failed: " << t->name() << " (" << t->cmd() << ")\n";
                result = false;
                break;
            }
        }
        delete t;
        smokeTest.pop_front();
    }
    return result;
}


//
// Populate smoke tests
//
static void init()
{
    //make sure the dnetlib assembly is in the tests directory
    if (
#if _WIN32
        system("csc /t:library /out:dnetlib.dll /debug+ ..\\runtime\\runtime.cs")
#elif linux
        system("gmcs -debug+ -t:library -out:dnetlib.dll ../d.net.compiler/runtime/runtime.cs")
#endif
        != 0)
    {
        throw runtime_error(strerror(errno));
    }

    { CompilationTest& t = cTest("void_main", "void.exe");
      INPUT(t, "void.d", "void main() { }");
    }
    { CompilationTest& t = cTest("main", "main.exe");
      INPUT(t, "main.d", "int main() { return 0; }");
    }

#if _WIN32
    //test Thread Local storage
    { CompilationTest& t = cTest("tls", "tls.exe");
      INPUT(t, "tls.d",
        "import System;\n"
        "import System.Threading;\n"
        "class Semaphore { bool go; }\n"
        "__thread int i = 42;\n"
        "Semaphore sem = new Semaphore;\n"
        "void main() {\n"
        "    void asyncWork() {\n"
        "        while (true) {\n"
        "            synchronized(sem) {\n"
        "                if (sem.go) break;\n"
        "            }\n"
        "        }\n"
        "        i = 13;\n"
        "   }\n"
        "   System.Threading.Thread t = new System.Threading.Thread(&asyncWork);\n"
        "   t.Start();\n"
        "   synchronized(sem) {\n"
        "        sem.go = true;\n"
        "   }\n"
        "   t.Join();\n"
        "   Console.WriteLine(i);\n"
        "   assert(i == 42);\n"
        "}\n")
    }
    { CompilationTest& t = cTest("tls 2", "tls2.exe");
      INPUT(t, "tls2.d",
        "import System;\n"
        "import System.Threading;\n"
        "class Semaphore {\n"
        "   bool go;\n"
        "   __thread int i = 42;\n"
        "}\n"
        "Semaphore sem = new Semaphore;\n"
        "void main() {\n"
        "    void asyncWork() {\n"
        "        while (true) {\n"
        "            synchronized(sem) {\n"
        "                if (sem.go) break;\n"
        "            }\n"
        "        }\n"
        "        Semaphore.i = 13;\n"
        "   }\n"
        "   System.Threading.Thread t = new System.Threading.Thread(&asyncWork);\n"
        "   t.Start();\n"
        "   synchronized(sem) {\n"
        "        sem.go = true;\n"
        "   }\n"
        "   t.Join();\n"
        "   Console.WriteLine(Semaphore.i);\n"
        "   assert(Semaphore.i == 42);\n"
        "}\n")
    }
#endif // _WIN32
    //{ CompilationTest& t = cTest("unreach", "unreach.exe");
    //  INPUT(t, "unreach.d", "int main() { return 0; return 1; }");
    //}
    { CompilationTest& t = cTest("assert", "assert.exe");
      INPUT(t, "assert.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    assert(i);         \n"
        "    assert(i == 1);    \n"
        "    return 0;          \n"
        "}")
    }

    { CompilationTest& t = cTest("Comparisons", "comparisons.exe");
      INPUT(t, "comparisons.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    int j = 2;         \n"
        "    int k = 2;         \n"
        "    assert(i < j);     \n"
        "    assert(i <= j);    \n"
        "    assert(k <= j);    \n"
        "    assert(k > i);     \n"
        "    assert(k >= i);    \n"
        "    assert(k >= j);    \n"
        "    assert(k == j);    \n"
        "    assert(i != j);    \n"

        "    string s =\"hell\";    \n"
        "    assert(s!=\"hello\");  \n"
        "    s ~= \"o\";            \n"
        "    assert(s==\"hello\");  \n"
        "    return 0;              \n"
        "}")
    }
    { CompilationTest& t = cTest("opEquals", "op_eq.exe");
      INPUT(t, "op_eq.d",
        "   struct Foo {                    \n"
        "       int i;                      \n"
        "       this(int j) { i = j; }      \n"
        "       bool opEquals(Foo other) {  \n"
        "           return i == other.i;    \n"
        "       }                           \n"
        "   }                               \n"
        "   int main()                      \n"
        "   {                               \n"
        "       Foo f = new Foo(42);        \n"
        "       Foo g = new Foo(42);        \n"
        "       Foo h = new Foo(13);        \n"
        "       assert(f == g);             \n"
        "       assert(f != h);             \n"
        "       return 0;                   \n"
        "   }\n")
    }

    { CompilationTest& t = cTest("opEquals 2", "op_eq2.exe");
      INPUT(t, "op_eq2.d",
        "   class Foo {\n"
        "       int i;\n"
        "       this(int j) { i = j; }\n"
        "       override bool opEquals(Object other) {\n"
        "           return i == (cast(Foo)other).i;\n"
        "       }\n"
        "   }\n"
        "   int main()\n"
        "   {\n"
        "       Foo a1[] = [new Foo(1), new Foo(2)];\n"
        "       Foo a2[] = [new Foo(1), new Foo(2)];\n"
        "       assert(a1 == a2);\n"
        "       assert(a1 !is a2);\n"
        "       Foo f1 = new Foo(42);\n"
        "       Foo f2 = f1;\n"
        "       Foo f3 = new Foo(42);\n"
        "       assert(f1 == f2);\n"
        "       assert(f1 == f3);\n"
        "       assert(f1 is f2);\n"
        "       assert(f1 !is f3);\n"
        "       return 0;\n"
        "   }\n")
    }

    { CompilationTest& t = cTest("Logical And", "logical_and.exe");
      INPUT(t, "logical_and.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    int j = 2;         \n"
        "    assert(i && true); \n"
        "    assert(i && j);    \n"
        "    return 0;          \n"
        "}")
    }

    { CompilationTest& t = cTest("Logical And, ShortCirc.", "logical_and_sc.exe");
      INPUT(t, "logical_and_sc.d",
        "class C {              \n"
        "   int i;              \n"
        "   bool f() {          \n"
        "       return i > 0;   \n"
        "   }                   \n"
        "}                      \n"
        "int main()             \n"
        "{                      \n"
        "   C c;                \n"
        "   if (c && c.f()) {   \n"
        "       assert(false);  \n"
        "   }                   \n"
        "   assert(!c || c.f());\n"
        "   return 0;           \n"
        "}")
    }

    { CompilationTest& t = cTest("Logical And If", "logical_and_if.exe");
      INPUT(t, "logical_and_if.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    int j = 2;         \n"
        "    bool ok = false;   \n"
        "    if (i==1 && j==2)  \n"
        "       ok = true;      \n"
        "    assert(ok);        \n"
        "    return 0;          \n"
        "}")
    }

    { CompilationTest& t = cTest("Logical Or", "logical_or.exe");
      INPUT(t, "logical_or.d",
        "int main()                     \n"
        "{                              \n"
        "    int i = 1;                 \n"
        "    int j = 2;                 \n"
        "    assert(i == 1 || false);   \n"
        "    assert(i == 1 || j == 2);  \n"
        "    assert(!(i==0 || j==0));   \n"
        "    return 0;                  \n"
        "}")
    }

    { CompilationTest& t = cTest("Bitwise And", "bitwise_and.exe");
      INPUT(t, "bitwise_and.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    int j = 2;         \n"
        "    int k = 3;         \n"
        "    assert((i & j)==0);\n"
        "    assert((i & k)==1);\n"
        "    return 0;          \n"
        "}")
    }

    { CompilationTest& t = cTest("Bitwise Or", "bitwise_or.exe");
        INPUT(t, "bitwise_or.d",
        "int main()             \n"
        "{                      \n"
        "    int i = 1;         \n"
        "    int j = 2;         \n"
        "    assert((i | j)==3);\n"
        "    return 0;          \n"
        "}")
    }

    { CompilationTest& t = cTest("Bitwise Xor", "bitwise_xor.exe");
        INPUT(t, "bitwise_xor.d",
        "void main()             \n"
        "{                      \n"
        "   int i = 1;          \n"
        "   int j = 2;          \n"
        "   assert((i ^ j)==3); \n"
        "}")
    }

    { CompilationTest& t = cTest("postinc", "postinc.exe");
      INPUT(t, "postinc.d",
        "void main() {          \n"
        "   int i;              \n"
        "   assert(i++ == 0);   \n"
        "   assert(i == 1);     \n"
        "}"
      )
    }

    //the result of post-inc is never referenced,
    //make sure we leave the stack in a clean state
    { CompilationTest& t = cTest("postinc_2", "postinc_2.exe");
      INPUT(t, "postinc_2.d",
        "void main() {              \n"
        "   for (int i; i != 3; i++)\n"
        "   {                       \n"
        "   }                       \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("post decrement", "postdec.exe");
      INPUT(t, "postdec.d",
        "void main() {          \n"
        "   int i = 1;          \n"
        "   assert(i-- == 1);   \n"
        "   assert(i == 0);     \n"
        "}"
      )
    }
    { CompilationTest& t = cTest("pre increment", "preinc.exe");
      INPUT(t, "preinc.d",
        "void main() {          \n"
        "   int i;              \n"
        "   assert(++i == 1);   \n"
        "   assert(i == 1);     \n"
        "}"
      )
    }
    { CompilationTest& t = cTest("pre decrement", "predec.exe");
      INPUT(t, "predec.d",
        "void main() {          \n"
        "   int i = 1;          \n"
        "   assert(--i == 0);   \n"
        "   assert(i == 0);     \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("for loop", "for.exe");
      INPUT(t, "for.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "for (; i; i--)         \n"
        "{                      \n"
        "}                      \n"
        "assert(i == 0);        \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("for / continue", "forcont.exe");
      INPUT(t, "forcont.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "for (; i; i--)         \n"
        "{                      \n"
        "   if (i==3) continue; \n"
        "   assert(i != 3);     \n"
        "}                      \n"
        "assert(i == 0);        \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("for w/ labeled break", "for_lbreak.exe");
      INPUT(t, "for_lbreak.d", 
        "void main() {          \n"
        "int i;                 \n"
        "loop1:                 \n"
        "for(; i != 10; ++i) {  \n"
        "   int j;              \n"
        "   assert(j == 0);     \n"
        "   for(; j != 5; ++j) {\n"
        "       if (j == 3) {   \n"
        "           break loop1;\n"
        "       }               \n"
        "   }                   \n"
        "}                      \n"
        "assert(i == 0);        \n"
        "}                      \n")
    }

    { CompilationTest& t = cTest("for w/ labeled continue", "for_lcont.exe");
      INPUT(t, "for_lcont.d", 
        "void main() {          \n"
        "int i;                 \n"
        "loop1:                 \n"
        "for(; i != 10; ++i) {  \n"
        "   int j;              \n"
        "   assert(j == 0);     \n"
        "   for(; j != 5; ++j) {\n"
        "       if (j == 3) {   \n"
        "           continue loop1;\n"
        "       }               \n"
        "   }                   \n"
        "   assert(j == 3);     \n"
        "}                      \n"
        "assert(i == 10);       \n"
        "}                      \n")
    }

    { CompilationTest& t = cTest("forever", "");
      INPUT(t, "forever.d",
        "int main() {           \n"
        "   for (; true; ){}    \n"
        "   return 0;           \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("while", "while.exe");
      INPUT(t, "while.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "while (i < 10)         \n"
        "{                      \n"
        "    ++i;               \n"
        "}                      \n"
        "assert(i == 10);       \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("while var", "whilev.exe");
      INPUT(t, "whilev.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "while (i)              \n"
        "{                      \n"
        "    --i;               \n"
        "}                      \n"
        "assert(i == 0);        \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("while, break", "whilebreak.exe");
      INPUT(t, "whilebreak.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "while (i < 10)         \n"
        "{                      \n"
        "    if(++i == 7)       \n"
        "       break;          \n"
        "}                      \n"
        "assert(i == 7);        \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("while, cont", "whilecont.exe");
      INPUT(t, "whilecont.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "while (i < 10)         \n"
        "{                      \n"
        "   if(++i == 7)        \n"
        "       continue;       \n"
        "   assert(i != 7);     \n"
        "}                      \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("do while, cont", "dowhilecont.exe");
      INPUT(t, "dowhilecont.d", 
        "int main() {           \n"
        "bool ok = false;       \n"
        "do                     \n"
        "{                      \n"
        "       ok = true;      \n"
        "       continue;       \n"
        "} while (false);       \n"
        "assert(ok);            \n"
        "return 0;              \n"
        "}"
      )
    }

    { CompilationTest& t = cTest("if, else", "ifelse.exe");
      INPUT(t, "ifelse.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "bool ok = false;       \n"
        "if (i)                 \n"
        "{                      \n"
        "    ok = true;         \n"
        "}                      \n"
        "else if (i < 6)        \n"
        "   ok = false;         \n"
        "assert(ok);            \n"
        "return 0;              \n"
        "}"
      )
    }
  
    { CompilationTest& t = cTest("if not, else", "ifnotelse.exe");
      INPUT(t, "ifnotelse.d", 
        "int main() {           \n"
        "int i = 5;             \n"
        "bool ok = false;       \n"
        "if (!i)                \n"
        "{                      \n"
        "    ok = false;        \n"
        "}                      \n"
        "else if (i < 6)        \n"
        "   ok = true;          \n"
        "assert(ok);            \n"
        "return 0;              \n"
        "}"
      )
    }
    // call external function with var args
    { CompilationTest& t = cTest("vargs_0", "vargs_0.exe");
      INPUT(t, "vargs_0.d",
          "import System;           \n"
          "void main() {            \n"
          "  Console.WriteLine(\"test {0} {1} {2}\".sys, 1, \"two\".sys, 3.0); \n"
          "}                        \n"
      )
    }

#if _WIN32
    // call external function with var args
    { CompilationTest& t = cTest("vargs_1", "vargs_1.exe");
      INPUT(t, "vargs_1.d",
          "import System;           \n"
          "class Test {             \n"
          "  override string toString() {\n"
          "     return \"cool!\";   \n"
          "  }                      \n"
          "}                        \n"
          "void main() {            \n"
          "  Object obj = new Test; \n"
          "  Console.WriteLine(\"test {0} {1} {2}\".sys, 1, \"two\".sys, obj.toString().sys); \n"
          "}                        \n"
      )
    }

    // call function that returns slice
    { CompilationTest& t = cTest("slice_ret", "slice_ret.exe");
      INPUT(t, "slice_ret.d",
          "import System;\n"
          "class Test {\n"
          " override string toString() { return \"Test\"; } \n"
          "}\n"
          "void main() {\n"
          " Test t = new Test;\n"
          " Console.WriteLine(t.toString().sys);\n"
          "}\n")
    }
#endif
    // access non-static fields
    { CompilationTest& t = cTest("fields", "fields.exe");
       INPUT(t, "fields.d",
            "import System;                                             \n"
            "class TestOne {                                            \n"
            "   int idata;                                              \n"
            "   double ddata;                                           \n"
            "   void print() {                                          \n"
            "       Console.WriteLine(\"{0}: {1}, {2}\".sys, this, idata, ddata);\n"
            "    }                                                      \n"
            "}                                                          \n"
            "class TestTwo {                                            \n"
            "    int data;                                              \n"
            "    this() {                                               \n"
            "        data = 42;                                         \n"
            "    }                                                      \n"
            "    void print() {                                         \n"
            "        Console.WriteLine(\"{0}: {1}\".sys, this, data);  \n"
            "    }                                                      \n"
            "}                                                          \n"
            "int main() {                                               \n"
            "   TestOne t1 = new TestOne;                               \n"
            "   TestTwo t2 = new TestTwo();                             \n"
            "   t1.print();                                             \n"
            "   t2.print();                                             \n"
            "   t1.idata = cast(int)(++t1.ddata) + t2.data;             \n"
            "   t1.print();                                             \n"
            "   assert (--t1.idata == 42);                              \n"
            "   return 0;                                               \n"
            "}\n"
        )
    }
    { CompilationTest& t = cTest("nested fields", "fnested.exe");
       INPUT(t, "fnested.d",
            "import System;\n"
            "\n"
            "class Child \n"
            "{\n"
            "	int data;\n"
            "	this()\n"
            "	{\n"
            "		data = 42;\n"
            "	}\n"
            "}\n"
            "\n"
            "class Parent \n"
            "{\n"
            "	Child child;\n"
            "	int data;\n"
            "	this()\n"
            "	{\n"
            "		child = new Child;\n"
            "	}\n"
            "}\n"
            "\n"
            "void main()\n"
            "{\n"
            "	Parent p = new Parent;\n"
            "	Child  c = new Child;\n"
            "	assert(p.child.data == 42);\n"
            "	System.Console.WriteLine(p.child.data);	\n"
            "	p.data = 13;\n"
            "	System.Console.WriteLine(p.data);	\n"
            "	p.data = ++p.data;\n"
            "	assert(p.data == 14);\n"
            "	System.Console.WriteLine(p.data);	\n"
            "	p.child.data = p.data;\n"
            "	++p.child.data;\n"
            "	assert(p.child.data == 15);\n"
            "	System.Console.WriteLine(p.child.data);	\n"
            "	p.child.data = ++c.data;\n"
            "	assert(p.child.data == 43);\n"
            "	System.Console.WriteLine(p.child.data);	\n"
            "	p.child.data = p.child.data++;\n"
            "	assert(p.child.data == 43);\n"
            "	System.Console.WriteLine(p.child.data);	\n"
            "   p.child.data = 1; \n"
            "   c.data = 2; \n"
            "   p.child.data += c.data; \n"
            "   assert(c.data == 2);    \n"
            "   assert(p.child.data == 3);\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("static var", "staticv.exe");
      INPUT(t, "staticv.d",
          "int i = 123;\n"
          "int main() {\n"
          "  return i - 123; \n"
          "}\n")
    }
    
    { CompilationTest& t = cTest("static field", "staticf.exe");
      INPUT(t, "staticf.d",
          "class Test {\n"
          "     static int i = 1;\n"
          "     this() { ++i; }\n"
          "}\n"
          "void main() {\n"
          "  Object o1 = new Test; \n"
          "  Object o2 = new Test; \n"
          "  assert(Test.i == 3);  \n"
          "}\n")
    }

    { CompilationTest& t = cTest("static ctor", "staticc.exe");
      INPUT(t, "staticc.d",
          "class Test {\n"
          "     static this () { ++i; }\n"
          "     static int i = 1;\n"
          "     this() { ++i; }\n"
          "}\n"
          "void main() {\n"
          "  Object o1 = new Test; \n"
          "  Object o2 = new Test; \n"
          "  assert(Test.i == 4);  \n"
          "}\n")
    }

    { CompilationTest& t = cTest("Exception Handling 1", "eh_1.exe");
      INPUT(t, "eh_1.d",
          "void main() {\n"
          " int i = 1;  \n"
          " try {       \n"
          "     i = 2;  \n"
          "     throw new Exception(\"bang\");\n"
          " }\n"
          " catch (Exception) \n"
          " {\n"
          "     i = 4;   \n"
          " }\n"
          " assert(i == 4);\n"
          "}\n")
    }

    { CompilationTest& t = cTest("Exception Handling 2", "eh_2.exe");
      INPUT(t, "eh_2.d",
          "int main() { \n"
          " int i = 1;  \n"
          " try {       \n"
          "     i = 2;  \n"
          "     throw new Exception(\"bang\");\n"
          " }\n"
          " catch (Exception e) \n"
          " {\n"
          "     i = 4;   \n"
          " }\n"
          " finally { i = 5; }\n"
          " assert(i == 5);\n"
          " return 0;}\n")
    }

    { CompilationTest& t = cTest("Exception Handling 3", "eh_3.exe");
      INPUT(t, "eh_3.d",
            "void f(int i)\n"
            "{\n"
            "    if (i == 0)\n"
            "        throw new Exception(\"boom\");\n"
            "}\n"
            "void main()\n"
            "{\n"
            "    int k;\n"
            "    try\n"
            "    {\n"
            "        f( k );\n"
            "        assert(false);\n"
            "    }\n"
            "    catch (Exception)\n"
            "    {\n"
            "    }\n"
            "    finally\n"
            "    {\n"
            "        k = 42;\n"
            "    }\n"
            "    assert(k == 42);\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("Exception Handling 4", "eh_4.exe");
      INPUT(t, "eh_4.d",
            "void main()\n"
            "{\n"
            "    int k;\n"
            "    try\n"
            "    {\n"
            "        k = 42;\n"
            "    }\n"
            "    finally\n"
            "    {\n"
            "    }\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("Exception Handling 5", "eh_5.exe");
      INPUT(t, "eh_5.d",
            "int main()\n"
            "{\n"
            "   int k;\n"
            "   try\n"
            "   {\n"
            "       k = 42;\n"
            "   }\n"
            "   catch (Exception)\n"
            "   {\n"
            "   }\n"
            "   assert(true);\n"
            "   return 0;\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("Exception Handling 6", "eh_6.exe");
      INPUT(t, "eh_6.d",
            "int main() {\n"
            "   try {\n"
            "       return 0;\n"
            "   }\n"
            "   catch (Exception) {\n"
            "   }\n"
            "   return 1;\n"
            "}\n"
        )
    }

    //test that the break / continue instructions do not jump
    //out of the try/catch blocks -- ILASM will catch the error
    //and cause the test to fail

    { CompilationTest& t = cTest("Exception Handling 7", "eh_7.exe");
      INPUT(t, "eh_7.d",
            "int main() {\n"
            " for (int i; i != 10; ++i) {\n"
            "   try {\n"
            "       if (i == 5) break;\n"
            "   }\n"
            "   catch (Exception) {\n"
            "   }\n"
            " }\n"
            " return 0;\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("Exception Handling 8", "eh_8.exe");
      INPUT(t, "eh_8.d",
            "int main() {\n"
            " for (int i; i != 10; ++i) {\n"
            "   try {\n"
            "   }\n"
            "   catch (Exception) {\n"
            "       continue;\n"
            "   }\n"
            " }\n"
            " return 0;\n"
            "}\n"
        )
    }

    { CompilationTest& t = cTest("eh_9", "eh_9.exe");
      INPUT(t, "eh_9.d",
           "void fun(int i) {                   \n"
           "    bool ok = false;                \n"
           "    switch (i)                      \n"
           "    {                               \n"
           "    case 3:                         \n"
           "        try {                       \n"
           "             ok = true;             \n"
           "        } catch(Exception) {        \n"
           "             break;                 \n"
           "        }                           \n"
           "        break;                      \n"
           "    case 42:                        \n"
           "        goto case 3;                \n"
           "    default:                        \n"
           "    }                               \n"
           "    assert(ok, \"ok\");             \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(3); fun(42);                \n"
           "}\n")
    }

    { CompilationTest& t = cTest("Static Array 1", "sa_1.exe");
      INPUT(t, "sa_1.d",
            "import System;\n"
            "int a[5] = [ 10, 11, 12 ];\n"
            "void main() {\n"
            "   assert(a[0] == 10);\n"
            "   assert(a[1] == 11);\n"
            "   assert(a[2] == 12);\n"
            "   assert(a[3] == 0); \n"
            "   assert(a[4] == 0); \n"
            "}\n")
    }

    //same as above, only this time the array is local to main
    { CompilationTest& t = cTest("Static Array 2", "sa_2.exe");
      INPUT(t, "sa_2.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5] = [ 10, 11, 12 ];\n"
            "   assert(a[0] == 10);\n"
            "   assert(a[1] == 11);\n"
            "   assert(a[2] == 12);\n"
            "   assert(a[3] == 0); \n"
            "   assert(a[4] == 0); \n"
            "}\n")
    }

    { CompilationTest& t = cTest("Static Array 3", "sa_3.exe");
      INPUT(t, "sa_3.d",
            "int a[5];\n"
            "import System;\n"
            "void main() {\n"
            "   a = [ 10, 11, 12 ];\n"
            "   assert(a[0] == 10);\n"
            "   assert(a[1] == 11);\n"
            "   assert(a[2] == 12);\n"
            "   assert(a[3] == 0); \n"
            "   assert(a[4] == 0); \n"
            "}\n")
    }

    { CompilationTest& t = cTest("Static Array of Struct", "sas.exe");
      INPUT(t, "sas.d",
            "struct X { int i; }\n"
            "X a[2];\n"
            "void main() {\n"
            "   assert(a[0].i == 0);\n"
            "   assert(a[1].i == 0);\n"
            "   a[0].i = 1;\n"
            "   a[1].i = 1;\n"
            "   foreach(s; a) {\n"
            "       assert(s.i == 1);\n"
            "   }\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Member Array of Struct", "mas.exe");
      INPUT(t, "mas.d",       
        "struct X { int i; }\n"
        "struct Y { X a[2]; }\n"
        "void main() {\n"
        "   Y y;\n"
        "   assert(y.a[0].i == 0);\n"
        "   assert(y.a[1].i == 0);\n"
        "}\n")
    }
 
    { CompilationTest& t = cTest("Array 1", "a_1.exe");
      INPUT(t, "a_1.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5];\n"
            "   a = [ 10, 11, 12 ];\n"
            "   assert(a[0] == 10);\n"
            "   assert(a[1] == 11);\n"
            "   assert(a[2] == 12);\n"
            "   assert(a[3] == 0); \n"
            "   assert(a[4] == 0); \n"
            "}\n")
    }

    { CompilationTest& t = cTest("Array 2", "a_2.exe");
      INPUT(t, "a_2.d",
            "import System;\n"
            "void main() {\n"
            "   int a[3];\n"
            "   a = 42;\n"
            "   assert(a[0] == 42);\n"
            "   assert(a[1] == 42);\n"
            "   assert(a[2] == 42);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Array 3", "a_3.exe");
      INPUT(t, "a_3.d",
            "import System;\n"
            "void main() {\n"
            "   int a[3] = 42;\n"
            "   assert(a[0] == 42);\n"
            "   assert(a[1] == 42);\n"
            "   assert(a[2] == 42);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Array 4", "a_4.exe");
      INPUT(t, "a_4.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5];\n"
            "   a[1..4] = 42; \n"
            "   assert(a[0] == 0);\n"
            "   assert(a[1] == 42);\n"
            "   assert(a[2] == 42);\n"
            "   assert(a[3] == 42);\n"
            "   assert(a[4] == 0);\n"
            "}\n")
    }

    // set array via slice and literal
    { CompilationTest& t = cTest("Array 5", "a_5.exe");
      INPUT(t, "a_5.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5];\n"
            "   a[2..4] = [ 11, 12, 13, 14, 15, 16, 17, 18 ]; \n"
            "   assert(a[0] == 0);\n"
            "   assert(a[1] == 0);\n"
            "   assert(a[2] == 11);\n"
            "   assert(a[3] == 12);\n"
            "   assert(a[4] == 0);\n"
            "}\n")
    }

    // set array via slice and literal
    { CompilationTest& t = cTest("Array 6", "a_6.exe");
      INPUT(t, "a_6.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5];\n"
            "   a[2..10] = [ 11, 12, 13, 14, 15, 16, 17, 18 ]; \n"
            "   assert(a[0] == 0);\n"
            "   assert(a[1] == 0);\n"
            "   assert(a[2] == 11);\n"
            "   assert(a[3] == 12);\n"
            "   assert(a[4] == 13);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Array 7", "a_7.exe");
      INPUT(t, "a_7.d",
            "import System;\n"
            "void main() {\n"
            "   int a[5];\n"
            "   a[2] = 1; \n"
            "   a[3] = 2; \n"
            "   assert(a[0] == 0);\n"
            "   assert(a[1] == 0);\n"
            "   assert(a[2] == 1);\n"
            "   assert(a[3] == 2);\n"
            "   assert(a[4] == 0);\n"
            "}\n")
    }

    //dynamic array initialization
    { CompilationTest& t = cTest("Array 8", "a_8.exe");
      INPUT(t, "a_8.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   assert(a[2] == 3); \n"
            "}\n")
    }
    { CompilationTest& t = cTest("Array 9", "a_9.exe");
      INPUT(t, "a_9.d",
            "void main() {\n"
            "   int a[];\n"
            "   a = [1, 2, 3];\n"
            "   assert(a[2] == 3); \n"
            "}\n")
    }

    //test array concatenation
    { CompilationTest& t = cTest("Array 10", "a_10.exe");
      INPUT(t, "a_10.d",
            "void main() {\n"
            "   int a[] = [1, 2, 4];\n"
            "   int b[];            \n"
            "   b = [3, 5];         \n"
            "   b ~= a;             \n"
            "   assert(b.length==5);\n"
            "}\n")
    }
    //test array concatenation
    { CompilationTest& t = cTest("Array 11", "a_11.exe");
      INPUT(t, "a_11.d",
            "void main() {\n"
            "   int a[] = [1, 2, 4];\n"
            "   int b[];            \n"
            "   b = [3, 5];         \n"
            "   b = a ~ b;          \n"
            "   assert(b.length==5);\n"
            "   assert(b[0] == a[0]);\n"
            "}\n")
    }
    { CompilationTest& t = cTest("Array Index", "aindex.exe");
      INPUT(t, "aindex.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   int i;\n"
            "   a[i + 0] = a[1 + i] = a[2] = 4;\n"
            "   assert(a[0] == 4);\n"
            "   assert(a[1] == 4);\n"
            "   assert(a[2] == 4);\n"
            "}\n")
    }
    { CompilationTest& t = cTest("Assoc. Array Index", "aaindex.exe");
      INPUT(t, "aaindex.d",
            "void main() {\n"
            "   int a[string] = [\"one\" : 1, \"two\" : 2];\n"
            "   a[\"one\"] = a[\"two\"] = 3;\n"
            "   assert(a[\"one\"] == 3);\n"
            "   assert(a[\"two\"] == 3);\n"
            "}\n")
    }

    //test array concatenation
    { CompilationTest& t = cTest("Array Concatenation ", "ac_12.exe");
      INPUT(t, "ac_12.d",
            "void main() {\n"
            "   int a[] = [1, 2, 4];\n"
            "   a ~= 3;             \n"
            "   assert(a.length==4);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Array Concatenation ", "ac_13.exe");
      INPUT(t, "ac_13.d",
            "void main() {\n"
            "   int a[] = [1, 2, 4];\n"
            "   a ~= [3];\n"
            "   assert(a.sort.length==4);\n"
            "}\n")
    }
#if _WIN32
    //mono does not compile #define nor .typedef statements
    { CompilationTest& t = cTest("Slice Concatenation ", "ac_14.exe");
      INPUT(t, "ac_14.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   int s[] = a[0..$];\n"
            "   s ~= [4, 5];\n"
            "   assert(s.length == 5);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Slice Concatenation ", "ac_15.exe");
      INPUT(t, "ac_15.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   int s[] = a[0..$];\n"
            "   s ~= 4;\n"
            "   assert(s.length == 4);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Slice Concatenation ", "ac_16.exe");
      INPUT(t, "ac_16.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   int s[] = a[0..$];\n"
            "   a ~= s;\n"
            "   assert(a.length == 6);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Slice Concatenation ", "ac_17.exe");
      INPUT(t, "ac_17.d",
            "void main() {\n"
            "   int a[] = [1, 2, 3];\n"
            "   int s[] = a[0..$];\n"
            "   s ~= s;\n"
            "   assert(s.length == 6);\n"
            "}\n")
    }
    //test array sort & reverse
    { CompilationTest& t = cTest("Array 12", "a_12.exe");
      INPUT(t, "a_12.d",
            "void main() {\n"
            "   int a[] = [2, 1, 4];\n"
            "   a.sort;             \n"
            "   assert(a[0] == 1);  \n"
            "   assert(a[1] == 2);  \n"
            "   assert(a[2] == 4);  \n"
            "   a.reverse;          \n"
            "   assert(a[0] == 4);  \n"
            "   assert(a[1] == 2);  \n"
            "   assert(a[2] == 1);  \n"
            "}\n")
    }
    //test passing array slices to functions
    { CompilationTest& t = cTest("Array slice arguments", "a_13.exe");
      INPUT(t, "a_13.d",
            "void f(double a[]) {\n"
            "    assert(a.length == 3);\n"
            "}\n"
            "void main() {\n"
            "   double a[] = [0.0, 1.1, 2.2, 3.3];\n"
            "   double b[] = a[1..$];\n"
            "   f(a[1..$]);\n"
            "   f(b);\n"
            "}\n")
    }
    //test evaluation of slice length
    { CompilationTest& t = cTest("Array slice length", "a_14.exe");
      INPUT(t, "a_14.d",
            "void main() {\n"
            "   double a[] = [0.0, 1.1, 2.2, 3.3];\n"
            "   assert(a[1..$].length == 3);\n"
            "}\n")
    }
#endif
#if _WIN32
    { CompilationTest& t = cTest("Jagged Array 1", "ja_1.exe");
      INPUT(t, "ja_1.d",
          "void main() { \n"
          " int b[2][3][4][5];\n"
          " for (int i = 0; i != 2; ++i)\n"
          " {\n"
          "      for (int j = 0; j != 3; ++j)\n"
          "      {\n"
          "	    for (int k = 0; k != 4; ++k)\n"
          "	    {\n"
          "             for (int n = 0; n != 5; ++n)\n"
          "                 b[i][j][k][n] = i + j + k + n;\n"
          "         }\n"
          "     }\n"
          " }\n"
          " for (int i = 0; i != 2; ++i)\n"
          " {\n"
          "      for (int j = 0; j != 3; ++j)\n"
          "      {\n"
          "	    for (int k = 0; k != 4; ++k)\n"
          "	    {\n"
          "             for (int n = 0; n != 5; ++n)\n"
          "                 assert(b[i][j][k][n] == i + j + k + n);\n"
          "         }\n"
          "     }\n"
          " }\n"
          "}\n")
    }

    //same as above, only the array is static
    { CompilationTest& t = cTest("Jagged Array 2", "ja_2.exe");
      INPUT(t, "ja_2.d",
          "int b[2][3][4][5];\n"
          "void main() { \n"
          " for (int i = 0; i != 2; ++i)\n"
          " {\n"
          "      for (int j = 0; j != 3; ++j)\n"
          "      {\n"
          "	    for (int k = 0; k != 4; ++k)\n"
          "	    {\n"
          "             for (int n = 0; n != 5; ++n)\n"
          "                 b[i][j][k][n] = i + j + k + n;\n"
          "         }\n"
          "     }\n"
          " }\n"
          " for (int i = 0; i != 2; ++i)\n"
          " {\n"
          "      for (int j = 0; j != 3; ++j)\n"
          "      {\n"
          "	    for (int k = 0; k != 4; ++k)\n"
          "	    {\n"
          "             for (int n = 0; n != 5; ++n)\n"
          "                 assert(b[i][j][k][n] == i + j + k + n);\n"
          "         }\n"
          "     }\n"
          " }\n"
          "}\n")
    }
#endif //_WIN32
    { CompilationTest& t = cTest("Dynamic Array Length 1", "dal_1.exe");
      INPUT(t, "dal_1.d",
          "void main() {\n"
          " float f[]; \n"
          " assert(f.length == 0); \n"
          "}\n")
    }

    { CompilationTest& t = cTest("Dynamic Array Length 2", "dal_2.exe");
      INPUT(t, "dal_2.d",
          "import System;\n"
          "void main() {\n"
          "  float f[]; \n"
          "  f.length = 3;\n"
          "  assert(f.length == 3); \n"
          "  for (int i; i != 3; ++i) \n"
          "     System.Console.WriteLine(f[i]);\n"
          "}\n")
    }

    { CompilationTest& t = cTest("Dynamic Array Length 3", "dal_3.exe");
      INPUT(t, "dal_3.d",
          "import System;\n"
          "void main() {\n"
          " float f[]; \n"
          " Console.WriteLine(\"array size={0}\".sys, f.length = 2);\n"
          " assert(f.length == 2); \n"
          "}\n")
    }

    { CompilationTest& t = cTest("explicit delete", "dtor_1.exe");
      INPUT(t, "dtor_1.d",
            "class Base\n"
            "{\n"
            "   static int done = -1;\n"
            "   this() { done = 0; }\n"
            "   ~this(){ done = 1; }\n"
            "}\n"
            "class Derived : public Base\n"
            "{\n"
            "   ~this() { }\n"
            "}\n"
            "void main()\n"
            "{\n"
            "   Object obj = new Derived;\n"
            "   delete obj;\n"
            "   assert(Base.done == 1);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("explicit delete w/ scope", "dtor_2.exe");
      INPUT(t, "dtor_2.d",
            "class Base\n"
            "{\n"
            "   static int done = -1;\n"
            "   this() { done = 0; }\n"
            "   ~this(){ ++done; }\n"
            "   static ~this() { assert(Base.done == 1); }\n"
            "}\n"
            "class Derived : public Base\n"
            "{\n"
            "   ~this() { }\n"
            "}\n"
            "void main()\n"
            "{\n"
            "   scope Object obj = new Derived;\n"
            "}\n")
    }

    { CompilationTest& t = cTest("function scope static var", "fssv.exe");
      INPUT(t, "fssv.d",
            "int f()\n"
            "{\n"
            "    static int i = 42;\n"
            "    return ++i;\n"
            "}\n"
            "void main()\n"
            "{\n"
            "    f();\n"
            "    assert(f() == 44);\n"
            "}\n")
    }

    //test that duplicate names are avoided
    { CompilationTest& t = cTest("function scope static var 2", "fssv2.exe");
      INPUT(t, "fssv2.d",
            "int f()\n"
            "{\n"
            "    static int i = 42;\n"
            "    return ++i;\n"
            "}\n"
            "void main()\n"
            "{\n"
            "   static int i = 13;\n"
            "   f();\n"
            "   assert(f() == 44);\n"
            "}\n")
    }

#if _WIN32
    // none of this slice-related stuff works on MONO because their
    // implementation of ILASM does not support #define
    // (and I ain't changing MY code just because Miguel is a dummy)
    { CompilationTest& t = cTest("array slice 1", "aslice_1.exe");
      INPUT(t, "aslice_1.d",
          "void main() {\n"
          " int a[] = [1, 2, 3, 4, 5];  \n"
          " assert(a.length == 5);      \n"
          " int b[] = a[2..5];          \n"
          " assert(b[0] == 3);          \n"
          " int c[] = b;                \n"
          " assert(c[0] == 3);          \n"
          " c[0] = 0;                   \n"
          " assert(a[2] == 0);          \n"
          "}\n")
    }

    //test slice bounds checking
    { CompilationTest& t = cTest("array slice 2", "aslice_2.exe");
      t.setCompileFlags("--c "); // catch all exceptions at main()
      INPUT(t, "aslice_2.d",
          "int main() {\n"
          " int a[] = [1, 2, 3, 4, 5, 6];\n"
          " assert(a.length == 6);      \n"
          " int b[] = a[2..5];          \n"
          " assert(b[0] == 3);          \n"
          " int c[] = b;                \n"
          " c[3] = 0;                   \n"
          " return -1;                  \n"
          "}\n")
    }

    { CompilationTest& t = cTest("array slice 3", "aslice_3.exe");
      INPUT(t, "aslice_3.d",
          "void main() {\n"
          " int a[]=[1, 2, 3, 4, 5, 6]; \n"
          " int b[6];                   \n"
          " int c[];                    \n"
          " b[] = a;                    \n"
          " assert(b[2] == 3);          \n"
          " c = a;                      \n"
          " assert(c[1] == 2);          \n"
          "}\n")
    }

    //test array slice length
    { CompilationTest& t = cTest("array slice length", "aslice_4.exe");
      INPUT(t, "aslice_4.d",
            "void main() {                          \n"
            "   int a[] = [1, 2, 3, 4, 5, 6];       \n"
            "   int s[] = a[2..5];                  \n"
            "   assert(s.length == 3);              \n"
            "   int z[] = a[2..10];                 \n"
            "   assert(z.length == 4);              \n"
            "}\n")
    }

    //test array slice resizing
    { CompilationTest& t = cTest("array slice resize", "aslice_5.exe");
      INPUT(t, "aslice_5.d",
            "void main() {                          \n"
            "   int a[] = [1, 2, 3, 4, 5, 6, 7];    \n"
            "   a.length = 8;                       \n"
            "   int s[] = a[2..5];                  \n"
            "   assert(s.length == 3);              \n"
            "   s.length = 5;                       \n"
            "   assert(s[0] == 3);                  \n"
            "   s[0] = 2;                           \n"
            "   assert(a[2] == 2);                  \n"
            "   s.length = 10;                      \n"
            "   s[0] = 4;                           \n"
            "   assert(a[2] == 2);                  \n"
            "}\n")
    }
    { CompilationTest& t = cTest("array slice sort", "aslice_6.exe");
      INPUT(t, "aslice_6.d",
            "void main() {                          \n"
            "   int a[] = [1, 2, 6, 5, 4, 3, 7];    \n"
            "   int s[] = a[2..6];                  \n"
            "   s.sort;                             \n"
            "   assert(a[2] == 3);                  \n"
            "   assert(a[3] == 4);                  \n"
            "   assert(a[4] == 5);                  \n"
            "}\n")
    }

    //construct a slice from another slice
    { CompilationTest& t = cTest("slice from slice", "slice_from_slice.exe");
      INPUT(t, "slice_from_slice.d",
            "void main() {                          \n"
            "   int a[] = [1, 2, 6, 5, 4, 3, 7];    \n"
            "   int s[] = a[2..6];                  \n"
            "   int z[] = s[0..4];                  \n"
            "   z = s[0..3];                        \n"
            "   assert(z.length == 3);              \n"
            "}\n")
    }

    //construct a slice from a string
    { CompilationTest& t = cTest("slice from string", "slice_from_string.exe");
      INPUT(t, "slice_from_string.d",
           "import System;                          \n"
            "void main() {                          \n"
            "   string greet = \"I say Hello\";     \n"
            "   invariant char msg[] = greet[6..$]; \n"
            "   assert(msg.length == 5);            \n"
            "   string foo = greet[6..11];          \n"
            "   assert(foo.length == 5);            \n"
            "}\n")
    }

    //slice from string slice
    { CompilationTest& t = cTest("slice string", "slice_string.exe");
      INPUT(t, "slice_string.d",
            "import System;\n"
            "void main()\n"
            "{\n"
            "    string greet = \"Hello World\";\n"
            "    string msg = greet[5..$];\n"
            "    assert(String.Compare(msg[1..$].sys, \"World\".sys) == 0);\n"
            "    msg = greet[5..11];\n"
            "    assert(String.Compare(msg[1..$].sys, \"World\".sys) == 0);\n"
            "}\n")
    }

    //Test passing arrays between functions
    { CompilationTest& t = cTest("array funcs", "afuncs.exe");
      INPUT(t, "afuncs.d",
          "int[] foo() {                            \n"
          "     int a[5] = [5, 4, 3, 2, 1];         \n"
          "     return a[1..5];                     \n"
          "}                                        \n"
          "void bar(int[] b) {                      \n"
          "     assert(b.length == 4);              \n"
          "}                                        \n"
          "void main() {                            \n"
          "     int c[] = foo();                    \n"
          "     bar(c);                             \n"
          "     int d[];                            \n"
          "     d = foo().sort;                     \n"
          "     assert(d[0] == 1);                  \n"
          "     assert(d[3] == 4);                  \n"
          "}                                        \n")
    }
    //Test array ptr
    { CompilationTest& t = cTest("array ptr", "aptr.exe");
      INPUT(t, "aptr.d",
          "void main() {                            \n"
          "     int a[ ] = [5, 4, 3, 2, 1];         \n"
          "     int* p = a.ptr;                     \n"
          "     ++p;                                \n"
          "     assert(*p == 4);                    \n"
          "}                                        \n")
    }
    //Test array ptr store 
    { CompilationTest& t = cTest("array ptr store", "aptr1.exe");
      INPUT(t, "aptr1.d",
          "void main() {                            \n"
          "     int a[ ] = [5, 4, 3, 2, 1];         \n"
          "     int* p = a.ptr;                     \n"
          "     *p = 4;                             \n"
          "     assert(*p == 4);                    \n"
          "}                                        \n")
    }
    //Test static array ptr
    { CompilationTest& t = cTest("static array ptr", "aptr2.exe");
      INPUT(t, "aptr2.d",
          "void main() {                            \n"
          "     int a[5] = [5, 4, 3, 2, 1];         \n"
          "     int* p = a.ptr;                     \n"
          "     ++p;                                \n"
          "     assert(*p == 4);                    \n"
          "}                                        \n")
    }
    //Test static array ptr w/ offset
    { CompilationTest& t = cTest("static array ptr", "aptr3.exe");
      INPUT(t, "aptr3.d",
          "void main() {                            \n"
          "     int a[5] = [5, 4, 3, 2, 1];         \n"
          "     int* p = &a[2];                     \n"
          "     ++p;                                \n"
          "     assert(*p == 2);                    \n"
          "}                                        \n")
    }
    //Test array ptr from addr
    { CompilationTest& t = cTest("array ptr from addr", "aptr4.exe");
      INPUT(t, "aptr4.d",
          "void main() {                            \n"
          "     int a[ ] = [5, 4, 3, 2, 1];         \n"
          "     int* p = &a[3];                     \n"
          "     ++p;                                \n"
          "     assert(*p == 1);                    \n"
          "}                                        \n")
    }

    //Test array ptr slice
    { CompilationTest& t = cTest("array ptr slice", "aptr5.exe");
      INPUT(t, "aptr5.d",
          "void main() {                            \n"
          "     int a[] = [5, 4, 3, 2, 1];          \n"
          "     int b[] = a[1..5];                  \n"
          "     int* p = b.ptr;                     \n"
          "     ++p;                                \n"
          "     assert(*p == 3);                    \n"
          "}                                        \n")
    }
    //Test array ptr slice w/ offset
    { CompilationTest& t = cTest("array ptr slice /w offset", "aptr6.exe");
      INPUT(t, "aptr6.d",
          "void main() {                            \n"
          "     int a[] = [5, 4, 3, 2, 1];          \n"
          "     int b[] = a[1..5];                  \n"
          "     int* p = &b[1];                     \n"
          "     ++p;                                \n"
          "     assert(*p == 2);                    \n"
          "     int i = 42;                         \n"
          "     p = &i;                             \n"
          "     assert(*p == 42);                   \n"
          "}                                        \n")
    }
#endif  // _WIN32

    //Test assignment to struct fields
    { CompilationTest& t = cTest("assign struct fields", "struct_1.exe");
      INPUT(t, "struct_1.d",
          "struct Foobah {                          \n"
          "     int x;                              \n"
          "     double y;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     Foobah foo;                         \n"
          "     foo.y = 1.2345;                     \n"
          "     assert(foo.y == 1.2345);            \n"
          "}\n")
    }

    //Test struct literal assignment
    { CompilationTest& t = cTest("struct literals", "struct_2.exe");
      INPUT(t, "struct_2.d",
          "struct Foobah {                          \n"
          "     int x;                              \n"
          "     double y;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     int i = 2;                          \n"
          "     Foobah foo = { i, 3.0 };            \n"
          "     assert(foo.x == 2);                 \n"
          "     assert(foo.y == 3.0);               \n"
          "}\n")
    }

    //Test static fields in structs
    { CompilationTest& t = cTest("struct static fields", "struct_3.exe");
      INPUT(t, "struct_3.d",
          "struct Foobah {                          \n"
          "     static int x = 42;                  \n"
          "     static int y = 13;                  \n"
          "     double z;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     Foobah foo = { 3.14 };              \n"
          "     assert(foo.x == 42);                \n"
          "     assert(foo.y == 13);                \n"
          "}\n")
    }
    
    //Test template struct literal assignment
    { CompilationTest& t = cTest("template struct / literals", "struct_4.exe");
      INPUT(t, "struct_4.d",
          "struct Foobah(T) {                       \n"
          "     T x;                                \n"
          "     double y;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     int i = 2;                          \n"
          "     Foobah!(int) foo = { i, 3.0 };      \n"
          "     assert(foo.x == 2);                 \n"
          "     assert(foo.y == 3.0);               \n"
          "}\n")
    }
    { CompilationTest& t = cTest("array struct members", "struct_5.exe");
      INPUT(t, "struct_5.d",
          "struct Foo {                             \n"
          "     int a[3];                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     Foo foo;                            \n"
          "     assert(foo.a[2] == 0);              \n"
          "}\n")
    }

    //Test struct default init
    { CompilationTest& t = cTest("struct init", "struct_init.exe");
      INPUT(t, "struct_init.d",
          "struct Foobah {                          \n"
          "     int x;                              \n"
          "     double y;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     Foobah foo;                         \n"
          "     assert(foo.x == 0);                 \n"
          "     assert(foo.y == 0.0);               \n"
          "}\n")
    }

    //Test struct initializers
    { CompilationTest& t = cTest("struct initializers", "struct_init2.exe");
      INPUT(t, "struct_init2.d",
          "struct Foobah {                          \n"
          "     int x = 123;                        \n"
          "     double y;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     Foobah foo;                         \n"
          "     assert(foo.x == 123);               \n"
          "     assert(foo.y == 0.0);               \n"
          "}\n")
    }

    //call function with default params
    { CompilationTest& t = cTest("default param", "default_param.exe");
      INPUT(t, "default_param.d",
          "void foo(int i = 42) {                   \n"
          "     assert(i == 42);                    \n"
          "}                                        \n"
          "void bar(int i = 42) {                   \n"
          "     assert(i == 13);                    \n"
          "}                                        \n"
          "void main() {                            \n"
          "     foo();                              \n"
          "     bar(13);                            \n"
          "}\n")
    }

    //test switch statement
    { CompilationTest& t = cTest("switch case", "switch_case.exe");
      INPUT(t, "switch_case.d",
           "void fun(int i) {                   \n"
           "     bool ok = false;               \n"
           "     switch (i)                     \n"
           "     {                              \n"
           "         case 3:                    \n"
           "             ok = true;             \n"
           "             break;                 \n"
           "         case 42:                   \n"
           "             ok = true;             \n"
           "             break;                 \n"
           "         default:                   \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(3); fun(42);                \n"
           "}\n")
    }

    //test switch statement w/ fallthru
    { CompilationTest& t = cTest("switch fallthru", "switch_fall.exe");
      INPUT(t, "switch_fall.d",
           "void fun(int i) {                   \n"
           "     bool ok = false;               \n"
           "     switch (i)                     \n"
           "     {                              \n"
           "        case 2:                     \n"
           "        case 3:                     \n"
           "             ok = true;             \n"
           "             break;                 \n"
           "        case 42:                    \n"
           "             ok = true;             \n"
           "             break;                 \n"
           "        default:                    \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(2); fun(42);                \n"
           "}\n")
    }
    //test switch statement w/ default fallthru
    { CompilationTest& t = cTest("default fallthru", "default_fall.exe");
      INPUT(t, "default_fall.d",
           "void fun(int i) {                   \n"
           "     bool ok = false;               \n"
           "     switch (i)                     \n"
           "     {                              \n"
           "        case 2:                     \n"
           "        case 3:                     \n"
           "             break;                 \n"
           "        case 42:                    \n"
           "             break;                 \n"
           "        default:                    \n"
           "        case 15:                    \n"
           "            ok = true;              \n"
           "            break;                  \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(13);                        \n"
           "}\n")
    }

    //test switch statement w/ no default
    { CompilationTest& t = cTest("no default", "no_default.exe");
      INPUT(t, "no_default.d",
           "void fun(int i) {                   \n"   
           "     switch (i) {                   \n"
           "        case 2:                     \n"           
           "            break;                  \n"
           "     }                              \n"
           "}                                   \n"
           "void main() {                       \n"
           "    bool ok = false;                \n"
           "    try {                           \n"
           "        fun(13);                    \n"
           "    }                               \n"
           "    catch (Exception e) {           \n"
           "        ok = true;                  \n"
           "    }                               \n"
           "    assert(ok);                     \n"
           "}\n")
    }

    //test switch statement w/ variable labels
    { CompilationTest& t = cTest("switch with var labels", "switch_var.exe");
      INPUT(t, "switch_var.d",
           "bool ok = false;                    \n"
           "void fun(int i) {                   \n"
           "     int j = 15;                    \n"
           "     switch (i)                     \n"
           "     {                              \n"
           "        case 2:                     \n"
           "        case 3:                     \n"
           "             break;                 \n"
           "        case 42:                    \n"
           "             break;                 \n"
           "        default:                    \n"
           "        case j:                     \n"
           "            ok = true;              \n"
           "            break;                  \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(13);                        \n"
           "    ok = false;                     \n"
           "    fun(15);                        \n"
           "}\n")
    }

    //test switch statement with no cases
    { CompilationTest& t = cTest("switch no case", "switch_no_case.exe");
      INPUT(t, "switch_no_case.d",
           "void fun(int i) {                   \n"
           "     bool ok = false;               \n"
           "     switch (i)                     \n"
           "     {                              \n"
           "     default:                       \n"
           "        ok = true; break;           \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(3); fun(42);                \n"
           "}\n")
    }

#if _WIN32
    //test switch statement with string labels
    { CompilationTest& t = cTest("switch w/ strings", "switch_str.exe");
      INPUT(t, "switch_str.d",
           "void fun(string s) {                \n"
           "     bool ok = false;               \n"
           "     switch (s) {                   \n"
           "     case \"hello\":                \n"
           "        ok = true;                  \n"
           "        break;                      \n"
           "     default:                       \n"
           "        assert(false);              \n"
           "        break;                      \n"
           "     }                              \n"
           "     assert(ok, \"ok\");            \n"
           "}                                   \n"
           "void main() {                       \n"
           "    fun(\"hello\");                 \n"
           "}\n")
    }
#endif

    { CompilationTest& t = cTest("conditional exp", "cond_exp.exe");
      INPUT(t, "cond_exp.d",
          "int foo(int i = 42) {                    \n"
          "     return i;                           \n"
          "}                                        \n"
          "void main() {                            \n"
          "     int x = foo() > 0 ? 1 : -1;         \n"
          "     assert(x == 1);                     \n"
          "     x = foo(-1) > 0 ? 1 : -1;           \n"
          "     assert(x == -1);                    \n"
          "}\n")
    }

    { CompilationTest& t = cTest("shift left", "shift_left.exe");
      INPUT(t, "shift_left.d",
          "void main() {                            \n"
          "     int i = 1;                          \n"
          "     i <<= 3;                            \n"
          "     assert(i == 8);                     \n"
          "     assert((i << 2) == 32);             \n"
          "}\n")
    }

    { CompilationTest& t = cTest("shift right", "shift_right.exe");
      INPUT(t, "shift_right.d",
          "void main() {                            \n"
          "     int i = 2;                          \n"
          "     i >>= 1;                            \n"
          "     assert(i == 1);                     \n"
          "     assert((i << 2) == 4);              \n"
          "}\n")
    }
    
    { CompilationTest& t = cTest("shift right unsigned ", "shift_uright.exe");
      INPUT(t, "shift_uright.d",
          "void main() {                            \n"
          "     int i = 2;                          \n"
          "     i >>>= 1;                           \n"
          "     assert(i == 1);                     \n"
          "     i = 1024;                           \n"
          "     assert((i >>> 10) == 1);            \n"
          "}                                        \n")
    }

    { CompilationTest& t = cTest("Array Literal Foreach", "alit_foreach.exe");
      INPUT(t, "alit_foreach.d",
            "int b[3];                  \n"
            "void main() {              \n"
            "   foreach(int i, int v; [ 10, 11, 12 ]) {\n"
            "       b[i] = v;           \n"
            "   }                       \n"
            "   assert(b[0] == 10);     \n"
            "   assert(b[1] == 11);     \n"
            "   assert(b[2] == 12);     \n"
            "}                          \n")
    }

    { CompilationTest& t = cTest("Array Foreach", "a_foreach.exe");
      INPUT(t, "a_foreach.d",
            "int a[] = [ 10, 11, 12 ];  \n"
            "int b[3];                  \n"
            "void main() {              \n"
            "   foreach(int i, int v; a){\n"
            "       b[i] = v;           \n"
            "   }                       \n"
            "   assert(b[0] == 10);     \n"
            "   assert(b[1] == 11);     \n"
            "   assert(b[2] == 12);     \n"
            "}                          \n")
    }

    { CompilationTest& t = cTest("Array Foreach Reverse", "a_foreach_reverse.exe");
      INPUT(t, "a_foreach_reverse.d",
            "int a[] = [ 10, 11, 12 ];  \n"
            "int b[3];                  \n"
            "void main() {              \n"
            "   int i = 0;              \n"
            "   foreach_reverse(int v; a){\n"
            "       b[i++] = v;         \n"
            "   }                       \n"
            "   assert(b[0] == 12);     \n"
            "   assert(b[1] == 11);     \n"
            "   assert(b[2] == 10);     \n"
            "}                          \n")
    }

    { CompilationTest& t = cTest("Foreach Range", "foreach_range.exe");
      INPUT(t, "foreach_range.d",
            "int a[] = [ 10, 11, 12 ];  \n"
            "int b[3];                  \n"
            "void main() {              \n"
            "   int begin;              \n"
            "   int end;                \n"
            "   end = 3;                \n"
            "   foreach(i; begin..end){ \n"
            "       b[i] = a[i];        \n"
            "   }                       \n"
            "   assert(b[0] == 10);     \n"
            "   assert(b[1] == 11);     \n"
            "   assert(b[2] == 12);     \n"
            "}                          \n")
    }

#if _WIN32
    //executable crashes on MONO
    { CompilationTest& t = cTest("Array Foreach Ref", "a_foreach_ref.exe");
      INPUT(t, "a_foreach_ref.d",
            "void main() {              \n"
            "   int a[] = new int[3];   \n"
            "   foreach_reverse(i, ref v; a){\n"
            "       v = i;              \n"
            "   }                       \n"
            "   foreach(i, v; a){       \n"
            "       assert(v == i);     \n"
            "   }                       \n"
            "}                          \n")
    }

#endif
    { CompilationTest& t = cTest("Array Concat Foreach", "ac_foreach.exe");
      INPUT(t, "ac_foreach.d",
            "import System;             \n"
            "void main() {              \n"
            "   int a[] = [1, 2, 3];    \n"
            "   foreach(i; a ~ a){      \n"
            "       Console.WriteLine(i);\n"
            "   }                       \n"
            "}                          \n")
    }
#if _WIN32
    { CompilationTest& t = cTest("Slice Concat Foreach", "sc_foreach.exe");
      INPUT(t, "sc_foreach.d",
            "import System;             \n"
            "void main() {              \n"
            "   int a[] = [1, 2, 3];    \n"
            "   int s[] = a[0..$];      \n"
            "   foreach(i; s ~ a){      \n"
            "       Console.WriteLine(i);\n"
            "   }                       \n"
            "}                          \n")
    }
    { CompilationTest& t = cTest("Slice Concat Foreach Rev", "sc_foreach_rev.exe");
      INPUT(t, "sc_foreach_rev.d",
            "import System;             \n"
            "void main() {              \n"
            "   int a[] = [1, 2, 3];    \n"
            "   int s[] = a[0..$];      \n"
            "   foreach_reverse(i; s ~ s){\n"
            "       Console.WriteLine(i);\n"
            "   }                       \n"
            "}                          \n")
    }
#endif // _WIN32
    { CompilationTest& t = cTest("foreach opApply", "foreach_opApply.exe");
      INPUT(t, "foreach_opApply.d",
        "struct S {\n"
        "   int i;\n"
        "   double d;\n"
        "   int j;\n"
        "   int opApply(int delegate(ref int) dg) { \n"
        "       return dg(j);\n"
        "   }\n"
        "}\n"
        
        "void main() {\n"
        "   S s = { 13, 3.14159, 2 };\n"
        "   foreach(ref x; s) {\n"
        "       x = 4;  \n"
        "   }\n"
        "   assert(s.j == 4);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("foreach delegate", "foreach_deleg.exe");
      INPUT(t, "foreach_deleg.d",
        "struct S {\n"
        "   int i;\n"
        "   double d;\n"
        "   int j;\n"
        "   int foo(int delegate(ref int) dg) { \n"
        "       return dg(j);\n"
        "   }\n"
        "}\n"
        
        "void main() {\n"
        "   S s = { 13, 3.14159, 2 };\n"
        "   foreach(ref x; &s.foo) {\n"
        "       x = 4;\n"
        "   }\n"
        "   assert(s.j == 4);\n"
        "}\n")
    }

#if _WIN32
    { CompilationTest& t = cTest("enum_1", "enum_1.exe");
      INPUT(t, "enum_1.d",
            "enum Fu : byte {           \n"
            "   bah = 100,              \n"
            "   baz                     \n"
            "}                          \n"
            "void main() {              \n"
            "   Fu fu;                  \n"
            "   assert(fu == 100);      \n"
            "   fu = Fu.baz;            \n"
            "   assert(fu == 101);      \n"
            "}                          \n")
    }
#endif

    //test comma operator
    { CompilationTest& t = cTest("comma", "comma.exe");
      INPUT(t, "comma.d",
            "void main() {              \n"
            "   int a;                  \n"
            "   int b = 2;              \n"
            "   int c;                  \n"
            "   c = (a = 1, b++);       \n"
            "   assert(a == 1);         \n"
            "   assert(c == 2);         \n"
            "   assert(b == 3);         \n"
            "}                          \n")
    }

    //simple assoc array test
    { CompilationTest& t = cTest("simple assoc array", "saa.exe");
      INPUT(t, "saa.d",
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   a[\"fubar\"] = 123;     \n"
            "   int i = a[\"fubar\"];   \n"
            "   assert(i == 123);       \n"
            "}                          \n")
    }

    //global assoc array test
    { CompilationTest& t = cTest("global assoc array", "gaa.exe");
      INPUT(t, "gaa.d",
        "string aa[char[]];             \n"
        "void main() {                  \n"
        "   aa[\"fubar\"]=\"barfu\";    \n"
        "   assert(aa[\"fubar\"]==\"barfu\");\n"
        "}                              \n")
    }

    //assoc array length property test
    { CompilationTest& t = cTest("assoc array length", "saal.exe");
      INPUT(t, "saal.d",
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   int len = a.length;     \n"
            "   assert(len == 0);       \n"
            "   a[\"a\"] = 123;         \n"
            "   len = a.length;         \n"
            "   assert(len == 1);       \n"
            "   a[\"b\"] = 124;         \n"
            "   len = a.length;         \n"
            "   assert(len == 2);       \n"
            "}                          \n")
    }
#if _WIN32
    //assoc array 'keys' property test
    { CompilationTest& t = cTest("assoc array length", "saak.exe");
      INPUT(t, "saak.d",
            "void main() {              \n"
            "   int[int] a;             \n"
            "   a[3] = 11;              \n"
            "   a[4] = 12;              \n"
            "   int[] keys = a.keys;    \n"
            "   assert(keys.length == 2);\n"
            "   assert(keys[0] == 3);   \n"
            "   assert(keys[1] == 4);   \n"
            "}                          \n")
    }

    //assoc array 'keys' property test - string type
    { CompilationTest& t = cTest("assoc array length", "saaks.exe");
      INPUT(t, "saaks.d",
            "void main() {              \n"
            "   int[string] a;          \n"
            "   a[\"3\"] = 45;          \n"
            "   a[\"44\"] = 32;         \n"
            "   string[] keys = a.keys; \n"
            "   assert(keys.length == 2);\n"
            "   assert(keys[0] == \"3\");\n"
            "   assert(keys[1] == \"44\");\n"
            "}\n")
    }

    //assoc array 'values' property test - string type
    { CompilationTest& t = cTest("assoc array length", "saakv.exe");
      INPUT(t, "saakv.d",
            "void main() {              \n"
            "   string[string] a;       \n"
            "   a[\"3\"] = \"45\";      \n"
            "   a[\"44\"] = \"32\";     \n"
            "   string[] values = a.values; \n"
            "   assert(values.length == 2); \n"
            "   assert(values[0] == \"45\");\n"
            "   assert(values[1] == \"32\");\n"
            "}\n")
    }

    //assoc array literal
    { CompilationTest& t = cTest("assoc array literal", "aalit.exe");
      INPUT(t, "aalit.d",
            "void main() {              \n"
            "   int[char[]] a = [\"one\" : 1, \"two\" : 2];\n"
            "   int i = a[\"two\"];     \n"
            "   assert(i == 2);         \n"
            "}                          \n")
    }

    //assoc array of structs with postblit defined
    { CompilationTest& t = cTest("assoc array postblit", "aapblit.exe");
      INPUT(t, "aapblit.d",
          "struct X {\n"
          "     int i;\n"
          "     this(this) {\n"
          "         ++i;\n"
          "     }\n"
          "}\n"
          "void main() {\n"
          "     X [int] xs;\n"
          "     xs[0] = X();\n"
          "     int i = 42;\n"
          "     xs[i] = X();\n"
          "     xs[i++] = X();\n"
          "}\n")
    }
    //assoc array of structs with opAssign defined
    { CompilationTest& t = cTest("assoc array opAssign", "aao.exe");
      INPUT(t, "aao.d",
          "struct X {\n"
          "     int i;\n"
          "     void opAssign(ref X other) {\n"
          "         i = other.i;\n"
          "     }\n"
          "}\n"
          "void main() {\n"
          "     X [int] xs;\n"
          "     xs[0] = X();\n"
          "     int i = 42;\n"
          "     xs[i] = X();\n"
          "     xs[--i] = X();\n"
          "}\n")
    }
#endif
    //foreach over array of structs with postblit defined
    { CompilationTest& t = cTest("foreach array postblit", "feapblit.exe");
      INPUT(t, "feapblit.d",
          "struct X {\n"
          "     int i;\n"
          "     this(this) {\n"
          "         ++i;\n"
          "     }\n"
          "}\n"
          "void main() {\n"
          "     X [1] xs;\n"
          "     xs[0] = X();\n"
          "     foreach(x; xs) {\n"
          "         assert(x.i == 1);\n"
          "     }\n"
          "}\n")
    }

    //global assoc array literal
    { CompilationTest& t = cTest("global assoc array literal", "gaalit.exe");
      INPUT(t, "gaalit.d",
            "int[char[]] a;             \n"
            "void main() {              \n"
            "   a = [\"one\" : 1, \"two\" : 2];\n"
            "   int i = a[\"two\"];     \n"
            "   assert(i == 2);         \n"
            "}                          \n")
    }

    //global assoc array literal2
    { CompilationTest& t = cTest("global assoc array literal 2", "gaalit2.exe");
      INPUT(t, "gaalit2.d",
            "int[string] a = [\"one\" : 1, \"two\" : 2];\n"
            "void main() {              \n"
            "   int i = a[\"two\"];     \n"
            "   assert(i == 2);         \n"
            "}                          \n")
    }

    //assoc array literal argument
    { CompilationTest& t = cTest("assoc array literal arg", "aalit_arg.exe");
      INPUT(t, "aalit_arg.d",
            "void main() {              \n"
            "   assert( [\"one\" : 1, \"two\" : 2].length == 2 );\n"
            "}                          \n")
    }

    //assoc array literal foreach
    { CompilationTest& t = cTest("assoc array literal foreach", "aalit_foreach.exe");
      INPUT(t, "aalit_foreach.d",
            "void main() {\n"
            "   foreach(k, v; [\"one\" : 1, \"two\" : 2] ) {;\n"
            "   }\n"
            "}\n")
    }
#if _WIN32
    //another assoc array test: the key is returned from a function
    { CompilationTest& t = cTest("another assoc array", "aaa.exe");
      INPUT(t, "aaa.d",
            "string f(){return \"foo\";}\n"
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   a[\"foo\"] = 123;       \n"
            "   int i = a[f()];         \n"
            "   assert(i == 123);       \n"
            "}                          \n")
    }
    //another assoc array test: the key is missing
    { CompilationTest& t = cTest("another assoc array", "aaa2.exe");
      INPUT(t, "aaa2.d",
            "string f(){return \"foo\";}\n"
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   a[\"bar\"] = 123;       \n"
            "   bool ok = false;        \n"
            "   try {                   \n"
            "       int i = a[f()];     \n"
            "   } catch (Exception) {   \n"
            "       ok = true;          \n"
            "   }                       \n"
            "   assert(ok);             \n"
            "}                          \n")
    }

    //foreach assoc array test
    { CompilationTest& t = cTest("foreach assoc array", "faa.exe");
      INPUT(t, "faa.d",
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   a[\"fubar\"] = 123;     \n"
            "   a[\"bubah\"] = 456;     \n"
            "   int[] b = new int[2];   \n"
            "   int k = 0;              \n"
            "   foreach(i; a) {         \n"
            "       b[k++] = i;         \n"
            "   }                       \n"
            "   assert(b[0] == 123);    \n"
            "   assert(b[1] == 456);    \n"
            "}                          \n")
    }

    { CompilationTest& t = cTest("foreach assoc array 2", "faa2.exe");
      INPUT(t, "faa2.d",
            "import System;\n"
            "void main() {\n"
            "   int [string] x;\n"
            "   x[\"one\"] = 1;\n"
            "   x[\"two\"] = 2;\n"
            "   foreach (k, v; x) {\n"
            "       Console.WriteLine(\"{0}, {1}\".sys, k, v);\n"
            "   }\n"
            "}\n")
    }
    { CompilationTest& t = cTest("foreach assoc array ref", "faa_ref.exe");
      INPUT(t, "faa_ref.d",
            "void main() {              \n"
            "   int[char[]] a;          \n"
            "   a[\"fubar\"] = 123;     \n"
            "   a[\"bubah\"] = 456;     \n"
            "   foreach(ref i; a) {     \n"
            "       if (i==123) i = 13; \n"
            "       break;              \n"
            "   }                       \n"
            "   assert(a[\"fubar\"]==13);\n"
            "}                          \n")
    }
#endif // _WIN32
    //test method override
    { CompilationTest& t = cTest("method override", "method_override.exe");
      INPUT(t, "method_override.d",
            "void main() {              \n"
            "   class Base {            \n"
            "       public void foo() { \n"
            "           assert(false);  \n"
            "       }                   \n"
            "       public void bar() { \n"
            "       }                   \n"
            "   }                       \n"
            "   class Derived : Base {  \n"
            "       override void foo(){\n"
            "       }                   \n"
            "       public void bar() { \n"
            "           assert(false);  \n"
            "       }                   \n"
            "   }                       \n"
            "   Base b = new Derived;   \n"
            "   b.foo();                \n"
            "   b.bar();                \n"
            "}                          \n")
    }

    //test invoking base class ctor explicitly with super
    { CompilationTest& t = cTest("super", "super.exe");
      INPUT(t, "super.d",
            "void main() {                  \n"
            "   class Base {                \n"
            "       public int ok = -1;     \n"
            "       public this(int) {      \n"
            "           assert(ok == -1);   \n"
            "           ok = 1;             \n"
            "       }                       \n"
            "   }                           \n"
            "   class Derived : Base {      \n"
            "       public this(int i){     \n"
            "           assert(ok == -1);   \n"
            "           super(i);           \n"
            "       }                       \n"
            "   }                           \n"
            "   Base b = new Derived(42);   \n"
            "   assert(b.ok == 1);          \n"
            "}                              \n")
    }

#if _WIN32
    //test suppport for special $ symbol
    { CompilationTest& t = cTest("dollar", "dollar.exe");
      INPUT(t, "dollar.d",
        "void main()                        \n"
        "{                                  \n"
        "    double d[] = [1.1, 2.2, 3.3];  \n"
        "    double[] a = d[0..$];          \n"
        "    assert(a.length == d.length);  \n"
        "    a = d[1..$];                   \n"
        "    assert(a.length==d.length - 1);\n"
        "    double x = d[length - 1];      \n"
        "    assert(x == 3.3);              \n"
        "}\n")
    }
#endif
    //test string.length
    { CompilationTest& t = cTest("string.length", "strlen.exe");
      INPUT(t, "strlen.d",
        "void main()                        \n"
        "{                                  \n"
        "    string s = \"Alice\";          \n"
        "    assert(s.length == 5);         \n"
        "}\n")
    }

    //test string.ptr -- this case is interesting because
    //the front-end optimizes out the .ptr part
    { CompilationTest& t = cTest("string.ptr", "str_ptr.exe");
      INPUT(t, "str_ptr.d",
        "void main()                        \n"
        "{                                  \n"
        "    invariant char* p=\"ABC\".ptr; \n"
        "    assert(*p == 'A');             \n"
        "}\n")
    }
#if _WIN32 // unverifiable code
    { CompilationTest& t = cTest("string.ptr deref", "str_ptr2.exe");
      INPUT(t, "str_ptr2.d",
        "void main()                        \n"
        "{                                  \n"
        "   immutable (char)* p;            \n"
        "   p = \"ABC\".ptr;                \n"
        "   ++p;                            \n"
        "   assert(*p == 'B');              \n"
        "}\n")
    }
#endif
    //test string.ptr 2
    { CompilationTest& t = cTest("string var ptr", "str_var_ptr.exe");
      INPUT(t, "str_var_ptr.d",
        "void main()                        \n"
        "{                                  \n"
        "    string x=\"ABC\";              \n"
        "    invariant char* p=x.ptr;       \n"
        "    assert(*p == 'A');             \n"
        "}\n")
    }

    //test string_literal.len
    { CompilationTest& t = cTest("string_literal.len", "str_len.exe");
      INPUT(t, "str_len.d",
        "void main()                        \n"
        "{                                  \n"
        "    int len = \"ABC\".length;      \n"
        "    assert(len == 3);              \n"
        "}\n")
    }

    //test string.dup
    { CompilationTest& t = cTest("string.dup", "str_dup.exe");
      INPUT(t, "str_dup.d",
        "void main()                        \n"
        "{                                  \n"
        "    string s = \"abc\";            \n"
        "    assert(s.dup == \"abc\");      \n"
        "}\n")
    }

    { CompilationTest& t = cTest("string.sort", "str_sort.exe");
      INPUT(t, "str_sort.d",
        "void main()                        \n"
        "{                                  \n"
        "    string s = \"bac\";            \n"
        "    assert(s.sort == \"abc\");     \n"
        "}\n")
    }

    { CompilationTest& t = cTest("string.reverse", "str_reverse.exe");
      INPUT(t, "str_reverse.d",
        "void main()                        \n"
        "{                                  \n"
        "    string s = \"abc\";            \n"
        "    assert(s.reverse == \"cba\");  \n"
        "}\n")
    }
#if _WIN32
    //test user-defined property
    { CompilationTest& t = cTest("string.my_len", "my_len.exe");
      INPUT(t, "my_len.d",
        "int my_len(string s) {             \n"
        "   return s.length;                \n"
        "}                                  \n"
        "void main()                        \n"
        "{                                  \n"
        "    string s = \"Alice\";          \n"
        "    assert(s.my_len == 5);         \n"
        "}\n")
    }
#endif
    { CompilationTest& t = cTest("Nested Functions", "nested_fun.exe");
      INPUT(t, "nested_fun.d",     
        "int i;                             \n"
        "void main()                        \n"
        "{                                  \n"
        "   int j;                          \n"
        "   void foo() {                    \n"
        "       void bar() {                \n"
        "           ++i;                    \n"
        "           ++j;                    \n"
        "       }                           \n"
        "       bar();                      \n"
        "   }                               \n"
        "   foo();                          \n"
        "   assert(i == 1);                 \n"
        "   foo();                          \n"
        "   assert(i == 2);                 \n"
        "   assert(j == 2);                 \n"
        "}\n")
    }

     { CompilationTest& t = cTest("Template method call", "tmc.exe");
      INPUT(t, "tmc.d",
        "class Test {                       \n"
        "   int foo(T)(T arg) {             \n"
        "       return arg + 40;            \n"
        "   }                               \n"
        "}                                  \n"
        "void main()                        \n"
        "{                                  \n"
        "   Test t = new Test;              \n"
        "   assert(t.foo(2) == 42);         \n"
        "}\n")
    }

    { CompilationTest& t = cTest("Struct template method call", "stmc.exe");
      INPUT(t, "stmc.d",     
        "struct Test {                      \n"
        "   int foo(T)(T arg) {             \n"
        "       return arg + 40;            \n"
        "   }                               \n"
        "}                                  \n"
        "void main()                        \n"
        "{                                  \n"
        "   Test t;                         \n"
        "   assert(t.foo(2) == 42);         \n"
        "}\n")
    }

    { CompilationTest& t = cTest("Variadic template call", "vtc.exe");
      INPUT(t, "vtc.d",
        "auto max(T1, T2, Tail...)(T1 first, T2 second, Tail args){\n"
        "    auto r = second > first ? second : first;\n"
        "    static if (Tail.length == 0) {\n"
        "        return r;\n"
        "    }\n"
        "    else {\n"
        "        return max(r, args);\n"
        "    }\n"
        "}\n"
        "void main() {\n"
        "   uint k = 42;\n"
        "    auto i = max(3, 2.5, k);\n"
        "    assert(i == 42);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Member nested function", "mnf.exe");
      INPUT(t, "mnf.d",
        "class Test {\n"  
        "   int i;\n"
        "   int j = 13;\n"
        "   public void outer() {\n"
        "       int j = 1;\n"
        "       i = 41;\n"
        "       void inner() { \n"
        "           assert(i + j == 42);\n"
        "       }\n"
        "       inner();\n"
        "   }\n"
        "}\n"
        "void main() {\n"
        "   Test t = new Test;\n"
        "   t.outer();\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Struct member nested function", "smnf.exe");
      INPUT(t, "smnf.d",
        "struct Test {\n"  
        "   int i;\n"
        "   int j = 13;\n"
        "   public void outer() {\n"
        "       int j = 1;\n"
        "       i = 41;\n"
        "       void inner() { \n"
        "           assert(i + j == 42);\n"
        "       }\n"
        "       inner();\n"
        "   }\n"
        "}\n"
        "void main() {\n"
        "   Test t;\n"
        "   t.outer();\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Non-member nested function", "nmnf.exe");
      INPUT(t, "nmnf.d",
        "   void outer() {\n"
        "       int j = 1;\n"
        "       void inner() { \n"
                "   int i = 41;\n"
        "           assert(i + j == 42);\n"
        "       }\n"
        "       inner();\n"
        "   }\n"
        "void main() {\n"
        "   outer();\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Ctor delegation", "ctor_deleg.exe");
      INPUT(t, "ctor_deleg.d",
        "class Test {\n"  
        "   int i = 13;\n"
        "   this() { }\n"
        "   this(int j) { i = j; this(); }\n"
        "}\n"
        "void main() {\n"
        "   Test t = new Test(42);\n"
        "   assert(t.i == 42);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Class invariant", "class_invariant.exe");
      INPUT(t, "class_invariant.d",
        "bool ok = false;\n"
        "class Test {\n"
        "   invariant() { ok = true; }\n"
        "}\n"
        "void main() {\n"
        "   Test t = new Test;\n"
        "   assert(ok);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("Class invariant assert", "class_inv_assert.exe");
      INPUT(t, "class_inv_assert.d",
        "bool ok = false;\n"
        "class Test {\n"
        "   invariant() { ok = true; }\n"
        "}\n"
        "void main() {\n"
        "   Test t = new Test;\n"
        "   assert(t);\n"
        "   assert(ok);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("interface", "interface.exe");
       INPUT(t, "interface.d",
        "uint len = 0;\n"
        "interface Foo {\n"
        "   void exec(int []);\n"
        "}\n"
        "class Test : Foo {\n"
        "   public void exec(int[] a) {\n"
        "       len = a.length;\n"
        "   }\n"
        "}\n"
        "void main() {\n"
        "   int x[] = [1, 2, 3];\n"
        "   Test f = new Test;\n"
        "   f.exec(x);\n"
        "   assert(len == 3);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("delegates", "delegates.exe");
      INPUT(t, "delegates.d",
          "int delegate(int) dg;\n"
          "int foo = 42;\n"
          "class Test {\n"
          " int i = 123;\n"
          " int run(int j) {\n"
          "     foo = i + j;\n"
          "     return j;\n"
          " }\n"
          " static void hide() { foo = 13; }\n"
          "}\n"
          "void main() {\n"
          " Test t = new Test;\n"
          " dg = &t.run;\n"
          " assert(foo == 42);\n"
          " assert(t.i == 123);\n"
          " dg(7);\n"
          " assert(foo == 130);\n"
          " void (*pf)() = &Test.hide;\n"
          " pf();\n"
          " assert(foo == 13);\n"
          "}\n")
    }

    { CompilationTest& t = cTest("nested delegate", "nest_deleg.exe");
      INPUT(t, "nest_deleg.d",
        "int j;\n"
        "void delegate() foo(int i) {\n"
        "void bar() { \n"
        "   j = i;\n"
        "}\n"
        "void delegate() dg = &bar;\n"
        "return dg;\n"
        "}\n"
        "void main() { \n"
        "void delegate() dg = foo(6);\n"
        "dg();\n"
        "assert(j == 6);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("param by ref", "ref_1.exe");
      INPUT(t, "ref_1.d",
        "void bar(ref int x) { \n"
        "   x = 42;\n"
        "}\n"
        "void main() { \n"
        "   int i;\n"
        "   assert(i == 0);\n"
        "   bar(i);\n"
        "   assert(i == 42);\n"
        "}\n")
    }
#if _WIN32
    { CompilationTest& t = cTest("array by ref", "ref_2.exe");
      INPUT(t, "ref_2.d",
        "int[] x = [1, 2, 3];\n"
        "void bar(ref int[] a) { \n"
        "   a = x;\n"
        "}\n"
        "void main() { \n"
        "   int b[];\n"
        "   assert(b.length == 0);\n"
        "   bar(b);\n"
        "   assert(b.length == 3);\n"
        "}\n")
    }
#endif
    { CompilationTest& t = cTest("return by ref", "ref_3.exe");
      INPUT(t, "ref_3.d",
        "struct X { int i; }\n"
        "class Test {\n"
        "   double d;\n"
        "   Test t;\n"
        "   X myx;\n"
        "   ref double fun() { \n"
        "       return d;\n"
        "   }\n"
        "   ref Test test() { \n"
        "       return t;\n"
        "   }\n"
        "   ref X x() {\n"
        "       return myx;\n"
        "   }\n"
        "}\n"
        "void main() { \n"
        "   Test t = new Test;\n"
        "   t.fun() = 3.14159;\n"
        "   t.test() = new Test();\n"
        "   assert(t.test().d == 0);\n"
        "   assert(t.d == 3.14159);\n"
        "   X x;\n"
        "   x.i = 42;\n"
        "   t.x() = x;\n"
        "   assert(t.myx.i == 42);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("method w/ param by ref", "ref_4.exe");
      INPUT(t, "ref_4.d",
        "class Test {\n"
        "   int i;\n"
        "   void f(ref int value) { \n"
        "       i = value;\n"
        "   }\n"
        "}\n"
        "void main() { \n"
        "   Test t = new Test;\n"
        "   int k = 42;\n"
        "   t.f(k);\n"
        "   assert(t.i == 42);\n"
        "}\n")
    }
    
    { CompilationTest& t = cTest("var args by ref", "ref_5.exe");
      INPUT(t, "ref_5.d",
        "import System;\n"
        "void bar(ref int x) { \n"
        "   Console.WriteLine(\"x={0}\".sys, x);\n"
        "}\n"
        "void main() { \n"
        "   int i;\n"
        "   bar(i);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("modify ret ref", "ref_6.exe");
      INPUT(t, "ref_6.d",
        "int i;\n"
        "ref int f() { \n"
        "   return i;\n"
        "}\n"
        "void main() { \n"
        "   assert((f() = 2) == 2);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("modify ret ref 2", "ref_7.exe");
      INPUT(t, "ref_7.d",
        "int i;\n"
        "ref int f() {\n"
        "   return i;\n"
        "}\n"
        "void main() {\n"
        "   assert(++f() == 1);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("modify ret ref 3", "ref_8.exe");
      INPUT(t, "ref_8.d",
        "int i = 1;\n"
        "ref int f() {\n"
        "   return i;\n"
        "}\n"
        "void main() {\n"
        "   assert((f() += 1) == 2);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("ret ref / postinc", "ref_9.exe");
      INPUT(t, "ref_9.d",
        "int i;\n"
        "ref int f() {\n"
        "   return i;\n"
        "}\n"
        "void main() {\n"
        "   assert((f()++) == 0);\n"
        "   assert(i == 1);\n"
        "}\n")
    }
#if _WIN32 // todo!
    { CompilationTest& t = cTest("var args forwarding", "vargs_fwd.exe");
      INPUT(t, "vargs_fwd.d",
        "import System;\n"
        "void bar(string fmt, ...) { \n"
        "   Console.WriteLine(\"fmt\".sys, _arguments);\n"
        "}\n"
        "void main() { \n"
        "   int i;\n"
        "   bar(\"(0}, {1}\", \"test\".sys, i);\n"
        "}\n")
    }
#endif
    { CompilationTest& t = cTest("multi-source", "multi.exe");
      INPUT(t, "multi.d",
        "class Test {\n"
        "   int i;\n"
        "   void f(ref int x) { \n"
        "       i = x;\n"
        "   }\n"
        "}\n")

      INPUT(t, "multi_2.d",
        "import multi;\n"
        "void main() { \n"
        "   Test t = new Test;\n"
        "   int k = 42;\n"
        "   t.f(k);\n"
        "   assert(t.i == 42);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("struct fields", "struct_fields.exe");
      INPUT(t, "struct_fields.d",
        "struct Test {\n"
        "   int value;\n"
        "   int f() { \n"
        "       return value;\n"
        "   }\n"
        "}\n"
        "void main() { \n"
        "   Test t = { 123 };\n"
        "}\n")
    }
    //foreach over tuples
    { CompilationTest& t = cTest("foreach over tuples", "feot.exe");
      INPUT(t, "feot.d",
        "import System;\n"
        "void print(A...)(A a) {\n"
        "   foreach(t; a) {\n"
        "       Console.WriteLine(t);\n"
        "   }\n"
        "}\n"
        "void main() {\n"
        "    print(\"Hello\".sys, 1, 3.14159);\n"
        "}\n")
    }

    //foreach over tuples w/ indices
    { CompilationTest& t = cTest("foreach over tuples w/ indices", "feot2.exe");
      INPUT(t, "feot2.d",
        "import System;\n"
        "void print(A...)(A a) {\n"
        "   foreach(i, t; a) {\n"
        "       Console.WriteLine(i);\n"
        "       Console.WriteLine(t);\n"
        "   }\n"
        "}\n"
        "void main() {\n"
        "    print(\"Hello\".sys, 1, 3.14159);\n"
        "}\n")
    }

    // Fibonacci
    { CompilationTest& t = cTest("Fibonacci", "fib.exe");
        INPUT(t, "fib.d",
        "uint fib(uint n) {\n"
        "   uint iter(uint i, uint fib_1, uint fib_2) {\n"
        "       return i == n ? fib_2 : iter(i + 1, fib_1 + fib_2, fib_1);\n"
        "   }\n"
        "   return iter(0, 1, 0);\n"
        "}\n"
        "void main() {\n"
        "   assert(fib(1) == 1);\n"
        "   assert(fib(2) == 1);\n"
        "   assert(fib(3) == 2);\n"
        "   assert(fib(4) == 3);\n"
        "   assert(fib(5) == 5);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("array member append", "ama.exe");
      INPUT(t, "ama.d",
        "struct X {\n"
        "    int a[];\n"
        "}\n"
        "void main() {\n"
        "    X x;\n"
        "    x = X();\n"
        "    x.a ~= 42;\n"
        "    assert(x.a.length == 1);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("array member init", "ami.exe");
      INPUT(t, "ami.d",
        "struct X {\n"
        "    int a[];\n"
        "}\n"
        "void f(X x) {\n"
        "   foreach(i; x.a) {\n"
        "       assert(i == 1);\n"
        "   }\n"
        "   assert(x.a.length == 3);\n"
        "}\n"
        "void main() {\n"
        "    X x;\n"       
        "    x.a = [1, 1, 1];\n"
        "}\n")
    }
    
    { CompilationTest& t = cTest("array member append 2", "ama2.exe");
      INPUT(t, "ama2.d",
        "struct X {\n"
        "    int a[];\n"
        "}\n"
        "void main() {\n"
        "    X x[int];\n"
        "    x[0] = X();\n"
        "    x[0].a ~= 42;\n"
        "    assert(x[0].a.length == 1);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("array member append 3", "ama3.exe");
      INPUT(t, "ama3.d",
        "struct X {\n"
        "   int a[];\n"
        "   double d;\n" 
        "}\n"
        "struct Y {\n"
        "   double d;\n" 
        "   X x;\n"
        "}\n"
        "void main() {\n"
        "    Y y[int];\n"
        "    y[0] = Y();\n"
        "    y[0].x.a ~= 42;\n"
        "    assert(y[0].x.a.length == 1);\n"
        "}\n")
    }

    { CompilationTest& t = cTest("in-remove", "inremove.exe");
      INPUT(t, "inremove.d",
        "int a[string] = [\"foo\" : 1, \"bar\" : 2];\n"
        "void main() {\n"
        "    assert(\"foo\" in a);\n"
        "    string b = \"bar\";\n"
        "    a.remove(b);\n"
        "    assert(!(b in a));\n"
        "}\n")
    }

    { CompilationTest& t = cTest("forward module reference", "moduleB.exe");
      INPUT(t, "moduleB.d",
          "import moduleA;\n"
          "static this()\n"
          "{\n"
          "     map[\"hello\"] = \"world\";\n"
          "}\n")

      INPUT(t, "moduleA.d",
            "import System;\n"
            "string [string] map;\n"
            "void main()\n"
            "{\n"
            "   assert(map.length);\n"
            "   foreach(k, v; map)\n"
            "       Console.WriteLine(\"{0} -> {1}\".sys, k, v.sys);\n"
            "}\n")
    }

    { CompilationTest& t = cTest("Hidden method call", "hmc.exe");
      INPUT(t, "hmc.d",
        "class Base {                       \n"
        "   public int foo(int arg) {       \n"
        "       return 0;                   \n"
        "   }                               \n"
        "   public long foo(long arg) {     \n"
        "       return arg;                 \n"
        "   }                               \n"
        "}                                  \n"
        "class Derived : Base {             \n"
        "   public override long foo(long i){\n"
        "       return i;                   \n"
        "   }                               \n"
        "}                                  \n"
        "void main()                        \n"
        "{  bool ok = false;                \n"
        "   Base b = new Derived;           \n"
        "   try {                           \n"
        "       assert(b.foo(42) != 42);    \n"
        "   }                               \n"
        "   catch (Exception) {             \n"
        "       ok = true;                  \n"
        "   }                               \n"
        "   assert(true);                   \n"
        "}\n")
    }

#if _WIN32 //todo
    { CompilationTest& t = cTest("vargs_typeid", "vatid.exe");
      INPUT(t, "vatid.d",
       "import System;\n"
       "void fun(...) {\n"
       "    int ok;\n"
       "    foreach(arg; _arguments)\n"
       "    {\n"
       "         if (_argtype(arg) == typeid(int))\n"
       "         {\n"
       "             int i = arg;\n"
       "             Console.WriteLine(\"int={0}\".sys, i);\n"
       "             ++ok;\n"
       "         }\n"
       "         else if (_argtype(arg) == typeid(string))\n"
       "         {\n"
       "             string s = arg;\n"
       "             Console.WriteLine(s.sys);\n"
       "             ++ok;\n"
       "         }\n"
       "     }\n"
       "     assert(ok == 2);\n"
       "}\n"
       " void main() {\n"
       "    fun(1, 2.0, \"three\");\n"
       "}\n")
    }
#endif

    { CompilationTest& t = cTest("lazy argument evaluation", "lazy.exe");
      INPUT(t, "lazy.d",
          "void f(lazy int dg) {\n"
          "     assert(dg() == 42);\n"
          "}\n"
          "int g(int i) { return i; }\n"
          "void main() {\n"
          "     f(g(42));\n"
          "}\n")    
    }

    { CompilationTest& t = cTest("template alias", "t_alias.exe");
      INPUT(t, "t_alias.d",
            "int x;\n"
            "template Foo(alias X)\n"
            "{\n"
            "    static int* p = &X;\n"
            "}\n"
            "void main()\n"
            "{\n"
            "    alias Foo!(x) bar;\n"
            "    *bar.p = 3;		// set x to 3\n"
            "    assert(x == 3);\n"
            "    static int y;\n"
            "    alias Foo!(y) abc;\n"
            "    *abc.p = 3;		// set y to 3\n"
            "    assert(y == 3);\n"
            "}\n")
    }
    
    { CompilationTest& t = cTest("escape sequences", "esc.exe");
      INPUT(t, "esc.d",
            "void main()\n"
            "{\n"
            "    string hello = \"foo\\nbar\";\n"
            "    string hello2 = \"foo\\\"bar\";\n"
            "    string hello3 = \"\\\\\";\n"
            "    mixin(\"string s = \\\"ula\\\";\");\n"
            "    char c = '\\n';\n"
            "    assert(c == 10);\n"
            "}\n")
    }

    {   CompilationTest& t = cTest("nested class", "nested.exe");
        INPUT(t, "nested.d",
            "class Outer {\n"
            "   class Inner {\n"
            "       static void f() {\n"
            "       }\n"
            "   }\n"
            "}\n"
            "void main() {\n"
            "   Outer.Inner.f();\n"
            "}\n")
        }
}


int main(int argc, char* argv[])
{
    string flags =
#ifdef DEBUG
        "-debug ";
#else
        //"-w ";
        "";
#endif

    for (--argc, ++argv; argc; --argc, ++argv)
    {
        if (strcmp(*argv, "-cleanup") == 0)
        {
            flags = "-cleanup";
            break;
        }
        flags += *argv;
        flags += " ";
    }
    static size_t total = 0;
    bool result = false;
    try
    {
        init();
        total = smokeTest.size();

        result = run(flags.c_str());
    }
    catch (const exception& e)
    {
        clog << e.what() << endl;
    }
    clog << "Passed " << passedCount << " / " << total << endl;
    return result ? 0 : -1;
}
