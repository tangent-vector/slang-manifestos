
// slang-reflection-part2.h

// Layout: The (First) Hard Part
// =============================
//
// So far we've left out the most important bits of the layout
// reflection information. We need to be able to do simple
// things like query the offset of a member in a constant buffer,
// or to get the `binding` and `set` used by a global-scope
// texture parameter.
//
// The most important thing to understand is that on most targets
// there are multiple distinct kinds of resources that shader
// parameters can consume, and that a single parameter can
// actually consume resources of more than one kind.
//
// As a simple example, consider this code:
//
// As a simple example:
//
//      struct Light
//      {
//          float3      intensity;
//          Texture2D   shadowMap;
//          float       radius;
//          Texture2D   cookieMap;
//      }
//
//      ConstantBuffer<Light> gLight;
//
// If we compile code like the above for a simple target
// like D3D11, then the `gLight` parameter consumes two
// different kinds of resources:
//
// * One `b` register, for the `gLight` constant buffer
// * Two `t` registers, for the `gLight.shadowMap` and `gLight.cookieMap` textures
//
// In turn, the `Light` type, when laid out for this use
// case, consumes two kinds of resources:
//
// * 16 bytes, for the `intensity` and `radius` fields
// * Two `t` register, for the `shadowMap` and `cookieMap` fields
//
// Furthermore, the fields of `Light` each have an offset
// when laid out in this fashion:
//
// * The `intensity` field has an offset of zero bytes
// * The `shadowMap` field has an offset of zero `t` registers
// * The `radius` field has an offset of 12 bytes
// * The `cookieMap` field has an offset of one `t` register
//
// Different target platforms not only have different rules for
// how things are laid out, but they may also introduce very
// different kinds of resources that a type or variable can
// consume.
//
// In order to tell all of these apart, the Slang reflection
// API includes an `enum` for each of the distinct resource
// kinds that it recognizes. The current user-facing API
// calls this `SlangParameterCategory`, but the name we have
// used in the compiler implementation has proven to be
// significantly better, if not perfect:
//
enum class LayoutResourceKind
{
    // Most targets have *some* notion of byte-based storage
    // for values of **ordinary** type:
    //
    Bytes,              // aka `SLANG_PARAMETER_CATEGORY_UNIFORM`
    //
    // For some targets this is the *only* kind of resource
    // that needs to be tracked for layout.

    // For D3D11/12 targets, we need to be able to count
    // resource usage in terms of `b`, `t`, `s`, and `u`
    // registers:
    //
    D3D_ConstantBuffer,     // `b` register
    D3D_ShaderResource,     // `t` register
    D3D_SamplerState,       // `s` register
    D3D_UnorderedAccess,    // `u` register

    // For Vulkan, we need to be able to count resource
    // usage in terms of `binding`s:
    //
    VK_Binding,             // for `layout(binding=...)`

    // Both D3D12 and Vulkan introduce a kind of hierarchical
    // or two-dimensional layout model. Instead of just having
    // a flat range of registers or `binding`s, the program
    // can have multiple distinct "register spaces" (D3D12)
    // or "descriptor sets" (Vulkan), each of which has its
    // own internal range of registers/`bindings`.
    //
    // While there are some detailed differences, the broad
    // strokes of these two constructs are similar enough
    // that we reflect them using a single resource kind:
    //
    RegisterSpace, // D3D12 `space`, Vulkan `set`

    // Vulkan supports specialization constants, which use
    // their own distinct kind of resource during layout,
    // separate from the `set` and `binding` stuff:
    //
    VK_SpecializationConstant, // for `layout(constant_id=...)`

    // Vulkan also allows a global `uniform` block to be
    // mapped as a push-constant buffer, which consumes
    // yet another kind of resource distinct from `binding`s:
    //
    VK_PushConstantBuffer,      // for `layout(push_constant)`
    //
    // Note that SPIR-V and Vulkan only support a single
    // `push_constant` buffer, so a program can only use
    // a single slot/unit of this resource kind before it
    // would be invalid. The Slang reflection API doesn't
    // need to treat the limit of one any different than,
    // e.g., the D3D11 limit of 256 `t` registers.

    // Both D3D and VK/GL have a notion of varying input
    // and output parameters mapping to a flat range of
    // indices, where each index can be used to pass up
    // to a `float4`-sized vector between stages, but
    // cannot be used for larger types like matrices
    // or `struct`s.
    //
    // Slang uses the same resource kinds for varying
    // input and output across targets:
    //
    VaryingInput,       // D3D 'v' register, VK `layout(index=...)`
    VaryingOutput,      // D3D 'o' register, VK `layout(index=...)`

    // Ray tracing introduces a few new parameter-passing
    // mechanisms, which from a layout standpoint act much
    // like the `VK_PushConstantBuffer` case above.
    // They are distinct bindable resources, but *within*
    // a slot of reach resource kind the layout is entirely
    // byte-based:
    //
    RT_RayPayload,
    RT_HitAttributes,
    RT_CallablePayload,
    RT_ShaderRecord,

