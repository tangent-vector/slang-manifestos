// slang-reflection-part3.h

// The Binding-Oriented View
// =========================
//
// The API that has been described so far is sufficient for looking
// up layout information on shader parameters, but it is actually
// very inconvenient to work with for applications that have a
// more advanced approach to shader parameter binding.
//
// Example Application Code
// ========================
//
// Consider an application that wants to make efficient use of
// D3D12/VK descriptor tables/sets as a way to bind their shader
// parameters. The Slang language design encourages such applications
// to encapsulate their shader parameters into `struct` types,
// and to use the `ParameterBlock` construct to declare the
// descriptor tables/sets that they use. E.g.:
//
//      struct MaterialParams
//      {
//          Texture2D diffuseMap;
//          Texture2D specularMap;
//      }
//
//      struct ModelParams
//      {
//          float4x4 modelMatrix;
//          MaterialParams material;
//      }
//
//      struct LightParams
//      {
//          float3 dir;
//          float3 intensity;
//          Texture2D shadowMap;
//      }
//
//      ParameterBlock<ModelParams> gModel;
//      ParameterBlock<LightParams> gLight;
//
// Such an application will typically have C/C++ types
// in the application or engine code that correspond to
// the `struct` types they declare in Slang shaders. E.g.:
//
//      class AppLight : public AppSceneNode
//      {
//          // ...
//          Point3 m_dir;
//          Color3 m_intensity;
//          AppTexture* m_shadowMap;
//      };
//
//      class AppMaterial
//      {
//          // ...
//          AppTexture* m_diffuseMap;
//          AppTexture* m_specularMap;
//      };
//
//      class AppModel : public AppSceneNode
//      {
//          AppMaterial* m_material;
//          Mat4x4 m_modelMatrix;
//      };
//
// We are using the prefix `App` here in an effort to
// distinguish C++ types in the hypothetical application
// codebase from C++ types provided by the Slang API.
//
// Operations the Application Needs to Perform
// ===========================================
//
// For simplicity, let us assume that this application
// *only* wants to deal with rendering using Vulkan.
//
// In order to efficiently bind shader parameter data
// for parameter blocks like `gModel` and `gLight`
// above, the application needs to be able to perform
// a few key operations:
//
// * The application needs to be able to fill in a
//   `VkDescriptorSetLayoutCreateInfo` to be able
//   to describe a descriptor set layout suitable
//   for passing all of the data that goes into
//   a `Model` or `Light`.
//
// * The application needs to allocate descriptor sets
//   based on that layout and, if necessary, allocate
//   a suitably-sized piece of buffer memory to hold
//   any "ordinary" data in the `Model` or `Light` type.
//   (Recall that a descriptor set can directly contain
//   descriptors for textures/buffers/samplers, but cannot
//   directly contain ordinary data like vectors or matrices).
//
// * The application needs to be able to fill in one or more
//   `VkWriteDescriptorSet`s, so that it can write descriptors from
//   the fields of an `AppModel` or `AppLight` to a descriptor
//   set allocated for a `Model` or `Light`.
//
// * The application also needs to be able to write bytes to
//   the buffer (if any) allocated for a parameter block to
//   store the fields of ordinary type at the right offsets.
//
// * Finally, the application needs to be able to query, for
//   a particular program, the correct `set` index for each
//   parameter block (e.g., `gModel` or `gLight`), as part
//   of filling in a `VkPipelineLayoutCreateInfo`.
//
// The reflection API entry points presented so far are only
// really suitable for implementing a handful of these operations:
//
// * Given a `TypeLayout` for a type like `Model`, calling
//   `typeLayout->getSize()` will tell us how big of a
//   constant buffer needs to be allocated for any ordinary-type
//   fields in that type (and will return zero if the type
//   has no ordinary-type fields).
//
// * When filling in such a constant buffer, calling `getOffset()`
//   on the `VarLayout*` for any fields of ordinary type will
//   yield the correct offset for writing that field into the
//   buffer.
//
// * When filling in a `VkPipelineLayoutCreateInfo` the existing
//   `getBindingSpace()` operation should yield the expected
//   result when applied to the `VarLayout` for a global shader
//   parameter with a `ParameterBlock` type, like `gModel` or `gLight`.
//
// For the remaining operations, the current API encodes all
// the information an application would need, but it does not
// encode it in a way that is *actionable*: that is, in a way
// that the application can directly use to pass to the chosen
// GPU API.
//
// The rest of this document describes the extensions to the
// Slang reflection API that are intended to support these
// scenarios.
//
// Descriptor Set Layouts
// ======================
//
// At it's most basic, the layout for a type can be decomposed into:
//
// * A contiguous range of zero or more bytes,
//   to hold its ordinary data.
//
// * Zero or more *ranges* of descriptors, that need to be
//   bound via descriptor tables/sets.
//
// The first can already be queried through the Slang API
// easily as `typeLayout->getSize()`, so is is the second
// that needs to be added to the API.
//
// One detail that is relevant here is that a single type
// in Slang might map to more than one descriptor set when
// it comes time to bind it. Without getting into the details
// on *why* that is, let us accept that a type needs to
// describe multiple descriptor sets, each of which might
// have its own ranges:
//
extension TypeLayout
{
    Sequence<DescriptorSetInfo> getDescriptorSets();
};

