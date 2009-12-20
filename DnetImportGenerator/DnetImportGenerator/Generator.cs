/********************************************************************
 *       This file is distributed under the Microsoft Public        *
 *       License (Ms-PL). See license.txt for details.              *
 *       Copyright (c) 2009 Tim Matthews & Cristian L. Vlasceanu    *
 *       All rights reserved                                        *
 * *****************************************************************/

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Threading;

namespace DnetImportGenerator
{
    class Generator
    {
        static List<NS> namespaces = new List<NS>();
        static ImportType impType;
        static List<Thread> threads = new List<Thread>();

        public static string trimTemplate(string s)
        {
            if (s.Contains("`"))
                return s.Remove(s.IndexOf('`'));
            else
                return s;
        }

        public static ImportType getImportType()
        {
            return impType;
        }

        public static bool isPrimitive(Type type)
        {
            string s = elementType(type).Name;

            if (s == "Object")
                return true;
            else if (s == "Void")
                return true;
            else if (s == "Boolean")
                return true;
            else if (s == "Byte")
                return true;
            else if (s == "SByte")
                return true;
            else if (s == "Char")
                return true;
            else if (s == "Single")
                return true;
            else if (s == "Double")
                return true;
            else if (s == "Int16")
                return true;
            else if (s == "Int32")
                return true;
            else if (s == "Int64")
                return true;
            else if (s == "UInt16")
                return true;
            else if (s == "UInt32")
                return true;
            else if (s == "UInt64")
                return true;
            else if (s == "IntPtr")
                return true;
            else if (s == "UIntPtr")
                return true;
            else if (s == "Enum")
                return true;
            else
                return false;
        }

        public static NS getCreateNS(string name)
        {
            NS n = namespaces.Find(delegate(NS ns) { return ns.name == name; });
            if (n == null)
            {
                n = new NS(name);
                namespaces.Add(n);
            }
            return n;
        }

        public static NS getNS(string name)
        {
            return namespaces.Find(delegate(NS ns) { return ns.name == name; });
        }



        public static bool isGenPar(Type type)
        {
            return elementType(type).IsGenericParameter;
        }

        public static Type elementType(Type type)
        {
            Type t = type;
            while (t.HasElementType)
                t = t.GetElementType();
            return t;
        }

        public static string getNestedName(Type type)
        {
            Type t = type.DeclaringType;
            string s = type.Name;
            while (t != null)
            {
                s = t.Name + "." + s;
                t = t.DeclaringType;
            }
            return s;
        }

        public static BindingFlags memberBindingFlags()
        {
            return (BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly);
        }