    // There are still a bunch of other API-specific cases,
    // but at this point the gist of what this `enum` means
    // should be clear.
    //
    VK_SubpassInputAttachment,
    Metal_ArgumentBufferElement,
    Metal_Attribute,
    Metal_Payload,
    // ...

    // There are two more cases added to this `enum` that
    // exist to allow us to provide a simpler set of queries
    // for the application programmer to use in the 99%
    // case where a given type/variable only consumes *one*
    // kind of resource.
    //
    None,   // pseudo-kind for types/variables that consume nothing
    Mixed,  // pseudo-kind for types/variables that consume more than one kind of resource
};

//
// At the most basic, we need a way to query the size of
// a type for *each* resource kind it might consume and,
// similarly, to query the offset of a variable for each
// resource kind:
//

extension TypeLayout
{
    Size getSize(LayoutResourceKind kind);
}

extension VarLayout
{
    Offset getOffset(LayoutResourceKind kind);
}

//
// We also need to support queries for the *alignment*
// of a type: 
//
extension TypeLayout
{
    Int getAlignment(LayoutResourceKind kind);
}
//
// In practice, we expect the alignment to be `1` for
// all resource kinds other than `Bytes`, but we include
// the general query here for completeness.

// Unlike the layout rules used for C, it is *not* the case that
// the size of a type will always be a mutliple of its alignment,
// for many of our target platforms and their native layout rules.
//
// When storing values in an array, each element needs to start
// at an offset that is properly aligned for the element type.
// Thus the offset between consecutive array elements is equal
// to the size of the type rounded up to a multiple of its
// alignment, which we refer to as the *stride* of the type.
//
extension TypeLayout
{
    Size getStride(LayoutResourceKind kind);
}

// The above queries are nice and orthogonal, but given
// the sheer number of `LayoutResourceKind`s, no application
// programmer is going to want to just query them all.
//
// There needs to be a way for the application to ask
// what kinds of resources a type layout *actually* consumes:
//
extension TypeLayout
{
    Sequence<LayoutResourceKind> getConsumedResourceKinds();
}
extension VarLayout
{
    Sequence<LayoutResourceKind> getConsumedResourceKinds();
}
//
// In principle this simply returns a list of the resource kinds
// for which `getSize` would return a non-zero value.
// In practice, there can be subtleties, where the compiler
// implementation may take a type that consumes no resources
// (such as an empty `struct`) and report it as consuming
// zero units of the resource kind it *would* consume, if
// it had any data in it.

// Having a `getConsumedResourceKinds()` query makes it easier
// for an application to loop over all the resources consumed
// by a type or variable, but in some of the most common cases
// the application developer knows that they are in a context
// where a given variable or type should consume only a single
// kind of resource.
//
// For developer convenience, we provide a query to get the
// *single* resource type consumed by a type or variable:
//
extension TypeLayout
{
    LayoutResourceKind getConsumedResourceKind();
}
extension VarLayout
{
    LayoutResourceKind getConsumedResourceKind();
}
//
// The result of these functions conceptually depends on what
// `getConsumedResourceKinds()` would return:
//
// * For a single-element sequence, they return that single element.
// * For an empty sequence they return `LayoutResourceKind::None`
// * Otherwise, they return `LayoutResourceKind::Mixed`

// As a futher simplification, when the application knows it wants
// to query layout information for the `Bytes` resource kind,
// it can use functions that elide the resource kind:
//
extension TypeLayout
{
    Size getSize();
    Int getAlignment();
    Size getStride();
}
extension VarLayout
{
    Offset getOffset();
}
//
// This design choice makes it so that working with ordinary
// types (which only consume `Bytes`) is compact and natural.

// While there are many contexts where an application will
// only want to work with bytes, there are also many cases
// where an application knows that it is dealing with some
// kind of API-bindable resource, such as `register`s or
// `binding`s.
//
// As another convenience, when a variable consumes only
// a single kind of resource and that resource is something
// bindable, the following queries can be used:
//
extension VarLayout
{
    Index getBindingIndex();
    Index getBindingSpace();
};

