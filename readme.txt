Dnet is a D compiler that targets the common language runtime. It is based on the dmd frontend (currently version 2.036) (http://digitalmars.com/d/) with backend code to generate common intermediate language.

It is in a very alpha like state to prove the concepts and spark the interest of compiler developers.

Currently dnet knows of the interfaces to CLR code based on '.di' import files. This is currently the way due to it's dmd inheritance (which uses files). Generating the import files is done like so:

1) DnetImportGenerator -l

2) A list of assemblies found in the GAC is created as ./assemblylist. Remove the leading '//' from each line of the desired assemblies (mscorlib is uncommented by default).

3) DnetImportGenerator -i creates the actuall import files. Each import file describes the interface to the CLR code and the assembly that it is defined in.

The import files should be in the import search path so the dnet compiler can find them in a similar way as dmd would find them.

Running "dnet hello.d" will generate hello.il, then hello.il will be used to create hello.exe by ilasm (a Microsoft tool taht should also be in your path)

The generated executables currently rely on the dnetlib.dll file that should be copied to the same directory.

Building the compiler and other tools:

In Visual Studio on a windows system it can be built be opening up the sollution file '.sln' and hitting f7.

Compiling the dnet assembly place-holder under Visual Studio Express 2008:
csc /t:library /out:dnetlib.dll /debug+ marz\runtime\runtime.cs

The compiler can be also built on Linux (although the .NET backend does not work very well on that platform as of now).

Compiling dnet under Linux:
cd dmd
make depend
make

Compiling the dnetlib assembly (runtime support) under linux:
gmcs -debug+ -t:library -out:dnetlib.dll marz/runtime/runtime.cs


