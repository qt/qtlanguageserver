// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant

/*!
\internal
\page lspgenerate.html

\title Generate Language Server Protocol

The language server protocol is largely generated from the
specification
  src/languageserver/3rdparty/specification.md
by the src/languageserver/generate.ts script.
Use
  npm install
  tsc --downlevelIteration --strictNullChecks --ignoreDeprecations 6.0 --noImplicitAny false generate.ts && node generate.js
to run it. Then git clang-format to reformat its output.
It generates:

src/languageserver/qlanguageserverspectypes_p.h:
  Types defined in the protocol are defined as C++ POD objects in the
  QLspSpecification namespace, that implement the walk template for
  serialization/deserialization, enmuerations are also defined, along
  with helpers to ensure their conversion to json is what defined in
  the specification (some enumeration should be stored as numbers,
  other as lowercase string, other as string equal to the enumeration
  constant).

src/languageserver/qlanguageserverspec_p.h
  Includes qlanguageserverspectypes_p.h, and defines:
  * the Capabilites one has to check/define, and aliases for the types
    defining them in the namespace ClientCapabilitiesInfo for the
    client ones, and in ServerCapabilitiesInfo for the server side.
  * in Requests the method and alias for the paramenters of the
    requests
  * in Notifications the same for notifications
  * in Responses, aliases for the types of the responses or partial
    responses
  * finally it defines RequestParams and NotificationsParams as
    variants able to keep any request or notification parameters

src/languageserver/qlanguageservergen_p.h
  Defines the ProtocolGen class that defines the typed LSP protocol:
  * requestXX or notifyXX methods to preform requests/send
    notifications
  * registerXXRequestHandler methods to handle requests
  * registerXXNotificationHandler methods to handle notifications

src/languageserver/qlanguageservergen_p_p.h
  Declaration of QLanguageServeGenPrivate.

src/languageserver/qlanguageservergen.cpp
  Implementation of QLanguageServeGen and QLanguageServeGenPrivate.

src/languageserver/qlspnotifysignals_p.h An object that emits signals
  for notifications, this way one can easily have multiple handlers
  for the same notification, by simply connecting the signal multiple
  times.

The goal is to keep the generated part simple, that is the reason when
possible setup/tweaks are done in other non generated files:
src/languageserver/qlanguageserverprespectypes_p.h:
  setup for qlanguageserverspectypes_p.h and qlanguageserverspec_p.h
src/languageserver/qlanguageserverbase_p.h and qlanguageserverbase.cpp:
  setup for qlanguageservergen* (base class)
src/languageserver/qlanguageserverprotocol_p.h, qlanguageserverprotocol_p_p.h and
    qlanguageserverprotocol.cpp:
  Define QLanguageServerProtocol the main "user facing" class built on
  the top of qlanguageservergen*
*/
import * as ts from "typescript";

var globalBaseClass = "";

function stringLiteral(text: string)
{
    return "QLatin1String(\"" + text + "\")";
}

const builtinTypes: { [key: string]: string } = {
    "string" : "QByteArray",
    "number" : "int",
    "decimal" : "double",
    "integer" : "int", // technically is qint32
    // uinteger is technically a qint32 that is in [0, 2^31-1]. Note that it can't hold numbers up to 2^32-1 like a quint32.
    "uinteger" : "int",
    "boolean" : "bool",
    "any" : "QJsonValue",
    "unknown" : "QJsonValue",
    "uri" : "QByteArray",
    "DocumentUri" : "QByteArray",
    "URI" : "QByteArray",
    "Array" : "QList",
    "null" : "std::nullptr_t",
    "object" : "QJsonObject",
  // Use QJson(Value|Object|Array) to represent LSP(Any|Object|Array). It
  // turned out that implementing our own versions of them comes with lots
  // of challenges: qt container types do not support the recursive
  // inheritance, which leads to weird compile errors like QTBUG-146007 or
  // QTBUG-146563. Also, to make a custom LSPAny class usable you have to
  // implement methods to get the underlying type (string, int, ...), which
  // is exactly what QJsonValue offers.
    "LSPAny" : "QJsonValue",
    "LSPObject" : "QJsonObject",
    "LSPArray" : "QJsonArray",
    "[number, number]" : "std::pair<int,int>",
    // For enumerations in typescript single numbers can be types,
    // namely a type containing exactly that number.
    // That allows to also get the exact bit mask, and generally have more control.
    // We just consider them ints.
    "0" : "int",
    "1" : "int",
    "2" : "int",
    "3" : "int",
    "4" : "int"
};

const specialStructs = {
    "Message" : null, // non templatized top level
    "RequestMessage" : null, // non templatized top level
    "ResponseMessage" : null, // non templatized top level
    "NotificationMessage" : null, // non templatized top level
    "SelectionRange" : null, // recursive reference
};

const specialAliases = {
    "MarkedString" : null, // already defined by hand
    "LSPAny" : null, // recursive reference
    "LSPObject" : null, // recursive reference
    "LSPArray" : null, // recursive reference
}

const specialEnums = {
    "ErrorCodes" : null
}

interface PatchedStructMembers {
    memberCodeByName: Map<string, string>, dependencies: string[],
}

