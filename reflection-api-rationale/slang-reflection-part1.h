// slang-reflection.h

namespace slang { namespace reflection {

// This file shows a skeletal draft of a hypothetical reflection API
// for Slang that is first and foremost meant to illustrate the good
// parts of the design ideas behind the *current* reflection API
// (which are often obscured by cumbersome boilerplate and limitations
// in how we have done C/C++ API so far).
//
// This document will be slightly pseudo-code-y in places, and
// leave certain design decisions that are *not* central to
// reflection itself as hand-waves. In particular:
//
// * We pretend that there is a `Sequence<T>` type that we can
// use freely instead of the `getThingCount`/`getThing` pairs
// that the current API uses.
//
// * We make liberal use of a `class` hierarchy here, and do
// not go into detail of how to make casting/querying work.
// A real implementation would need to use some combination
// of `enum` tags, along with moving bits of API into
// base types that really ought to be in derived types (the
// currently reflection API does a lot of both).
//
// * For brevity we pretend that the default in C++ is
// `public` inheritance, so that we can more clearly use
// `class` for thing that are logically heap-allocated
// objects, and `struct` for things that are more often
// passed around by value.
//
// * We will also pretend that C++ has Slang-like
// `extension` declarations, so that we can present
// the members of a type in whatever order we like.
//
// Another important detail is that we will not cover
// all the possible opportunities for convenience
// methods (e.g., helper methods so that you can query
// the properties of a `Type` on a `TypeLayout`).
// Such conveniences are *vitally* important if we are
// to end up with a usable API, so their absence here
// should not be taken as an argument against them.
//
// Background: What information are we talking about?
// ==================================================
//
// If we look at the kinds of information in the `slangc`
// implementation that a user might want to perform
// reflection on, we see that it falls into a few broad
// categories.
//
// AST Level: `Decl`s
// ------------------
//
// First there is just the plain syntactic hierarchy
// of the AST for a given module. Most of this level
// consists of `Decl`s.
//
// `Entity` Level
// --------------
//
// This level needs a better name, but it's really hard
// to know what to refer to it as.
//
// The key thing that differentiates this level from the
// AST level is that this level can form references to
// distinct specializations of the same AST-level construct.
//
// So if we have:
//
//      struct Outer<T>
//      {
//          struct Inner { T someField; }
//      }
//
// then the types `Outer<int>.Inner` and `Outer<float>.Inner`
// are distinct entities, even if they both refer to the same
// AST-level declaration. This distinction matters a *lot*
// when somebody queries the type of `someField`; each of
// the two types will answer differently.
//
// Those with more intimate knowledge of the semantic-checking
// parts of the Slang compiler probably realize that this
// is the level at which `DeclRef`s and `Type`s operate.
//
// Linked Level
// ------------
//
// At some point a user pulls together a collection of
// different modules, entry points, etc. that they would
// like to compile, and produces a linked program.
//
// Within the current reflection API, this is the level
// that is dominated by `IComponentType`, which basically
// represents something that can be used as input to
// linking (and is also used to represent the output
// of linking).
//
// The main new construct that this level introduces
// is the notion of a *program*, which is a linked
// collection of zero or more modules, entry points,
// etc.
//
// Layout Level
// ------------
//
// The layout level stores layout information for how types,
// parameters, entry points, etc. have been laid out or had
// binding information applied to them.
//
// A single type may be laid out differently for different
// targets, and even for the *same* target in cases (e.g.,
// the difference between D3D constant buffer and structured
// buffer layout rules). As a consequence, it is important
// that, e.g., the `Type` and `TypeLayout` representations
// be kept distinct.
//
// Target-Specialized Level
// ------------------------
//
// The Slang compiler and runtime API supports multiple
// compilation targets being active in a single session.
//
// A linked program is still target-independent by default,
// and needs to be explicitly bound/specialized to a platform
// to produce a target-specific version of that program.
//
// Inside the implementation, we have classes like `Target`
// and `TargetProgram` that represent exactly these concepts,
// but the user-facing API currently doesn't reify them.
//
// The target-specialized level supports the extraction of
// compiled code from a program and its entry points, as well
// as querying for reflection information.
//
// Cross-Level References
// ----------------------
//
// A key point here is that the above levels have an implied
// ordering to them; objects at each level can typically
// refer to those from earlier levels, but not vice versa.
// The main exception is that `Decl`s at the AST level end
// up referring to things from the `Entity` level).
//
// Big-Picture: How many of those levels do we need?
// ==================================================
//
// This proposal attempts to avoid some of the proliferation
// that has occured in the Slang API, where the various
// levels above each have their own objects and representations.
//
// Here we will instead propose only *two* levels:
//
// * A per-`Session` level for AST/Entity/Linked information.
//
// * A per-`Target` level for layout and codegen.
//
// The `Entity` Level
// ==================
//
// Modules have (rightly) become central to how programmers
// interact with Slang, and so our breakdown starts with
// the `loadModule` operation:
//

class Session
{
    Module* loadModule(const char* name, IBlob** outDiagnostics);
};

//
// The `Module` type here corresponds to the `IModule` interface
// in the current API. Here we will give it a more simplified
// and streamlined definition:
//

class Module : Linkable
{
    // A module needs to have a way to look up the entry points
    // defined in that module (as per the current API):
    //
    Sequence<EntryPoint*> getEntryPoints();
    EntryPoint* findEntryPoint(char const* name);