// Each descriptor set is basically just a sequence
// of descriptor ranges, but it also neds to record
// the `set` offset of that particular descriptor
// set, relative to whatever `set` an entire
// `ParameterBlock` gets bound to:
// 
struct DescriptorSetInfo
{
    Sequence<DescriptorRange> descriptorRanges;
    Count spaceOffset;
};

// The descriptor ranges are then the more
// interesting type, since they need to
// provide enough information for the application
// to fill in a coresponding `VkDescriptorSetLayoutBinding`
// (or the equivalent for any other API):
//
struct DescriptorRangeInfo
{
    // Each range can represent one or more
    // descriptors, so there needs to be a count.
    // This maps directly to
    // `VkDescriptorSetLayoutBinding::descriptorCount`:
    //
    Count               descriptorCount;

    // Each range corresponds to some starting
    // register or `binding` index, in an API-specific
    // fashion.
    //
    // For Vulkan, this field corresponds directly to:
    // `VkDescriptorSetLayoutBinding::binding`:
    //
    Count               indexOffset;

    // Finally, each range records the type of bindings/descriptors
    // that go into it:
    //
    BindingType         bindingType;
};

//
// Readers who have been following along all the way may wonder
// why that last field is not using the existing `LayoutResourceKind`
// type; after all, that type was intended to record the kinds
// or parameter-passing resource a given type/parameter/field uses.
//
// The challenge is that there are differences between how shader
// parameters are grouped and counted at the shader IL level,
// and at the API level.
//
// For example, SPIR-V treats both a `Texture2D` and a `SamplerState`
// as consuming the same kind of resource: each uses up one `binding`.
// When compiling to SPIR-V, both textures and samplers are  reflected
// as using `LayoutResourceKind::DescriptorTableSlot`.
//
// However, when filling in a `VkDescriptorSetLayoutBinding`, the
// fact that both the texture and sampler consume `binding`s for
// layout doesn't matter. The Vulkan API cares about the distinction
// between `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` and
// `VK_DESCRIPTOR_TYPE_SAMPLER`.
//
// Thus we need a *distinct* enumeration that covers the differences
// between types of bindings that a GPU API cares about, instead of
// those that a shader compilation target cares about:
//

    enum class BindingType
    {
        Unknown,
        Sampler,
        Texture,
        ConstantBuffer,
        // ...
    };

//
// Given the descriptor range information, an application should
// be able to easily allocate a `VkDescriptorSetLayout` to match
// some Slang `struct` type that they want to use with a
// `ParameterBlock`.
//