const patchedStructMembers = new Map<string, PatchedStructMembers>([
    [
        "ProgressParams", {
            "memberCodeByName" : new Map<string, string>([ [
                "value",
                "\n    std::variant<WorkDoneProgressBegin, WorkDoneProgressReport, WorkDoneProgressEnd> %1 = {};"
            ] ]),
            "dependencies" :
                    [ "WorkDoneProgressBegin", "WorkDoneProgressReport", "WorkDoneProgressEnd" ]
        }
    ],
    // documentChanges has a weird type, so simplify it
    [
        "WorkspaceEdit", {
            "memberCodeByName" : new Map<string, string>([ [
                "documentChanges",
                `using DocumentChange = std::variant<TextDocumentEdit, CreateFile, RenameFile, DeleteFile>;
    std::optional<QList<DocumentChange>> %1 = {};`
            ] ]),
            "dependencies" : [ "TextDocumentEdit", "CreateFile", "RenameFile", "DeleteFile" ]
        }
    ],
]);

var postStruct: { [key: string]: string } = {
    "DocumentFilter" : "using DocumentSelector = QList<DocumentFilter>;\n\n",
    "Range" : `
class Q_LANGUAGESERVER_EXPORT SelectionRange
{
public:
    SelectionRange() = default;
    SelectionRange(const Range &r):
        range(r)
    {}
    SelectionRange(const SelectionRange &o):
        range(o.range)
    {
        if (o.parent)
            parent = std::make_unique<SelectionRange>(*o.parent);
    }
    SelectionRange &operator=(const SelectionRange &o) {
        range = o.range;
        if (o.parent)
            parent = std::make_unique<SelectionRange>(*o.parent);
        return *this;
    }
    SelectionRange(SelectionRange &&) noexcept = default;
    SelectionRange& operator=(SelectionRange &&) noexcept = default;

    Range range = {};
    std::unique_ptr<SelectionRange> parent;

    template <typename W> void walk(W &w) {
        field(w, "range", range);
        field(w, "parent", parent);
    }
};

class Q_LANGUAGESERVER_EXPORT RangePlaceHolder
{
public:
    Range range = {};
    QByteArray placeholder = {};

    template <typename W> void walk(W &w) {
        field(w, "range", range);
        field(w, "placeholder", placeholder);
    }
};

class Q_LANGUAGESERVER_EXPORT DefaultBehaviorStruct
{
public:
    bool defaultBehavior = {};

    template <typename W> void walk(W &w) {
        field(w, "defaultBehavior", defaultBehavior);
    }
};

`
};

function upperCase(type: string): string
{
    return type.substr(0, 1).toUpperCase() + type.substr(1);
}

function generateEnum(e: metaModel.Enumeration): string
{
    if (e.name in specialEnums)
        return "";

    let output: string = "enum class " + e.name + "\n{\n";
    output += e.values.map(function(entry: metaModel.EnumerationEntry) {
                          let value =
                                  (typeof entry.value == "number") ? (" = " + (+entry.value)) : "";
                          return "    " + upperCase(entry.name) + value;
                      })
                      .join(",\n");
    return output + "\n};\nQ_ENUM_NS(" + e.name + ")\n\n";
}

function generateStringAccessors(e: metaModel.Enumeration): string
{
    let output: string = "";
    const enumName = namify(e.name);
    output = "template<>\n";
    output += "inline QString enumToString<QLspSpecification::" + enumName
            + ">(QLspSpecification::" + enumName + " value)\n";
    output += "{\n";
    output += "    switch (value) {\n"
    output += e.values.map(function(member: metaModel.EnumerationEntry) {
                          return "    case QLspSpecification::" + enumName
                                  + "::" + namify(member.name) + ": return "
                                  + stringLiteral(<string>member.value) + ";\n";
                      })
                      .join("");
    output += "    default: return QString::number(int(value));\n";
    output += "    }\n"
    output += "}\n\n";
    output += "template<>\n";
    output += "inline QLspSpecification::" + enumName
            + " enumFromString<QLspSpecification::" + enumName + ">(const QString &string)\n";
    output += "{\n";
    output += e.values.map(function(member: metaModel.EnumerationEntry) {
                          return "    if (string.compare(" + stringLiteral(<string>member.value)
                                  + ", Qt::CaseInsensitive) == 0)\n" +
                                  "        return QLspSpecification::" + enumName
                                  + "::" + namify(member.name) + ";\n";
                      })
                      .join("    else ");
    output += "    return QLspSpecification::" + enumName + "{};\n";
    output += "}\n\n";
    return output;
}

function generateNumberAccessors(e: metaModel.Enumeration): string
{
    let output: string = "";
    output += "template<>\n";
    output += "inline QString enumToString<QLspSpecification::" + e.name
            + ">(QLspSpecification::" + e.name + " value)\n";
    output += "{\n";
    output += "    return enumToIntString<QLspSpecification::" + e.name + ">(value);";
    output += "}\n\n";

    return output;
}

let typeToCppCache: Map<metaModel.Type, string> = new Map();
// Returns the C++ type name of a metamodel type. Use literalObjectName to create literal objects on demand if needed.
// Transforms std::variant<T, null> into std::optional<T>.
function typeToCppType(type: metaModel.Type,
                       literalObjectName: ((s: metaModel.StructureLiteral) => string)|undefined):
        string
{
    const cachedResult = typeToCppCache.get(type);
    if (cachedResult)
        return cachedResult
        const result = typeToCppTypeImpl(type, literalObjectName);
    typeToCppCache.set(type, result);
    return result;
}