    // For legacy/convenience reasons, we also need to be able
    // to take an already-loaded module and kick od semantic
    // checking of one of the functions in that module as an
    // entry point:
    //
    // TODO(Tess): put this on `Func`?
    //
    EntryPoint* findEntryPoint(
        const char* name,
        Stage       stage,
        IBlob**     outDiagnostics = nullptr);

    // This document is going to ignore the following functions
    // from the current `IModule` API, since they aren't relevant
    // to what reflection needs:
    //
    //      serialize
    //      writeToFile
    //      getFilePath
    //      getUniqueIdentity
    //      getDependencyFileCount
    //      getDependencyFilePath    
};

//
// Astute readers might have already guessed that the `Linkable`
// base class there corresponds to the current `IComponentType`
// interface.
//
// The most notable feature of a `Linkable` is that it can
// be linked to form a program:
//
class Linkable : public Entity
{
    Program* link(IBlob** outDiagnostics);

    // TODO:      renameEntryPoint()
};

//
// In addition, we need the ability to compose multiple
// linkables together to create a composite. Those who
// have experience with Unix-y linking might think of
// `Linkable` as an archive file while `Program` is a
// binary.
//

extension Session
{
    Linkable* compose(
        Count               componentCount,
        Linkable* const*    components);    
};

//
// So far this is just regurgitating bits of the existing
// API with new names, but the first interesting difference
// here is that `Linkable` inherits from this new `Entity`
// class which (along with needing a better name) is the
// root of the hierarchy of target-independent reflection
// classes.
//
// As a starting point, the `Entity` class supports most
// of the functionality of the recently-added `DeclReflection`
// type in the current implementation:
//

class Entity
{
    // Gets a "reasonable" name for this entity where possible,
    // which is suitable for displaying to a programmer
    // navigating the reflection info.
    //
    const char* getName();

    // Gets the leaf-most name of this entity. E.g., for a `struct`
    // type this is the name on the `struct` declaration, ignoring
    // all of its surrounding context.
    //
    const char* getSimpleName();

    // Gets a fully-qualified name for this entity, including the
    // module name, and with full-qualified names for any types
    // that it refers to in, e.g., generic arguments.
    //
    // This may or may not be the same as `getName()`
    //
    const char* getFullyQualifiedName();

    // Gets the parent entity, if any. If this entity logically corresponds
    // to a declaration, that will be the outer declaration.
    //
    Entity* getParent();

    // Gets the children of this entity. If this entity corresponds to
    // a declaration, that will be the child declarations.
    //
    Sequence<Entity*> getChildren();
    Entity* findChild(const char* name);