// Shader Cursors: Filling in Descriptor Sets
// ==========================================
//
// While there are multiple ways that an application
// might want to organize its code for filling in
// a descriptor set based on a C++ type like `AppModel`
// or `AppLight`, we have found a specific idiom that
// works well for portable applications that want to
// have clean/modular design in their shader and
// application code.
//
// A *shader cursor* is an application data type that
// can conceptually point "into" a descriptor table/set
// (or another application object used to represent a
// collection of shader parameter data). A shader
// cursor is a lot like a pointer in C/C++ (e.g.,
// a `Model*`), and supports similar operations:
//
// * Given a pointer to a value of `struct` type, we can form
// a pointer to one of the fields of that struct.
//
// * Given a pointer to a value of array type, we can form
// a pointer to one of the elements of that array.
//
// * Given a pointer to a `float` or other ordinary value,
// we can write/store a value to that address.
//
// An application-specific shader cursor implementation
// thus looks something like:
//
//
//      struct AppShaderCursor
//      {
//          AppShaderCursor getField(char const* fieldName);
//          AppShaderCursor getField(int fieldIndex);
//          AppShaderCursor getElement(int elementIndex);
//
//          void write( void const* data, size_t dataSize );
//          void write( Point3 const& value );
//          void write( Color3 const& value );
//          // ...
//
//          void write( AppTexture* texture );
//          void write( AppSampler* sampler );
//          // ...
//      };
//
// Most of the rest of this file is concerned with what
// the Slang reflection API provides to make a type
// like `AppShaderCursor` possible to implement.
// Before we get into that, though, let's briefly
// look at how application code can use this model
// to implement writing the state of a C++ object
// like `AppModel` to a descriptor set for a Slang
// type like `Model`:
//
//      void AppModel::writeInto( AppShaderCursor cursor )
//      {
//          m_material->writeInto( cursor.getField("material") );
//          cursor.getField("modelMatrix").write(m_modelMatrix);
//      }
//
//      void AppMaterial::writeInto( AppShaderCursor cursor )
//      {
//          cursor.getField("diffuseMap").write(m_diffuseMap);
//          cursor.getField("specularMap").write(m_specularMap);
//      }
//
//      void AppLight::writeInto( AppShaderCursor cursor )
//      {
//          cursor.getField("dir").write(m_dir);
//          cursor.getField("intensity").write(m_intensity);
//          cursor.getField("shadowMap").write(m_shadowMap);
//      }
//
// Hopefully these examples help show why the shader cursor
// idiom is such a powerful model for shader parameter setting
// in application code.
//
// One important thing to note is the way that the `AppModel::writeInto`
// method is able to delegate filling in the `material` field
// of `Model` to the `AppMaterial` type. Each C++ type only
// needs to be concerned with the Slang type that it corresponds to,
// and careful factoring of state into types on both the C++ and
// Slang types can enable good separation of concerns.
//
// Readers who care a lot about performance might be anxious
// to see string-based lookups in the code above, and worry
// that shader cursors fundamentally require such lookups at
// runtime. In practice, an application that knows the order
// of the fields within a shader `struct` (and that the order
// won't change) can use explicit indices instead of strings:
//
//      void AppLight::writeInto( AppShaderCursor cursor )
//      {
//          cursor.getField(0).write(m_dir);
//          cursor.getField(1).write(m_intensity);
//          cursor.getField(2).write(m_shadowMap);
//      }
//
// In fact, the string-based lookup operation can just be
// layered on top of index-based lookup:
//
//      AppShaderCursor AppShaderCursor::getField(const char* fieldName)
//      {
//          int fieldIndex = m_typeBeingPointedAt->findFieldIndex(fieldName);
//          return getField(fieldIndex);
//      }
//
// This also tells us that the shader cursor at the very
// least needs to hang onto the Slang reflection information
// for the type that it currently points at:
//
//      extension AppShaderCursor
//      {
//          TypeLayout* m_typeBeingPointedAt;
//      };
//
// Ordinary Data is Easy
// =====================
//
// As covered above, the Slang reflection API as described
// in the previous two files already gives us what we need
// for ordinary data that would get written to a constant
// buffer.
//
// The application's shader cursor needs a way to track
// the buffer to write ordinary data into, and an offset
// into it:
//
//      extension AppShaderCursor
//      {
//          VkBuffer    m_constantBuffer;
//          size_t      m_byteOffset;
//      };
//
// With those fields added, it is relatively easy to
// implement the parts of `getField` and `getElement`
// that pertain to ordinary data:
//
//      AppShaderCursor AppShaderCursor::getField(int fieldIndex)
//      {
//          slang::VarLayout* field =
//              m_typeBeingPointedAt->getFieldByIndex(fieldIndex);
//          size_t fieldByteOffset =
//              field->getOffset();
//          
//          AppShaderCursor result = *this;
//          result.m_typeBeingPointedAt = field->getTypeLayout();
//          result.m_byteOffset += fieldByteOffset;
//
//          // ...
//
//          return result;
//      }
//
//      AppShaderCursor AppShaderCursor::getElement(int elementIndex)
//      {
//          auto elementTypeLayout =
//              m_typeBeingPointedAt->getElementTypeLayout();
//          size_t elementByteStride =
//              elementTypeLayout->getStride();
//
//          MyShaderCursor result = *this;
//          result.m_typeBeingPointedAt = elementTypeLayout;
//          result.byteOffset += index * elementByteStride;
//
//          // ...
//
//          return result;
//      }
//
// Note: the code above ignores details around error handling,
// as well as down-casting the type layout in `m_typeBeingPointedAt`
// to a structure or array type, depending on whether a field
// or element is being accessed.
//
// Ideally, we want a model under which all the *other* state
// in a type (the stuff that isn't ordinary data) can be handled
// in a way that adds a small number of lines in places of the
// `...`s in the two methods above.
//
// Using Binding Ranges
// ====================
//
// In order to support implementation of the navigation required
// by an application shader cursor, we introduce the idea of
// *binding ranges* in a type layout.
//
// Binding ranges are similar to descriptor ranges (and the
// details on *why* they are different will require a document
// of their own...). Every type breaks down into zero or
// more binding ranges:
//
extension TypeLayout
{
    Sequence<BindingRangeInfo> getBindingRanges();
};
//
// and every `struct` type layout records the offset,
// in binding ranges, of each of its fields:
//
extension StructTypeLayout
{
    Count getBindingRangeOffsetForField(Index fieldIndex);
};