// squash variant of object literals that only differ in their optional properties
function squashType(types: metaModel.Type[]): metaModel.Type|undefined
{
    if (!types.every(x => x.kind == "literal"))
        return;
    const firstType = types[0];
    const otherTypes = types.slice(1);
    if (!otherTypes.every(x => x.value.properties.length == firstType.value.properties.length))
        return;

    if (!otherTypes.every(x => x.value.properties.every(
                                  (p, i) => p.name == firstType.value.properties[i].name)))
        return;

    let optionalProperties = new Set<String>();
    const collectOptionalProperties = (p: metaModel.Property) => {
        if (p.optional)
            optionalProperties.add(p.name);
    };
    types.forEach(t => t.value.properties.forEach(collectOptionalProperties));

    firstType.value.properties.forEach(p => {
        if (optionalProperties.has(p.name))
            p.optional = true;
    });
    return firstType;
}

function typeToCppTypeImpl(
        type: metaModel.Type,
        literalObjectName: ((s: metaModel.StructureLiteral) => string)|undefined): string
{
    if (!type)
        return "std::nullptr_t";
    switch (type.kind) {
    case "base":
        if (type.name in builtinTypes)
            return builtinTypes[type.name];
    case "reference":
        const name = (<metaModel.ReferenceType>type).name;
        if (name in builtinTypes)
            return builtinTypes[type.name];
        return name
    case "array":
        return `QList<${typeToCppType((<metaModel.ArrayType>type).element, literalObjectName)}>`;
    case "map":
        return `QMap<${typeToCppType((<metaModel.MapType>type).key, literalObjectName)}, ${
                typeToCppType((<metaModel.MapType>type).value, literalObjectName)}>`;
    case "and":
        throw new Error(
                "and types are not supported in the current LSP specification, if you see this error it means the specification has changed and the code needs to be updated");
    case "or":
        let orTypes = (<metaModel.OrType>type).items;

        // fold std::variant<T..., null> into std::optional<T...>
        let isOptional = false;
        const isNull = (t: metaModel.Type) =>
                t.kind == "base" && (<metaModel.BaseType>t).name == "null";
        if (orTypes.some(isNull)) {
            isOptional = true;
            orTypes = orTypes.filter(t => !isNull(t));
        }

        // squash variant of object literals that only differ in their optional properties
        const squashed = squashType(orTypes)
        if (squashed)
        {
            const result = typeToCppTypeImpl(squashed, literalObjectName);
            return isOptional ? `std::optional<${result}>` : result;
        }

        // don't create variants if only one option is left.
        if (orTypes.length == 1) {
            const result = typeToCppType(orTypes[0], literalObjectName);
            return isOptional ? `std::optional<${result}>` : result;
        }

        const result =
                `std::variant<${orTypes.map(x => typeToCppType(x, literalObjectName)).join(", ")}>`;
        return isOptional ? `std::optional<${result}>` : result;
    case "tuple":
        return `std::tuple<${
                (<metaModel.TupleType>type)
                        .items.map(x => typeToCppType(x, literalObjectName))
                        .join(", ")}>`;
    case "literal":
        if (literalObjectName)
            return literalObjectName((<metaModel.StructureLiteralType>type).value)
            return "QJsonObject";
    case "stringLiteral":
        return "QByteArray";
    case "integerLiteral":
        return "int";
    case "booleanLiteral":
        return "bool";
    }
}

// collect dependencies and add them to result. All strings added to result are typenames that have to be defined
// before type's C++ definition can be generated. Otherwise, the compiler will complain about unknown types that are
// actually defined later in the generated .cpp file.
function collectDependencies(type: metaModel.Type, result: string[]): void
{
    switch (type.kind) {
    case "base":
    case "stringLiteral":
    case "integerLiteral":
    case "booleanLiteral":
        return;
    case "reference":
        const name = (<metaModel.ReferenceType>type).name;
        if (name in builtinTypes)
            return;
        result.push((<metaModel.ReferenceType>type).name);
        return;
    case "array":
        return collectDependencies((<metaModel.ArrayType>type).element, result);
    case "map":
        collectDependencies((<metaModel.MapType>type).key, result);
        collectDependencies((<metaModel.MapType>type).value, result);
        return;
    case "and":
        throw new Error(
                "and types are not supported in the current LSP specification, if you see this error it means the specification has changed and the code needs to be updated");
    case "or":
    case "tuple":
        const orType = <metaModel.OrType|metaModel.TupleType>type;
        orType.items.forEach(x => collectDependencies(x, result));
        return;
    case "literal":
        const literalType = <metaModel.StructureLiteralType>type;
        type.value.properties.forEach(x => collectDependencies(x.type, result));
        return;
    }
}

// Generate the definition code in a struct so that the definitions can be sorted dependencies first.
interface GeneratedClassOrAlias
{
    name: string, code: string, dependencies: string[]
}
;

