/********************************************************************
 *       This file is distributed under the Microsoft Public        *
 *       License (Ms-PL). See license.txt for details.              *
 *       Copyright (c) 2009 Tim Matthews & Cristian L. Vlasceanu    *
 *       All rights reserved                                        *
 * *****************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.IO;

namespace DnetImportGenerator
{
    class NS
    {
        public string name = null;
        StreamWriter fileData;
        List<Type> types = new List<Type>();
        List<TypeRequirement> typeReqs = new List<TypeRequirement>();
        List<DImport> imports = new List<DImport>();
        public DImport getCreateDImport(string name)
        {
            DImport import = imports.Find(delegate(DImport imp) { return imp.name == name; });
            if (import == null)
            {
                import = new DImport(name);
                imports.Add(import);
            }
            return import;
        }
        public NS(string name)
        {
            this.name = name;
        }
        public void addType(Type type)
        {
            if (!types.Contains(type))
            {
                types.Add(type);
                typeReqs.Add(new TypeRequirement(type, this));
            }
        }
        public bool hasGenericVersion(Type type)
        {
            if (type.Namespace != name)
            {
                NS ns = Generator.getNS(type.Namespace);
                if (ns == null)
                    return false;
                else
                    return ns.hasGenericVersion(type);
            }
            foreach (Type t in types)
            {
                if ((Generator.trimTemplate(t.Name) == Generator.trimTemplate(type.Name)) && (t.IsGenericTypeDefinition))
                    return true;
            }
            return false;
        }
        void reOrderTypes(ref List<Type> types)
        {
            List<Type> orderedTypes = new List<Type>();
            foreach (Type t in types) //enum
            {
                if (t.IsEnum)
                    orderedTypes.Add(t);
            }
            foreach (Type t in types) //struct
            {
                if (!t.IsEnum && t.IsValueType && !t.IsPrimitive)
                    orderedTypes.Add(t);
            }
            foreach (Type t in types) //interface
            {
                if (t.IsInterface)
                    orderedTypes.Add(t);
            }
            foreach (Type t in types) //delegates
            {
                if (t.IsSubclassOf(typeof(MulticastDelegate)))
                    orderedTypes.Add(t);
            }
            while(orderedTypes.Count<types.Count)
            {
                foreach (Type t in types)
                if (!orderedTypes.Contains(t))
                {
                    /* Do the classes but do the base classes that are from same ns first */
                    if( (t.BaseType == null) || (!types.Contains(t.BaseType)) || (orderedTypes.Contains(t.BaseType)) )
                        orderedTypes.Add(t);
                }
            }
            types = orderedTypes;
        }
        void addIndent(int indentCount)
        {
            for (int i = 0; i < indentCount; i++)
                fileData.Write("    ");
        }
        void addParamString(MethodBase mb)
        {
            ParameterInfo[] pars = mb.GetParameters();
            fileData.Write("(");
            foreach (ParameterInfo pi in pars)
            {
                Type paramType = pi.ParameterType;
                string s = Generator.nameConvert(paramType, true, !Generator.isGenPar(pi.ParameterType), false, name);
                string r = s.Replace("*", "");
                if (s.Contains("&"))
                {
                    s = s.Remove(s.IndexOf('&'), 1);
                    r = r.Remove(r.IndexOf('&'), 1);
                    if (pi.IsOut)
                        s = "out " + s;
                    else
                        s = "ref " + s;
                }
                paramType = Generator.elementType(paramType);
                if (!paramType.IsVisible)
                {
                    s = s.Replace(r, "Object");
                }
                else
                {
                    NS n = Generator.getNS(paramType.Namespace);
                    if (n != null)
                    {
                        if ((!paramType.IsGenericType) && (n.hasGenericVersion(paramType)))
                            s += "!()";
                    }
                }
                if (pi == pars.Last())
                {
                    if (pi.GetCustomAttributes(typeof(System.ParamArrayAttribute), false).Length > 0)
                    {
                        if (paramType.Name != "Object")
                            fileData.Write(s); fileData.Write(" ");
                        fileData.Write("...");
                    }
                    else
                    {
                        fileData.Write(s);
                    }
                }
                else
                {
                    fileData.Write(s);
                    fileData.Write(", ");
                }
            }
            fileData.Write(")");
        }
        bool hasGenericOverload(MethodInfo mth)
        {
            MethodInfo[] methods = mth.DeclaringType.GetMethods(Generator.memberBindingFlags());
            foreach (MethodInfo method in methods)
            {
                if ((method.Name == mth.Name) && (method.GetGenericArguments().Length != 0))
                    return true;
            }
            return false;
        }
        void expandTemplate(MethodInfo mth)
        {
            int lastDot = mth.Name.LastIndexOf('.');
            if(lastDot>=0)
                fileData.Write(mth.Name.Substring(lastDot+1));
            else
                fileData.Write(mth.Name);

            Type[] gens = mth.GetGenericArguments();
            if (gens.Length == 0)
            {
                /* Templated and non templated version of a D method
                 * cannot reside in the same class so if 1 method is templated so must all others
                 * with the same name. */
                if (hasGenericOverload(mth))
                    fileData.Write("()");
            }
            else
            {
                fileData.Write("(");
                foreach (Type gen in gens)
                {
                    fileData.Write(Generator.trimTemplate(Generator.nameConvert(gen, false, false, false, name)));
                    if(gen != gens.Last())
                        fileData.Write(", ");
                }
                fileData.Write(")");
            }
        }
        void methodPrefix(MethodBase mb)
        {
            if (mb.IsPublic)
                fileData.Write("public ");
            else
                fileData.Write("protected ");
            if (mb.IsStatic)
                fileData.Write("static ");
        }

        public void makeFiles(object o)
        {
            object[] oa = (object[])o;
            makeFiles((string)oa[0], (bool)oa[1], (bool)oa[2]);
        }

        public void makeFiles(string importDir, bool doPragmas, bool doOverride)
        {
            Console.WriteLine(name);

            string[] ss = name.Split('.');
            string fileName = importDir;
            for (int i = 0; i < ss.Length-1; i++)
            {
                fileName = Path.Combine(fileName,ss[i]);
            }

            DirectoryInfo di = new DirectoryInfo(fileName);
            if (!di.Exists)
                di.Create();

            fileName = Path.Combine(fileName, ss.Last());

            fileData = new StreamWriter(new FileStream(fileName + ".di", FileMode.Create));


            fileData.WriteLine("/******************************************************************************"); fileData.WriteLine();
            fileData.WriteLine("    Dnet automatically generated import file"); fileData.WriteLine();
            fileData.WriteLine("*******************************************************************************/"); fileData.WriteLine();
            fileData.WriteLine("module " + name + ";");


            foreach (DImport import in imports)
            {
                fileData.WriteLine(); fileData.Write(import);
            }

            string lastAssembly = null;
            string thisAssembly = null;

            reOrderTypes(ref types);

            foreach (Type type in types)
            {
                if (type.DeclaringType != null)
                    continue;
                if (type.IsEnum || type.IsSubclassOf(typeof(MulticastDelegate)))
                {
                }
                else if (Generator.isPrimitive(type))
                    continue;
                else if (type.IsClass || type.IsInterface || (type.IsValueType && !type.IsPrimitive))
                {
                }
                else
                    continue;
                thisAssembly = type.Assembly.GetName().FullName;
                if (thisAssembly != lastAssembly)
                {
                    if (doPragmas)
                    {
                        if (lastAssembly != null)
                            fileData.Write("}");
                        fileData.WriteLine();
                        fileData.WriteLine();
                        fileData.Write("pragma(assembly, \"");
                        fileData.Write(thisAssembly);
                        fileData.WriteLine("\")");
                        fileData.WriteLine("{");
                    }
                    else
                    {
                        fileData.WriteLine(); fileData.WriteLine();
                    }
                }
                doType(type, doPragmas == true ? 1 : 0, null, false, doOverride);
                lastAssembly = thisAssembly;
            }

            fileData.WriteLine();
            if (doPragmas)
                fileData.WriteLine("}"); 
            else
                fileData.WriteLine();
            fileData.WriteLine();

            fileData.Close();
        }

        /* Gets called for each type by the owning namespace.
         Checks if the type is either an enum, delegate, primitive, interface, class or struct.
         It then creates the D equivalent declaration for it*/
        void doType(Type type, int indentCount, Type owner, bool isNested, bool doOverride)
        {
            if (type.DeclaringType != owner)
                return; //start from base types and recurse down nested types


            /* Check for duplicate type names. The first gets output like normal. The remainder are commented out */
            bool commentOut = false;
            int typeDupIndex = types.FindIndex(delegate(Type t) { return (t.Name == type.Name) && (t != type) && (t.DeclaringType==type.DeclaringType); });
            if ((typeDupIndex >= 0) && (typeDupIndex < types.IndexOf(type)))
            {
                fileData.Write("/+");
                commentOut = true;
            }

            if (type.IsEnum)
            {
                addIndent(indentCount); fileData.Write("enum "); fileData.Write(Generator.nameConvert(type, false, false, true, name));
                fileData.WriteLine(" : long"); addIndent(indentCount); fileData.Write("{");
                FieldInfo[] fields = type.GetFields();
                bool first = true;
                foreach (FieldInfo field in fields)
                {
                    if (field.FieldType == type)
                    {
                        if (first)
                            first = false;
                        else
                            fileData.Write(",");
                        fileData.WriteLine(); addIndent(indentCount + 1); fileData.Write(field.Name); fileData.Write(" = "); fileData.Write(field.GetRawConstantValue().ToString());
                    }
                }
                fileData.WriteLine(); addIndent(indentCount); fileData.WriteLine("}");
            }
            else if (type.IsSubclassOf(typeof(MulticastDelegate)))
            {
                if (hasGenericVersion(type))
                {

                    addIndent(indentCount); fileData.Write("template "); fileData.Write(Generator.nameConvert(type, true, false, true, name).Replace("!", ""));
                    if (!type.IsGenericTypeDefinition)
                        fileData.Write("()");
                    fileData.WriteLine(); addIndent(indentCount); fileData.WriteLine("{"); addIndent(indentCount + 1);
                    fileData.Write("typedef "); fileData.Write(Generator.nameConvert(type.GetMethod("Invoke").ReturnType, true, true, false, name)); fileData.Write(" delegate ");
                    addParamString(type.GetMethod("Invoke")); fileData.Write(" "); fileData.Write(Generator.trimTemplate(type.Name)); fileData.WriteLine(";"); addIndent(indentCount); fileData.WriteLine("}");
                }
                else
                {
                    addIndent(indentCount); fileData.Write("typedef "); fileData.Write(Generator.nameConvert(type.GetMethod("Invoke").ReturnType, true, true, false, name));
                    fileData.Write(" delegate "); addParamString(type.GetMethod("Invoke")); fileData.Write(" "); fileData.Write(type.Name); fileData.WriteLine(";");
                }

            }
            else if (Generator.isPrimitive(type))
            {
                if (commentOut)
                    fileData.Write("+/");
                return; //dont define Object, Void, Int32 etc..
            }
            else
            {
                addIndent(indentCount);
                bool isStruct = false;

                if (type.IsInterface)
                    fileData.Write("interface ");
                else if (type.IsClass)
                {
                    if (type.IsAbstract)
                        fileData.Write("abstract ");
                    if (type.IsSealed)
                        fileData.Write("final ");
                    if (isNested)
                        fileData.Write("static ");
                    fileData.Write("class ");
                }
                else if ((type.IsValueType) && (!type.IsPrimitive))
                {
                    if (isNested)
                        fileData.Write("static ");
                    fileData.Write("struct ");
                    isStruct = true;
                }
                else
                {
                    if (commentOut)
                        fileData.Write("+/");
                    return;
                }
                fileData.Write(Generator.nameConvert(type, true, false, true, name).Replace("!", ""));
                if ((!type.IsGenericTypeDefinition) && (hasGenericVersion(type)))
                    fileData.Write("()");
                bool inherited = false;
                if ((type.BaseType != null) && (type.BaseType != typeof(Object)) && (type.BaseType.IsVisible) && (!isStruct))
                {
                    fileData.Write(" : "); fileData.Write(Generator.nameConvert(type.BaseType, true, true, false, name));
                    if ((!type.BaseType.IsGenericType) && hasGenericVersion(type.BaseType))
                        fileData.Write("!()");
                    inherited = true;
                }
                if (!isStruct)
                {
                    Type[] interfaces = type.GetInterfaces();
                    foreach (Type iface in interfaces)
                    {
                        if (!iface.IsVisible)
                            continue;
                        if (!inherited)
                            fileData.Write(" : ");
                        else
                            fileData.Write(", ");
                        fileData.Write(Generator.nameConvert(iface, true, true, false, name));
                        if ((!iface.IsGenericType) && hasGenericVersion(iface))
                            fileData.Write("!()");
                        inherited = true;
                    }
                }
                fileData.WriteLine(); addIndent(indentCount); fileData.WriteLine("{");
                MemberInfo[] members = type.GetMembers(Generator.memberBindingFlags());
                List<Type> nestedTypes = new List<Type>();
                List<MethodBase> methods = new List<MethodBase>();
                foreach(MemberInfo member in members)
                {
                    if ((member is MethodBase) && (!((MethodBase)member).IsPrivate) && (!((MethodBase)member).IsAssembly))
                        methods.Add((MethodBase)member);
                    else if (member.MemberType == MemberTypes.Event)
                    {
                        EventInfo ei = (EventInfo)member;
                        MethodBase mb = ei.GetAddMethod(true);
                        if(!methods.Contains(mb))
                            methods.Add(mb);
                        mb = ei.GetRemoveMethod(true);
                        if (!methods.Contains(mb))
                            methods.Add(mb);                        
                    }
                    else
                    {
                        Type nestedType = types.Find(delegate(Type t) { return ((t.Name == member.Name) && (t.DeclaringType == type)); });
                        if (nestedType != null)
                            nestedTypes.Add(nestedType);
                    }
                }
                reOrderTypes(ref nestedTypes);
                foreach (Type nestedType in nestedTypes)
                {
                    fileData.WriteLine();
                    doType(nestedType, indentCount + 1, type, true, doOverride);
                    fileData.WriteLine();
                }
                foreach (MethodBase mb in methods)
                {
                    try{ mb.GetParameters(); } catch(FileNotFoundException) { continue; }

                    addIndent(indentCount + 1);

                    /* visual c++. no longer necessary since not outputing private methods */
                    if (mb.Name[0] == '~')
                        fileData.Write("//");

                    if (mb.MemberType == MemberTypes.Constructor)
                    {
                        methodPrefix(mb);
                        fileData.Write("this"); addParamString(mb); fileData.WriteLine(";");
                    }
                    else if (mb.Name == "{dtor}")
                    {
                        methodPrefix(mb);
                        fileData.WriteLine("~this();");
                    }
                    else
                    {
                        MethodInfo mi = (MethodInfo)mb;
                        Type retType = null;
                        try
                        {
                            retType = mi.ReturnType;
                            if (!retType.IsVisible) //method returns an internal type
                                fileData.Write("//");
                        }
                        catch (FileNotFoundException) { fileData.WriteLine(); continue; }
                        methodPrefix(mb);
                        if (doOverride
                            && ((mi.Attributes & MethodAttributes.VtableLayoutMask) != MethodAttributes.NewSlot)
                            && (mi.GetBaseDefinition() != null) && (mi.GetBaseDefinition() != mi)
                            && (mi.GetBaseDefinition().DeclaringType != typeof(Object))
                            && (mi.GetBaseDefinition().DeclaringType.IsVisible)
                            && (!mi.GetBaseDefinition().DeclaringType.IsAbstract))
                                fileData.Write("override ");
                        /*if ((!Generator.isPrimitive(retType)) && (!Generator.isGenPar(retType))) //alternate fix for http://d.puremagic.com/issues/show_bug.cgi?id=3231
                            fileData.Write(".");*/
                        fileData.Write(Generator.nameConvert(retType, true, true, false, name));
                        if ((!retType.IsGenericType) && hasGenericVersion(retType))
                            fileData.Write("!()");
                        fileData.Write(" ");
                        expandTemplate(mi); addParamString(mb); fileData.WriteLine(";");
                    }
                }
                addIndent(indentCount); fileData.WriteLine("}");
            }

            if (commentOut)
                fileData.Write("+/");
        }
    }
}