// An application can easily account for binding ranges
// in its shader cursor by adding an additional offset
// that sits alongside the byte offset:
//
//      extension AppShaderCursor
//      {
//          Index   m_bindingRangeIndex;
//      };
//
// At that point, the `getField` operation is simple:
//
//      AppShaderCursor AppShaderCursor::getField(int fieldIndex)
//      {
//          // ...
//          auto bindingRangeOffsetForField =
//              m_typeBeingPointedAt->getBindingRangeOffsetForField(fieldIndex);
//
//          // ...
//          result.m_bindingRangeIndex += bindingRangeOffsetForField;
//          // ...
//      }
//
// Handling of arrays is made slightly trickier by the fact
// that an array of textures at the Slang language level maps
// to a *single* desctiptor range and a single binding range
// at the level of type layout. Tracking the binding range index
// alone is not sufficient, and the shader cursor also needs
// to track an array index *within* the indicated binding range:
//
//
//      extension AppShaderCursor
//      {
//          Index   m_arrayIndexInRange;
//      };
//
// The handling of array element indexing is then quite compact,
// although there is some subtlety:
//
//      AppShaderCursor AppShaderCursor::getElement(int elementIndex)
//      {
//          // ...
//
//          result.m_arrayIndexInRange *= m_typeBeingPointedAt->getElementCount();
//          result.m_arrayIndexInRange += elementIndex;
//
//          // ...
//      }
//
// Adding the desired element index into `m_arrayIndexInRange` likely
// makes sense, but a reader may be confused why this code multiplies
// any existing index by the number of elements in the array being
// indexed first. The long/short is that this logic properly computes
// the final linearized array index in cases where the high-level language
// code uses nested arrays.
//
// At this point the "traversal" parts of our example application
// shader cursor type are complete. Having binding ranges be
// exposed as an abstraction by the Slang reflection API was
// critical in making this kind of compact implementation possible.

// Okay, but what's *in* a binding range?
// ======================================
//
// What's missing here is how the application's shader cursor
// implementation is supposed to implement the writing of
// a texture descriptor, or other non-ordinary data into
// a cursor.
//
//      AppShaderCursor AppShaderCursor::write(AppTexture* texture)
//      {
//          // ???
//      }
//
// In our example, where we are primarily concerned with Vulkan,
// we can see that this operation should at some point fill in
// the fields of a `VkWriteDescriptorSet`, and that it should
// be able to get the information it needs from the binding range
// that the cursor currently "points" at:
//
struct BindingRangeInfo
{
    // A binding range stores the index of the descriptor set
    // for that range, in cases where the enclosing type
    // maps to multiple sets.
    //
    // This field can be used to fill in
    // `VkWriteDescriptorSet::dstSet`:
    //
    Index descriptorSetIndex;

    // The binding range stores its binding type, which can
    // be used to fill in `VkWriteDescriptorSet::descriptorType`:
    //
    BindingType type;

    // A single binding range maps to zero or more descriptor ranges
    // (although almost always just one). These fields can be
    // used to look up the matching descriptor range and that
    // descriptor range can be used to fill in
    // `VkWriteDescriptorSet::dstBinding`:
    //
    Index firstDescriptorRangeIndex;
    Count descriptorRangeCount;

    // For completeness, the binding range also stores the
    // total number of bindings in the range (which can be used
    // by the application for checking for out-of-range indexing).
    //
    Count bindingCount;

    // Finally, the range tracks the "leaf" type of this range,
    // which is the type represented by each binding (e.g., if
    // the range corresponds to an array like `Texture2D[10]`,
    // then the leaf type is just `Texture2D`), and also the
    // leaf variable, if any, that the range corresponds to
    // (which can be used by application code to read app-specific
    // attributes from that variable that might influence its
    // policies around shader parameter binding).
    //
    TypeLayout* leafTypeLayout;
    Var* leafVar;
};