function generateProperty(property: metaModel.Property, patch: PatchedStructMembers|undefined,
                          literalObjectName: ((s: metaModel.StructureLiteral) => string)|undefined):
        string
{
    if (property.type.kind == "stringLiteral")
        return `\n    static constexpr QByteArrayView kind = "${property.type.value}";`;

    if (patch && patch.memberCodeByName.has(property.name)) {
        const rType = patch!.memberCodeByName.get(property.name)!;
        return "    " + rType.replace(/%1/, property.name) + "\n";
    }

    let type = typeToCppType(property.type, literalObjectName);
    // some types like WorkspaceFoldersInitializeParams can be empty in multiple ways - only wrap type into optional
    if (property.optional && !type.startsWith("std::optional<"))
        type = `std::optional<${type}>`;
    return `\n    ${type} ${property.name} = {};`
}

function collectDependenciesForStruct(struct: metaModel.Structure): string[]
{
    let result: string[] = [];
    if (struct.extends) {
        struct.extends.forEach(baseType => collectDependencies(baseType, result));
    }
    if (struct.mixins) {
        struct.mixins.forEach(baseType => collectDependencies(baseType, result));
    }

    // process properties
    struct.properties.forEach(property => collectDependencies(property.type, result));
    return result;
}

// generate the C++ code for a metamodel structure
function generateClass(struct: metaModel.Structure, indent: string,
                       seenStructures: Set<string> = new Set()): GeneratedClassOrAlias
{
    let result: GeneratedClassOrAlias = {
        name : struct.name,
        code : "",
        dependencies : collectDependenciesForStruct(struct),
    };

    seenStructures.add(struct.name);
    let output: string = indent + "class Q_LANGUAGESERVER_EXPORT " + struct.name;

    // process base types
    if (struct.extends) {
        output += " : " + struct.extends
                .map(baseType => "public " + (<metaModel.ReferenceType>baseType).name)
                .join(", ");
    }
    if (struct.mixins) {
        if (!struct.extends)
            output += " : ";
        else
            output += ", ";
        output +=
                struct.mixins.map(baseType => "public " + (<metaModel.ReferenceType>baseType).name)
                        .join(", ");
    }

    // process properties
    output += "\n";
    const innerIndent = indent + "    ";
    output += indent + "{\n" + indent + "public:";

    const patch = patchedStructMembers.get(struct.name);
    if (patch)
        result.dependencies.push(...patch!.dependencies);
    output += struct.properties.map(property => generateProperty(property, patch, undefined))
                      .join("");
    output += "\n";
    output += "\n";

    // generate walk() method
    const usesArgument = struct.properties.length != 0 || struct.mixins || struct.extends;
    output += `${innerIndent}template <typename W> void walk(W &${usesArgument ? "w" : ""}) {`;
    const methodIndent = innerIndent + "    ";
    if (struct.extends)
        output += struct.extends
                .map(baseType => `\n${methodIndent}${typeToCppType(baseType, undefined)}::walk(w);`)
                .join("");
    if (struct.mixins)
        output += struct.mixins
                          .map(baseType => `\n${methodIndent}${
                                       typeToCppType(baseType, undefined)}::walk(w);`)
                          .join("");
    struct.properties.forEach(function(member: metaModel.Property) {
        output += "\n";
        output += `${methodIndent}field(w, "${member.name}", ${member.name});`;
    });
    output += "\n";
    output += innerIndent + "}\n";
    output += indent + "};\n\n";

    let post = postStruct[struct.name];
    if (post)
        output += post

        result.code = output;
    return result;
}

interface GeneratedTypes
{
    typeDeclarations: string, enumStringConversions: string
}

// creates a map from metamodel type name to index, such that all dependencies of a type have a smaller index than the type itself. Used
// to make sure that all dependencies are generated before the type.
function calculateOrderingOfGeneratedClassesAndAliases(classesAndAliases: GeneratedClassOrAlias[],
                                                       enumByNames: { [key: string]: boolean }):
        { [key: string]: number }
{
    let result: { [key: string]: number } = { };
    const classesAndAliasesByName = Object.fromEntries(classesAndAliases.map(a => [a.name, a]));

    let breakCycles = new Set<string>();
    let counter = 0;
    const processDependency = function(classOrAlias: GeneratedClassOrAlias) {
        if (classOrAlias.name in result)
            return;
        if (breakCycles.has(classOrAlias.name))
            return;
        breakCycles.add(classOrAlias.name);
        classOrAlias.dependencies.forEach(function(dependency: string) {
            if (dependency in enumByNames || dependency in specialEnums
                || dependency in specialAliases || dependency in specialStructs) {
                return;
            }
            processDependency(classesAndAliasesByName[dependency]);
        });
        const currentValue = ++counter;
        result[classOrAlias.name] = currentValue;
        return currentValue;
    };
    classesAndAliases.forEach(processDependency);
    return result;
}

interface GenerateObjectLiteralHelper
{
    literalBaseName: string, counter: number|null, alreadyGeneratedLiterals: GeneratedClassOrAlias[]
}

// generate C++ structs for anonymous literal object types found in aliases (usually literal | literal ...)
// This generates the C++ defintion for PrepareRenameResultVariantN in:
// using PrepareRenameResult =
//         std::variant<Range,
//             PrepareRenameResultVariant1,
//             PrepareRenameResultVariant2>;
// for example.