        public static string nameConvert(Type type, bool expandtemplate, bool prefixNS, bool includeTemplateConstraints, string currentNS)
        {
            string s = elementType(type).Name;

            if (s == "Void")
                s = type.Name.Replace("Void", "void");
            else if (s == "Boolean")
                s = type.Name.Replace("Boolean", "bool");
            else if (s == "Byte")
                s = type.Name.Replace("Byte", "ubyte");
            else if (s == "SByte")
                s = type.Name.Replace("SByte", "byte");
            else if (s == "Char")
                s = type.Name.Replace("Char", "wchar");
            else if (s == "Single")
                s = type.Name.Replace("Single", "float");
            else if (s == "Double")
                s = type.Name.Replace("Double", "double");
            else if (s == "Int16")
                s = type.Name.Replace("Int16", "short");
            else if (s == "Int32")
                s = type.Name.Replace("Int32", "int");
            else if (s == "Int64")
                s = type.Name.Replace("Int64", "long");
            else if (s == "UInt16")
                s = type.Name.Replace("UInt16", "ushort");
            else if (s == "UInt32")
                s = type.Name.Replace("UInt32", "uint");
            else if (s == "UInt64")
                s = type.Name.Replace("UInt64", "ulong");
            else if (s == "Object")
                s = type.Name;
            else if (s == "IntPtr")
                s = type.Name.Replace("IntPtr", "int*");
            else if (s == "UIntPtr")
                s = type.Name.Replace("UIntPtr", "uint*");
            else
            {
                if (prefixNS && !isGenPar(type))
                {
                    if (type.Namespace != currentNS)
                        s = type.Namespace + "." + getNestedName(type);
                    else
                        s = getNestedName(type);
                }
                else
                    s = type.Name;
            }

            if (s.Contains("`"))
            {
                s = trimTemplate(s);
                if (expandtemplate)
                {
                    Type[] types = type.GetGenericArguments();
                    if (types.Length == 0)
                        s += "!()";
                    else
                    {
                        s += "!(";
                        foreach (Type t in types)
                        {
                            Type[] gencs = null;
                            s += nameConvert(t, true, true, false, currentNS);
                            if (t.IsGenericParameter && includeTemplateConstraints)
                            {
                                gencs = t.GetGenericParameterConstraints();
                                if (gencs.Length != 0)
                                {
                                    if (gencs[0].IsValueType)
                                    {
                                        if (!gencs[0].IsPrimitive)
                                        {
                                            //template must be instantiated with a specific type of struct
                                        }
                                    }
                                    else
                                    {
                                        if (gencs[0].Name == "ValueType")
                                        {
                                            //template must be instantiated with any kind of struct
                                        }
                                        else
                                        {
                                            s += " : " + nameConvert(gencs[0], false, true, false, currentNS);
                                        }
                                    }
                                }
                            }
                            s += ", ";
                        }
                        s = s.Remove(s.Length - 2) + ")";
                    }
                }
                    
            }

            if (s[s.Length - 1] == '&') //visual c++ return by ref
                s = "ref " + s.Substring(0, s.Length - 1);

            return s;
        }

        //Generate the imports files.
        //
        //Params:
        //importDir: where to output generated imports
        //doPragmas, doOverride:  should usually be true but specifiy false to make testing easier
        public static void GenerateDImports(string importDir, bool doPragmas, bool doOverride)
        {
            impType = ImportType.Private;
            List<Assembly> explicitAssemblies = new List<Assembly>();

            FileStream fs;
            try
            {
                fs = new FileStream("./assemblylist", FileMode.Open ,FileAccess.Read);
            }
            catch(FileNotFoundException) { Console.WriteLine("Could not open the config file"); return; }
            StreamReader sr = new StreamReader(fs);

            while (!sr.EndOfStream)
            {
                string line = sr.ReadLine();
                if (line==null)
                    continue;
                if (line.StartsWith("//"))
                    continue;
                try
                {
                    explicitAssemblies.Add(Assembly.Load(line));
                }
                catch (Exception) { Console.WriteLine("Could not load assembly: " + line); }
            }

            List<Assembly> allAssemblies = new List<Assembly>(explicitAssemblies);

            /* Find all referenced */
            foreach (Assembly assembly in explicitAssemblies)
            {
                AssemblyName[] referenced = assembly.GetReferencedAssemblies();
                foreach(AssemblyName assemname in referenced)
                {
                    Assembly assem = Assembly.Load(assemname);
                    if (!allAssemblies.Contains(assem))
                        allAssemblies.Add(assem);
                }
            }

            Console.WriteLine("Loading types from assemblies:\n-------------------------------------");
            foreach (Assembly assembly in allAssemblies)
            {
                Console.WriteLine(assembly.GetName().Name + "(" + assembly.GetName().Version + ")");
                //try
                //{
                    Type[] assemtypes = assembly.GetExportedTypes();
                    foreach (Type type in assemtypes)
                    {
                        if (type.Namespace != null)
                        {
                            NS ns = getCreateNS(type.Namespace);
                            ns.addType(type);
                        }
                    }
                //}
                //catch (FileNotFoundException) { }
            }                

            Console.WriteLine("\n\nCurrent Namespace:\n-------------------------------------");
            foreach (NS ns in namespaces)
            {
                Thread th = new Thread(new ParameterizedThreadStart(ns.makeFiles));
                th.Start(new object[] { importDir, doPragmas, doOverride });
                threads.Add(th);
            }
            foreach (Thread th in threads)
                th.Join();

        }
    }
}