// Now that we see what goes into a binding range, we can better
// see how an application might implement the write operations:
//
//      AppShaderCursor AppShaderCursor::write(AppTexture* texture)
//      {
//          auto bindingRangeInfo = m_entireTypeLayout->getBindingRange(m_bindingRangeIndex);
//          auto descriptorSetIndex = bindingRangeInfo.descriptorSetIndex;
//
//          auto descriptorSetInfo = 
//              m_entireTypeLayout->getDescriptorSet(descriptorSetIndex);
//          auto descriptorRangeInfo =
//              descriptorSetInfo->descriptorRanges[bindingRangeInfo.firstDescriptorRangeIndex];
//
//          VkWriteDescriptorSet write = {};
//
//          write.dstSet = m_descriptorSets[descriptorSetIndex];
//          write.dstBinding = descriptorRangeInfo.indexOffset;
//          write.dstArrayElement = m_arrayIndexInRange;
//          write.descriptorCount = 1; // we are writing a single texture, not an array of them
//          write.descriptorType = mapDescriptorType(bindingRangeInfo.bindingType);
//          write.pImageInfo = texture->getImageInfo();
//
//          // ...
//      }
//
// To support this operation, we see the final pieces of state
// that an application shader cursor needs to track:
//
//      extension AppShaderCursor
//      {
//          slang::TypeLayout*      m_entireTypeLayout;
//          List<VkDescriptorSet>   m_descriptorSets;
//      };
//
// Somewhat obviously, the cursor needs to track the descriptor set(s)
// being written to, in order to write to them. Less obviously, the
// cursor needs to track the type layout that was used when allocating
// the *entire* backing storage (optional constant buffer plus one or
// more descriptor sets), so that the binding range index can be
// interpreted relative to that.
//
// Aside: Can we just have one kind of ranges?
// ===========================================
//
// A reader might at this point ask why there are two distinct
// kinds of ranges being reflected: both descriptor ranges and binding
// ranges.
//
// Well, to be honest, the *writer* of this document is starting
// to wonder if that distinction is actually all that necessary.
// We should probably take some time to look into how this part of
// the reflection API is being implemented (and how it interacts
// with `gfx`) to see if we can make some simplifications.
//
// Sub-Object Ranges
// =================
//
// One case that has been glossed over so far is when the Slang
// type used for a `ParameterBlock` or `ConstantBuffer` in turn
// has a field that uses a `ParameterBlock` or `ConstantBuffer`
// type:
//
//      struct ModelParams
//      {
//          ParameterBlock<MaterialParams> material;
//          float4x4 modelMatrix;
//      }
//      // or:
//      struct ModelParams
//      {
//          ConstantBuffer<MaterialParams> material;
//          float4x4 modelMatrix;
//      }
//
// In such a case an application would, seemingly, like to
// re-use any buffers and/or descriptor sets that have
// already been filled in for `MaterialParams` when
// writing into buffer/descriptor data for `ModelParams`.
//
// In order to enable application code to work with such
// hierarchical representations (e.g., the "shader object"
// abstraction in `gfx`), we provide additional queries
// to identify the binding ranges within a type that
// represent logical sub-objects:
//
extension TypeLayout
{
    Sequence<SubObjectRange> subObjectRanges;
};
//
struct SubObjectRangeInfo
{
    // Each sub-object range is able to identify the
    // binding range that it corresponds to:
    //
    Index       bindingRangeIndex;

    // A sub-object range also records the offset
    // from starting `set` or `space` of the outer
    // type to the starting `set` or `space` of
    // the sub-object.
    //
    Count       spaceOffset;

    // Finally, a sub-object range records the
    // more detailed offset information for the
    // sub-object. The offsets on this `VarLayout`
    // encode the offsets of `binding`s or `register`s
    // in the sub-object relative to those of
    // the outer type.
    //
    VarLayout*  offset;
};
//
// Note: It isn't entirely clear that sub-object
// ranges pull enough weight to be worth it. They
// are currently only being used to implement parts
// of the "shader object" system in `gfx`, but
// a lot of the code in `gfx` doesn't care about
// sub-objects all that much.
//

// Conclusion
// ==========
//
// At this point, we've covered almost all of the
// reflection API surface area that is still
// relevant to Slang users. A lot of legacy code
// that users really shouldn't be using has been
// swept under the rug along the way.
//
// It should be clear at this point that the
// current API we are exposing isn't as close to
// the ideal as we might want. This document isn't
// trying to dictate what the future form of the API
// should look like, and instead has been using
// a hypothetical "better" API just as a way to
// help explain the bits of the current design that
// still seem justified.
//
// There are, realistically, a lot of details that
// this document *doesn't* cover, and perhaps
// some of those are big-picture things. Questions
// and feedback are welcome.
//