function generateObjectLiteral(helper: GenerateObjectLiteralHelper,
                               literal: metaModel.StructureLiteral): string
{
    let literalName: string = helper.literalBaseName;
    if (helper.counter != null)
        literalName += ++helper.counter;

    let result: GeneratedClassOrAlias = { name : literalName, code : "", dependencies : [] };
    helper.alreadyGeneratedLiterals.push(result);

    result.code = "class Q_LANGUAGESERVER_EXPORT " + literalName + "\n{\npublic:";
    const innerIndent = "    ";

    result.code += literal.properties
                           .map(property => generateProperty(property, undefined,
                                                             x => generateObjectLiteral(helper, x)))
                           .join("");
    result.code += "\n";
    result.code += "\n";

    // generate walk() method
    result.code += `${innerIndent}template <typename W> void walk(W &w) {`;
    const methodIndent = innerIndent + "    ";
    if (literal.properties.length == 0) {
        result.code += "\n";
        result.code += `${methodIndent}Q_UNUSED(w);`;
    } else {
        literal.properties.forEach(function(member: metaModel.Property) {
            result.code += "\n";
            result.code += `${methodIndent}field(w, "${member.name}", ${member.name});`;
        });
    }
    result.code += "\n";
    result.code += innerIndent + "}\n";
    result.code += "};\n\n";
    collectDependencies({ kind : "literal", value : literal }, result.dependencies);
    return literalName;
}

// generate C++ code for a metamodel type alias, appends it to result
function generateAlias(alias: metaModel.TypeAlias, result: GeneratedClassOrAlias[])
{
    // generate normal structs for alias that are defined as literals
    if (alias.type.kind == "literal") {
        let helper: GenerateObjectLiteralHelper = {
            literalBaseName : alias.name,
            counter : null,
            alreadyGeneratedLiterals : result,
        };
        typeToCppType(alias.type, x => generateObjectLiteral(helper, x));
        return;
    }
    if (alias.type.kind == "or") {
        const squashed = squashType(alias.type.items);
        if (squashed) {
            let helper: GenerateObjectLiteralHelper = {
                literalBaseName : alias.name,
                counter : null,
                alreadyGeneratedLiterals : result,
            };
            typeToCppType(squashed, x => generateObjectLiteral(helper, x));
            return;
        }
    }

    let helper: GenerateObjectLiteralHelper = {
        literalBaseName : alias.name + "Variant",
        counter : 0,
        alreadyGeneratedLiterals : result,
    };
    let generatedAlias: GeneratedClassOrAlias = { name : alias.name, code : "", dependencies : [] };
    generatedAlias.code = "using " + alias.name + " = "
            + typeToCppType(alias.type, x => generateObjectLiteral(helper, x)) + ";\n";
    collectDependencies(alias.type, generatedAlias.dependencies);
    result.forEach(x => generatedAlias.dependencies.push(x.name));
    result.push(generatedAlias);
}
function stringCompare(a: string, b: string): number
{
    if (a == b)
        return 0;
    return a < b ? -1 : 1;
}

/** Generate code for all classes in a set of .ts files */
function generate(protoData: metaModel.MetaModel): GeneratedTypes
{
    let typeDeclarations: GeneratedClassOrAlias[] = [];
    let enumStringConversions: string = "";
    var generated: { [key: string]: boolean } = { };

    var doGenerateDeclarations = function(struct: metaModel.Structure) {
        if (generated[struct.name] !== undefined)
            return;

        if (struct.name in specialStructs)
            return;
        typeDeclarations.push(generateClass(struct, ""));
        generated[struct.name] = true;
    };
    var doGenerateAliases = function(alias: metaModel.TypeAlias) {
        if (alias.name in generated)
            return;
        if (alias.name in specialAliases)
            return;

        generateAlias(alias, typeDeclarations);
        generated[alias.name] = true;
    };

    protoData.typeAliases.forEach(doGenerateAliases);
    protoData.structures.forEach(doGenerateDeclarations);

    protoData.enumerations.sort((a, b) => stringCompare(a.name, b.name));
    protoData.enumerations.forEach(function(e: metaModel.Enumeration) {
        if (e.name in specialEnums)
            return;
        if (e.type.name == "string")
            enumStringConversions += generateStringAccessors(e);
        else
            enumStringConversions += generateNumberAccessors(e);
    });

    const enumByNames = Object.fromEntries(protoData.enumerations.map(x => [x.name, true]));
    typeDeclarations.sort((x, y) => stringCompare(x.name, y.name));
    const ordering = calculateOrderingOfGeneratedClassesAndAliases(typeDeclarations, enumByNames);
    typeDeclarations.sort((x, y) => ordering[x.name] - ordering[y.name]);

    let output = "";
    protoData.enumerations.forEach(function(e) { output += generateEnum(e); });

    typeDeclarations.forEach(x => output += x.code);

    return { typeDeclarations : output, enumStringConversions : enumStringConversions };
}

import * as metaModel from "./3rdparty/metaModel.js";
// some types in metaModel.json were patched by hand, see 3rdparty/metaModel.json.patch
const metaModelJson = ts.sys.readFile("./3rdparty/metaModel.json");
const protoData = JSON.parse(metaModelJson!) as metaModel.MetaModel;

let result: GeneratedTypes = generate(protoData);