    // Look up modifiers or user attributes on this entity.
    //
    Modifier* findModifier(Modifier::Tag tag);
};

//
// On the implementation side, note that an `Entity` will often
// correspond to a `DeclRef` or a `Type`. The user-facing
// reflection API will *not* draw a distinction between a `struct`
// declaration and a `struct` type, since most users will not
// be prepared to grasp the subtleties involved.
//
// When querying the children of an `Entity` via `getChildren()`
// or `findChild()`, the returned entity will always include
// the qualification from the parent `Entity` if it was a `DeclRef`,
// or a `DeclRefType`.
//
// A lot of the cases currently under `DeclReflection` fall
// naturally into the hierarchy under `Entity`.
//
// Note that `Module` above already serves the role of both `IModule`
// and a `DeclReflection` for the module.
//

class Func : public Entity
{
    Sequence<Var*> getParams();

    Type* getResultType();
};

//
// The current API's `TypeReflection` includes the queries that
// are specific to all of its logical subtypes, leading to
// it appearing more cluttered than it really needs to be:
//

class Type : public Entity
{};

//
// Here we instead break those APIs out into the more refined
// hierarchy that is implied:
//

/// Aggregate Types
class AggType : Type
{
    // The "fields" of an aggregate type are its non-`static`
    // member variables.
    //
    Sequence<Var*> getFields();
    Index findFieldIndexByName(char const* name);
};

class StructType : AggType {};
class ClassType : AggType {};
class InterfaceType : AggType {};

class EnumType : Type {};
class ConjunctionType : Type {};
class ScalarType : Type {};

class ArrayType : public Type
{
    Type* getElementType();
};

class SizedArrayType : public ArrayType
{
    Count getElementCount();
};

class UnsizedArrayType : public ArrayType
{
};

Type* unwrapArray(Type* type);
Size getTotalArrayElementCount(Type* type);

class VectorType : public Type
{
    ScalarType* getElementType();
    Count getElementCount();
};

class MatrixType : public Type
{
    ScalarType* getElementType();
    Count getRowCount();
    Count getColumnCount();
};

class ResourceType : public Type
{
    Type* getResultType();
    ResourceShape getShape();
    ResourceAccess getAccess();
};

//
// Because generics can have both type and value
// parameters, it is useful to have a base class
// in the reflection hierarchy that can cover
// both cases:
//

class VarBase : public Entity
{};

class Var : public VarBase
{
    Type* getType();
};

class TypeVar : public VarBase
{};

//
// The actual name to use for `VarBase` should be
// discussed in detail, but I hope its purpose here
// is clear.
//
// With the somewhat distateful `VarBase` out of the way,
// we can expose generics quite simply:
//

class Generic : public Entity
{
    Sequence<VarBase*> getParams();

    //
    // Given *any* generic (whether a function, type, etc.)
    // we can specialize it to a sequence of arguments.
    //
    Entity* specialize(
        Count           argCount,
        Entity* const*  args,
        ISlangBlob**    outDiagnostics);

    //
    // In cases where a user wants to perform reflection
    // on the members of a generic *without* first
    // specializing it, we need a query to return the
    // inner declaration directly (rather than via
    // `specialize`).
    //
    Entity* getUnspecializedInnerEntity();
};

//
// Aside: There is a *lot* of possible design space here
// for how generic-ness is exposed to users. The compiler
// implementation already runs into many cases where it is
// tedious to, e.g., enumerate both the child `FuncDecl`s
// of a type *and* any child `GenericDecl`s that wrap `FuncDecl`s.
//
// All the same, it might be *too* easy for a user of the
// reflection API to neglect to specialize types when doing
// reflection, and thus end up querying types for fields, etc.
// that are not actually usable.
//
// This is one area where I do not pretend to know the
// Right Answer at all.
//
// Further, when an `Entity` is itself a result of specialization
// (that is, it is a `SpecializedDeclRef`), we need a way
// to query for the arguments that were used to create that
// specialization. We want something akin to the following,
// even if that won't actually work as given:
//
class SpecializedEntity : public Entity
{
    Generic*            getSpecializedGeneric();
    Sequence<Value*>    getSpecializationArgs();
};

//
// In order to be able to invoke `Generic::specialize` when
// there are value parameters, we need a way to construct
// and represent `Entity`s that are plain values:
//

// An entity that represents a *value* rather
// than a *type*, declaration, etc.
//
class Value : public Entity
{
    Type* getType();
};

class ConstantValue : public Value
{};

class IntConstant : public ConstantValue
{
    Int64 getValue();
};

class FloatConstant : public ConstantValue
{
    Float64 getValue();
};

class StringConstant : public ConstantValue
{
    char const* getValue(Size* outSize);
};

extension Session
{
    IntConstant* getIntConstant(Type* type, Int64 value);
    FloatConstant* getFloatConstant(Type* type, Float64 value);
    StringConstant* getStringConstant(char const* text);
}

//
// With that `Value` hierarchy established, we can
// then expose user attributes:
// 
class Attribute
{
    char const* getName();