//
// Accumulating Offsets
// --------------------
//
// Consider the following input program:
//
//      struct A { float x; }
//      struct B { float y; A a; }
//      struct C { float z; B b; }
//
//      ConstantBuffer<C> gBuffer;
//
// When laid out for use in `gBuffer`, the offset
// of the nested field `b.a.x` is 32 bytes (for
// traditional D3D11 constant buffer layout).
// However, the offset of the `x` field within
// the `A` type is obviously zero.
//
// In order to properly compute the offset for a
// leaf variable, an application is expected to
// accumulate offsets from the `VarLayout`s
// along the *chain* that leads to that variable.
//
// When only dealing with `Byte`s, this accumulation
// tends to be easy.
//
// When dealing with `binding`s and `set`s in the
// context of `ConstantBuffer`s and `ParameterBlock`s,
// the application developer needs to have a deeper
// understanding of the layout rules that Slang applies.
//
// For example, consider a more complicated case with
// a texture inside of the constant buffer:
//
//      struct A { float x; Texture2D t; float y; }
//      ConstantBuffer<A> gBuffer;
//
// The *type* layout for `A` in this case is relatively simple:
// it consumes 8 `Bytes`, as well as one `t` register
// (for D3D), or one `binding` (for Vulkan). The offsets
// of the fields are also simple:
//
// * `x` has offset of zero bytes
// * `t` has an offset of zero registers/`binding`s
// * `y` has an offset of 4 bytes
//
// The type layout for `gBuffer` is only a little more
// subtle. For D3D it consumes one `b` register (for the
// constant buffer) and one `t` register. For Vulkan it
// simply consumes two `binding`s.
//
// Suppose that on Vulkan, `gBuffer` gets bound to
// `set=0, binding=10`. How should an application then
// accumulate offsets to compute the `binding` for
// `gBuffer.t`?
//
// If the application simply adds the offset stored
// for field `t` to the offset for `gBuffer`, it will
// get the wrong answer: `binding=10`. The Slang layout
// rules in this case will assign the constant buffer
// itself to use `binding=10`, and give the nested `t`
// field `binding=11`.
//
// In order to enable applications to do this kind of
// accumulation correctly across platforms, the type
// layout for *parameter groups* (meaning `ConstantBuffer`s,
// `ParamterBlock`s and a few other cases) stores a
// variable layout rather than a type layout for the
// element type:
//
class ParameterGroupTypeLayout : public TypeLayout
{
    VarLayout*          getElementVarLayout();

    // In addition, there are subtle cases where
    // the layout information for the "container"
    // itself (the `ConstantBuffer` or `ParameterBlock`)
    // may be difficult for an application to intuit,
    // so the layout for a group *also* stores
    // a complete variable layout for the container
    // itself.
    //
    VarLayout*          getContainerVarLayout();
};

// TODO: when *not* to accumulate

// TODO: how to accumulate spaces?

//
// Examples / Recipes
// ==================
//
// In this section, we cover some small examples of how to use the
// Slang reflection API in simple application use cases.
//
// Some applications will want to traverse the full hierarchy of
// Slang reflection information, and will want to deal with all
// of the possible complications that can arise when shader code uses
// various high-level-language constructs. Others, though, only
// need to be able to handle simpler shaders that follow idiomatic
// approaches.
//
// Minimal D3D11-Style
// -------------------
//
// Suppose an application has shaders that are typical of D3D11-era
// HLSL:
//
//      // MyShaders.hlsl
//
//      cbuffer PerFrame
//      {
//          float3 sunLightDir;
//          float3 sunLightIntensity;
//          float4x4 view;
//          float4x4 proj;
//      }
//
//      Texture2D diffuseMap;
//      Texture2D specularMap;
//      SamplerState sampler;
//
//      // ...
//
// And suppose the user has compiled and linked this
// code to yield a `ProgramLayout`:
//
//      ProgramLayout* program = ...;
//
// Let's look at how this application could answer
// various questions it might have about layout.
//
// ### What register/binding did my resource use?
//
// First, the application needs to be able to query
// the `register` (for D3D) or `binding` (for VK)
// that a particular global-scope shader parameter
// is using:
//
//      VarLayout* diffuseMap = program->findParam("diffuseMap");
//      int register = diffuseMap->getBindingIndex();
//      int space = diffuseMap->getBindingSpace();
//
// ### What is the size of my constant buffer?
//
// In order to allocate a constant buffer for `PerFrame`
// the application needs to be able to query the buffer
// and get its size:
//
//      VarLayout* perFrameBuffer = program->findParam("PerFrame");
//      TypeLayout* perFrameType =
//          perFrameBuffer->getTypeLayout()->getElementType();
//      size_t bufferSize = perFrameType->getSize();
//
// The above code is ignoring some uses of casting that
// would be required to make this work, depending on how
// the reflection API chooses to expose the class hierarchy
// of reflection types.
//
// Note: we may want to introduce a convenience subtype of
// `VarLayout` specifically for constant buffers, to enable
// the common queries to be more compact:
//
//      ConstantBufferVarLayout* perFrameBufer =
//          program->findParam("PerFrame");
//
//      size_t bufferSize = perFrameBuffer->getBufferSize();
//      // ...
//
// ### What is the offset of my constant buffer member?
//
// In order to write data into a constant buffer, the application
// will often need to query the offset of a specific field:
//
//      VarLayout* sunLightIntensityVar =
//          perFrameType->findField("sunLightIntensity");
//      size_t sunLightIntensityOffset = sunLightIntensityVar->getOffset();
//
//
// Conclusion
// ==========
//
// With what has been covered so far, an application can
// now extract all of the binding and layout information
// from a compiled Slang program, for whatever platforms
// they are using.
//
// While the information that can be extracted from this
// API is *complete*, it is not necessarily in the right
// form to be immediately *actionable* in the most common
// use cases that an application will have.
//

#include "slang-reflection-part3.h"