let license = ts.sys.readFile("generate.ts")!.split("\n\n")[0];

ts.sys.writeFile("qlanguageserverspectypes_p.h", `${license}

// this file was generated by the generate.ts script

#ifndef QLANGUAGESERVERSPECTYPES_P_H
#define QLANGUAGESERVERSPECTYPES_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLanguageServer/qtlanguageserverglobal.h>
#include <QtLanguageServer/private/qlanguageserverprespectypes_p.h>
#include <QtJsonRpc/private/qtypedjson_p.h>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QMap>

#include <optional>
#include <variant>

QT_BEGIN_NAMESPACE

namespace QLspSpecification {
Q_NAMESPACE_EXPORT(Q_LANGUAGESERVER_EXPORT)

enum class TraceValue
{
    Off,
    Messages,
    Verbose
};
Q_ENUM_NS(TraceValue)

enum class ErrorCodes {
    // Defined by JSON RPC
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    jsonrpcReservedErrorRangeStart = -32099,
    /** @deprecated use jsonrpcReservedErrorRangeStart */
    serverErrorStart = jsonrpcReservedErrorRangeStart,

    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,

    jsonrpcReservedErrorRangeEnd = -32000,
    /** @deprecated use jsonrpcReservedErrorRangeEnd */
    serverErrorEnd = jsonrpcReservedErrorRangeEnd,

    lspReservedErrorRangeStart = -32899,

    ContentModified = -32801,
    RequestCancelled = -32800,

    lspReservedErrorRangeEnd = -32800
};
Q_ENUM_NS(ErrorCodes)

${result.typeDeclarations}
} // namespace QLspSpecification

namespace QTypedJson {

template<>
inline QString enumToString<QLspSpecification::TraceValue>(QLspSpecification::TraceValue value)
{
    switch (value) {
    case QLspSpecification::TraceValue::Off: return QLatin1String("off");
    case QLspSpecification::TraceValue::Messages: return QLatin1String("messages");
    case QLspSpecification::TraceValue::Verbose: return QLatin1String("verbose");
    }
    return QString();
}

template<>
inline QString enumToString<QLspSpecification::ErrorCodes>(QLspSpecification::ErrorCodes value)
{
    return enumToIntString<QLspSpecification::ErrorCodes>(value);
}

${result.enumStringConversions}
} // namespace QTypedJson
QT_END_NAMESPACE
#endif // QLANGUAGESERVERSPECTYPES_P_H
`);