    Sequence<Value*> getArgs();
};

class UserAttribute : public Attribute
{};

extension Entity
{
    Sequence<UserAttribute> getUserAttributes();
}

//
// In order for users to be able to query the constraints
// on generic parameters, we need a representation of
// constraints (aka `ConstraintDecl` in the implementation):
//

class Constraint : public Entity
{};

class TypeConstraint : public Constraint
{};

class TypeConformanceConstraint : public TypeConstraint
{
    Type* getSubType();
    Type* getSuperType();
};

//
// The `IComponent` system in the current API also has
// a means to represent type conformances that should
// be explicitly linked into a program in order to
// facilitate dynamic-dispatch code generation.
// These simply need to be `Linkable`s:
//

class TypeConformanceWitness : public Linkable
{};

//
// Entry points are also linkable.
//
// Conceptually we could try to have `EntryPoint`
// inherit from `Func` so that you can directly
// pass an `EntryPoint` wherever a `Func` is
// expected, but doing so would seem to require
// multiple inheritance (unless one were to move
// the behavior of `Linkable` up into `Entity`,
// which seems ill-advised).
//

class EntryPoint : public Linkable
{
    /** Get the function that the entry point is based on.
    */
    Func* getFunc();

    const char* getNameOverride();

    Stage getStage();

    Var* getResultVar();

    // Not covered here:
    //
    //      getComputeThreadGroupSize
    //      getComputeWaveSize
    //      usesAnySampleRateInput
};

//
// The final piece of the `Entity`-level reflection API
// is the `Program` type, which results from calling
// `Linkable::link`:
//

class Program
{
    Sequence<EntryPoint*> getEntryPoints();
    EntryPoint* findEntryPoint(const char* name);

    Entity* findEntity(const char* name);
};

//
// Note that `Program` is *not* a subtype of `Entity`,
// since a program does not relate directly to any
// AST-level construct.
//
// Also note that the entry points enumerated on a
// `Program` are only those that were explicitly
// linked into it as part of composition. In particular,
// just because some module `M` is linked into the program
// and defines an entry point `E`, that does *not* mean
// that `E` will show up in the entry-point list for
// the resulting program.
//
// Finally, the `findEntity` operation on `Program`
// will lookup/parse the given `name` in a context where
// all the modules explicitly linked into the program
// are visible.
//
// (One complication I've just glossed over is that
// the `Linkable` that results from composition would
// not currently be something that exists within the
// AST hierarchy, and so it creates the possibility
// of `Entity`s needing to refer to implementation-side
// objects from disjoint class hierarchies...)
//


//
// Layout Hierarchy
// ================
//
// The layout hierarchy is where things start to get
// more interesting and challenging.
//
// First, we will start with the parts of this hierarchy
// that can be relatively straightforward: the ones that
// have little to nothing to do with layout:
//

class TypeLayout
{
    Type* getType();
};

class VarLayout
{
    Var* getVar();
    TypeLayout* getTypeLayout();
};

//
// Layout objects point back to their `Entity`-level versions,
// and variable layouts know their type layout.
//
// With the core `TypeLayout` and `VarLayout` pieces in place,
// we can more easily define the rest of the hierarchy for
// type layouts:
//

class StructTypeLayout : public TypeLayout
{
    StructType*             getType();

    Sequence<VarLayout*>    getFields();
};

class MatrixTypeLayout : public TypeLayout
{
    MatrixType*         getType();

    TypeLayout*         getElementTypeLayout();
    MatrixLayoutMode    getMatrixLayoutMode();
};

class ArrayTypeLayout : public TypeLayout
{
    ArrayType*          getType();

    TypeLayout*         getElementTypeLayout();
};

//
// Just as we use subclasses for the different cases
// of `TypeLayout`, here we will also use a subclass
// to factor out the parts of the `VarLayout` API that
// are really only applicable to varying input/output:
//

enum class SemanticKind
{
    None,
    User,