function namify(str: string): string
{
    if (str.startsWith("textDocument/"))
        str = str.replace(/[^\/]*\//, "");
    return str.split(/[$./ _]+/).map(upperCase).join("");
}
// naming strategy:
// Use parameter type name of request or notification if it ends in "Params" to name the
// request or notification. Otherwise, remove "/" in the method name and capitalize.
// Examples:
// * "textDocument/codeAction" has parameter type CodeActionParams => return "CodeAction"
// * "codeAction/resolve" has parameter type CodeAction => return "CodeActionResolve"
function methodNameFrom(requestOrNotification: metaModel.Request|metaModel.Notification): string
{
    let methodName = namify(requestOrNotification.method);
    if (!requestOrNotification.params)
        return methodName;

    // note: in 3.17, no notification or request has more than one parameter type
    const type = <metaModel.Type>requestOrNotification.params;
    if (!type || type.kind != "reference")
        return methodName;
    if (!type.name.endsWith("Params"))
        return methodName
        return namify(type.name.slice(0, type.name.length - "Params".length));
}

function responseType(request: metaModel.Request): string
{
    if (request.partialResult) {
        return `LSPPartialResponse<${typeToCppType(request.result, undefined)}, ${
                typeToCppType(request.partialResult, undefined)}>`;
    }
    return `LSPResponse<${typeToCppType(request.result, undefined)}>`;
}
function createRequest(request: metaModel.Request): string
{
    const parameterType = <metaModel.Type>request.params;
    let name = methodNameFrom(request);
    const indent = "    ";

    let result = `\n${indent}constexpr auto ${name}Method = QLatin1String("${request.method}");`;
    result += `\n${indent}using ${name}ParamsType = ${typeToCppType(parameterType, undefined)};`;
    result += `\n${indent}using ${name}ResultType = ${typeToCppType(request.result, undefined)};`;
    if (request.partialResult) {
        result += `\n${indent}using ${name}PartialResultType = ${
                typeToCppType(request.partialResult, undefined)};
${indent}using ${name}ResponseType = ${responseType(request)};`;
    } else {
        result += `\n${indent}using ${name}ResponseType = ${responseType(request)};`;
    }
    return result;
}

function createNotification(notification: metaModel.Notification): string
{
    const name = methodNameFrom(notification);
    return `\n    constexpr auto ${name}Method = "${notification.method}";
    using ${name}ParamsType = ${typeToCppType(<metaModel.Type>notification.params, undefined)};`;
}

function createInGroups<Type extends metaModel.Notification>(
        requests: Type[], createRequestOrNotification: (a: Type) => string): string
{
    return createInGroupsImpl(false, requests, createRequestOrNotification);
}
function createInGroupsWithComments<Type extends metaModel.Notification>(
        requests: Type[], createRequestOrNotification: (a: Type) => string): string
{
    return createInGroupsImpl(true, requests, createRequestOrNotification);
}

function createInGroupsImpl<Type extends metaModel.Notification>(
        withComments: boolean, requests: Type[],
        createRequestOrNotification: (a: Type) => string): string
{

    requests.sort((a, b) => stringCompare(a.method, b.method));

    let namespace: string = "";
    let output: string = "";
    requests.forEach(request => {
        if (withComments) {
            let neededNamespace = request.method;
            if (neededNamespace != namespace) {
                if (namespace != "")
                    output += `\n`;
                output += `\n// C++ types for the LSP method "${neededNamespace}"`;
                namespace = neededNamespace;
            }
        }
        output += createRequestOrNotification(request);
    });
    if (namespace != "")
        output += `\n`
        return output;
}

function responseHandlerType(type: metaModel.Type): string
{
    return type.kind == "base" && type.name == "null"
            ? "std::function<void()>"
            : `std::function<void(const ${typeToCppType(type, undefined)} &)>`;
}

function createRequestDeclaration(request: metaModel.Request): string
{
    const name = methodNameFrom(request);
    const paramsType = typeToCppType(<metaModel.Type>request.params, undefined);
    return `\n    void request${name}(const ${paramsType}&, ${
            responseHandlerType(
                    request.result)} responseHandler, ResponseErrorHandler errorHandler = &ProtocolBase::defaultResponseErrorHandler);
    void register${name}RequestHandler(const std::function<void(const QByteArray &, const ${
            paramsType} &, ${responseType(request)} &&)> &handler);`;
}

function createRequestImplementation(request: metaModel.Request): string
{
    const name = methodNameFrom(request);
    const paramsType = typeToCppType(<metaModel.Type>request.params, undefined);
    return `

void ProtocolGen::request${name}(const ${paramsType} &params, ${
            responseHandlerType(request.result)} responseHandler, ResponseErrorHandler errorHandler)
{
    typedRpc()->sendRequest(QByteArray(Requests::${
            name}Method), [responseHandler = std::move(responseHandler), errorHandler = std::move(errorHandler)](const QJsonRpcProtocol::Response &response) {
        if (response.errorCode.isDouble())
            errorHandler(ResponseError{response.errorCode.toInt(), response.errorMessage.toUtf8(), response.data});
        else
            decodeAndCall<${
            typeToCppType(request.result,
                          undefined)}>(response.data, responseHandler, errorHandler);
    }, params);
}

void ProtocolGen::register${
            name}RequestHandler(const std::function<void(const QByteArray &, const ${
            paramsType} &, ${responseType(request)} &&)> &handler)
{
    typedRpc()->registerRequestHandler<
        QLspSpecification::Requests::${name}ParamsType,
        QLspSpecification::Responses::${
            name}ResponseType>(QByteArray(QLspSpecification::Requests::${name}Method), handler);
}`;
}

function createRegisterNotification(notification: metaModel.Notification): string
{
    const name = methodNameFrom(notification);
    return `
protocol->register${name}NotificationHandler(
    [this, protocol](const QByteArray &method, const QLspSpecification::Notifications::${
            name}ParamsType &params) {
        static const QMetaMethod notificationSignal = QMetaMethod::fromSignal(&QLspNotifySignals::received${
            name}Notification);
        if (isSignalConnected(notificationSignal))
            emit received${name}Notification(params);
        else
            protocol->handleUndispatchedNotification(method, params);
    });`;
}

function createSignalDeclaration(notification: metaModel.Notification): string
{
    const name = methodNameFrom(notification);
    return `void received${name}Notification(const QLspSpecification::Notifications::${
            name}ParamsType &);`;
}

function createSendNotificationDeclaration(notification: metaModel.Notification): string
{
    const name = methodNameFrom(notification);
    const paramsType = typeToCppType(<metaModel.Type>notification.params, undefined);
    let output = "";
    output += `\n    void register${
            name}NotificationHandler(const std::function<void(const QByteArray &, const ${
            paramsType} &)> &handler);`;
    output += `\n    void notify${name}(const ${paramsType} &params);`;
    return output;
}
function createNotificationImplementation(notification: metaModel.Notification): string
{
    const name = methodNameFrom(notification);
    const paramsType = typeToCppType(<metaModel.Type>notification.params, undefined);
    return `

void ProtocolGen::register${
            name}NotificationHandler(const std::function<void(const QByteArray &, const ${
            paramsType} &)> &handler)
{
    typedRpc()->registerNotificationHandler<QLspSpecification::Notifications::${name}ParamsType>(
    QByteArray(QLspSpecification::Notifications::${name}Method), handler);
}

void ProtocolGen::notify${name}(const ${paramsType} &params)
{
    typedRpc()->sendNotification(Notifications::${name}Method, params);
}`;
}

ts.sys.writeFile(
        "qlanguageserverspec_p.h",
        license
                + `

// this file was generated by the generate.ts script

#ifndef QLANGUAGESERVERSPEC_P_H
#define QLANGUAGESERVERSPEC_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLanguageServer/qtlanguageserverglobal.h>
#include <QtLanguageServer/private/qlanguageserverspectypes_p.h>

QT_BEGIN_NAMESPACE

namespace QLspSpecification {
namespace Requests {${createInGroupsWithComments(protoData.requests, createRequest)}
} // namespace Requests
// for compatibility reasons:
namespace Responses { using namespace Requests; }

namespace Notifications {${createInGroupsWithComments(protoData.notifications, createNotification)}
} // namespace Notifications

// Variant over all possible request parameters, required by the generic handlers.
// This variant is used like a generic argument type that can be constructed from
// any argument type... except when it contains duplicate, in that case the
// constructors are deleted. Therefore ensure that each variant type only occurs
// once in the variant.
using RequestParams = std::variant<${
                                [...new Set(
                                         protoData.requests
                                                 .map(r => typeToCppType(<metaModel.Type>r.params,
                                                                         undefined))
                                                 .sort())]
                                        .join(",\n    ")},
    QJsonValue>;

// Variant over all possible notification parameters, required by the generic handlers.
// This can't contain duplicates, see comment on RequestParams.
using NotificationParams = std::variant<${
                                [...new Set(
                                         protoData.notifications
                                                 .map(r => typeToCppType(<metaModel.Type>r.params,
                                                                         undefined))
                                                 .sort())]
                                        .join(",\n    ")}>;

} // namespace QLspSpecification

QT_END_NAMESPACE

#endif // QLANGUAGESERVERSPEC_P_H
`);

ts.sys.writeFile("qlanguageservergen_p.h", license + `

// this file was generated by the generate.ts script

#ifndef QLANGUAGESERVERGEN_P_H
#define QLANGUAGESERVERGEN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLanguageServer/qtlanguageserverglobal.h>
#include <QtLanguageServer/private/qlanguageserverspec_p.h>
#include <QtLanguageServer/private/qlanguageserverbase_p.h>

#include <memory>
#include <functional>

QT_BEGIN_NAMESPACE

namespace QLspSpecification {

class ProtocolGenPrivate;

class Q_LANGUAGESERVER_EXPORT ProtocolGen: public ProtocolBase
{
protected:
    ProtocolGen(std::unique_ptr<ProtocolGenPrivate> &&p);
public:
    ~ProtocolGen();

// Requests
${createInGroups(protoData.requests, createRequestDeclaration)}

// Notifications
${createInGroups(protoData.notifications, createSendNotificationDeclaration)}

private:
    Q_DISABLE_COPY(ProtocolGen)
    Q_DECLARE_PRIVATE(ProtocolGen)
};

} // namespace QLspSpecification

QT_END_NAMESPACE

#endif // QLANGUAGESERVER_P_H
`);

ts.sys.writeFile(
        "qlanguageservergen.cpp",
        license
                + `

// this file was generated by the generate.ts script

#include <QtLanguageServer/private/qlanguageservergen_p_p.h>

#include <QtCore/QScopeGuard>

QT_BEGIN_NAMESPACE

namespace QLspSpecification {

QByteArray ProtocolBase::requestMethodToBaseCppName(const QByteArray &method)
{
    static QHash<QByteArray,QByteArray> map({${
                        protoData.requests
                                .map(r => `\n       { QByteArray("${r.method}"), QByteArray("${
                                             methodNameFrom(r)}") }`)
                                .join(",")}
    });
    return map.value(method);
}