    SV_InstanceID,
    SV_DispatchThreadID,
    // ...
    // TODO: list all the `SV_*` and comparable semantics here
};

struct SemanticInfo
{
    SemanticKind    kind;
    char const*     name;
    Int             index;
};

class VaryingVarLayout : public VarLayout
{
    SemanticInfo getSemantic();
    Stage getStage();
};


//
// For now we will skip over the important case of
// layout reflection information for `ParameterBlock`
// and `ConstantBuffer` types, because they will
// be easier to understand once we actually get into
// the details of how layout information is represented.
//
// The remaining interesting cases that the layout
// hierarchy needs to support are programs and
// their entry points:
//

class ProgramLayout : VarLayout
{
    Program* getProgram();

    Sequence<VarLayout>* getParams();

    Sequence<EntryPointLayout*> getEntryPoints();
    EntryPointLayout* findEntryPoint(const char* name);

    // Find the layout for the given `entryPoint`, if it
    // is one of the entry points linked into `Program`.
    //
    EntryPointLayout* findEntryPoint(EntryPoint* entryPoint);
};

class EntryPointLayout : VarLayout
{
    EntryPoint* getEntryPoint();

    Sequence<VarLayout>* getParams();
    VarLayout* findParam(const char* name);

    VarLayout* getResultVarLayout();
};

// While both `ProgramLayout` and `EntryPointLayout` support
// direct access to their parameters (global parameters in
// the case of programs, and explicit entry-point parameters
// in the case of entry points), this is not the only
// way of accessing that information, nor even the best one.
//
// A `ProgramLayout` will itself be a `VarLayout`, describing
// the layout for a variable with either:
//
// * A fictious `struct` type with a field for each global-scope
// shader parameter in the program.
//
// * A `ConstantBuffer` of the above fictious `struct` type.
//
// In the latter case, the `VarLayout` encodes binding information
// for the "default global constant buffer" created for the program.
//
// The `VarLayout` for an `EntryPoint` is similar, being either:
//
// * A fictitious `struct` with a field for each explicit entry-point
// parameter (whether uniform or varying), and an additional field
// for the result of the entry point.
//
// * A `ConstantBuffer` of that `struct` type.
//
// Similar to the case with `ProgramLayout`, the latter case for
// `EntryPointLayout` indicates that the entry point needed an
// implicit constant buffer to be allocated for its parameters.


// Target-Specialized Programs
// ===========================
//
// The current Slang API does not reify the targets associated
// with a `Session`, instead only referring to them by index.
// For convenience, we propose to explicitly reify targets:
//
class Target
{
    TargetProgram* specializeProgram(
        Program*    program,
        IBlob**     outDiagnostics);

    EntityLayout* getEntityLayout(Entity* entity, LayoutRules rules = LayoutRules::Default);
};
//
// The two key operations that a target supports are specialization
// of a program to that target (yielding a target-specific program),
// and querying the layout that a given entity would have on that
// target.
//
// TODO: Is there ever a reason to query layout for something other
// than a type?
//
// A `TargetEntryPoint` is just an `EntryPointLayout` plus the ability
// to query the compiled kernel code for the given entry point:
//
class TargetEntryPoint : public EntryPointLayout
{
    Target* getTarget();
    SlangResult getCode(IBlob** outCode, IBlob** outDiagnostics = nullptr);

    // Also from IEntryPoint:
    //      getResultAsFileSystem
    //      getEntryPointHash
    //      getEntryPointHostCallable
};

// A `TargetProgram` is then just a `ProgramLayout` plus the ability
// to enumerate and look up the target-specialized entry points:
//
class TargetProgram : public ProgramLayout
{
    Target* getTarget();
    SlangResult getCode(IBlob** outCode, IBlob** outDiagnostics = nullptr);

    Sequence<TargetEntryPoint*> getEntryPoints();

    TargetEntryPoint* findEntryPoint(EntryPoint* entryPoint);
};

//
// We've covered a lot of API surface area and yet we haven't
// actually gotten to stuff like layout information, bindings,
// etc. The next document is where we start to step into that
// material.

#include "slang-reflection-part2.h"

}} // ::slang::reflection