QByteArray ProtocolBase::notificationMethodToBaseCppName(const QByteArray &method)
{
    static QHash<QByteArray,QByteArray> map({${
                        protoData.notifications
                                .map(n => `\n        { QByteArray("${n.method}"), QByteArray("${
                                             methodNameFrom(n)}") }`)
                                .join(",")}
    });
    return map.value(method);
}

ProtocolGen::ProtocolGen(std::unique_ptr<ProtocolGenPrivate> &&p):
    ProtocolBase(std::move(p))
{
}

ProtocolGen::~ProtocolGen()
{}

// Requests
${createInGroups(protoData.requests, createRequestImplementation)}

// Notifications
${createInGroups(protoData.notifications, createNotificationImplementation)}

} // namespace QLspSpecification

QT_END_NAMESPACE
`);
ts.sys.writeFile("qlspnotifysignals_p.h", license + `

// this file was generated by the generate.ts script

#ifndef QLSPNOTIFYSIGNALS_P_H
#define QLSPNOTIFYSIGNALS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLanguageServer/private/qlanguageserverprotocol_p.h>

QT_BEGIN_NAMESPACE

class Q_LANGUAGESERVER_EXPORT QLspNotifySignals: public QObject
{
    Q_OBJECT
public:
    QLspNotifySignals(QObject *parent = nullptr) : QObject(parent) { }
    void registerHandlers(QLanguageServerProtocol *protocol);
signals:
    ${protoData.notifications.map(createSignalDeclaration).join("\n    ")}
};

QT_END_NAMESPACE

#endif // QLSPNOTIFYSIGNALS_P_H
`);

ts.sys.writeFile("qlspnotifysignals.cpp", license + `

// this file was generated by the generate.ts script

#include <QtLanguageServer/private/qlspnotifysignals_p.h>

QT_BEGIN_NAMESPACE

using namespace QLspSpecification;

void QLspNotifySignals::registerHandlers(QLanguageServerProtocol *protocol)
{
    ${protoData.notifications.map(createRegisterNotification).join("\n    ")}
}

QT_END_NAMESPACE
`);
