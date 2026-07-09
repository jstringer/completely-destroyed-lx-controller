# NAP RTTI macro reference

Every RTTI **registration** macro (`RTTI_BEGIN_*`) goes in a **`.cpp`** file (documented in
`<SDK>/include/rtti/typeinfo.h`); the **enable** macro (`RTTI_ENABLE`) and `DECLARE_COMPONENT` go in
the **header**, inside the class body. Examples below are verified against this SDK.

## Header macros (in the class body)

```cpp
class NAPAPI SpinComponent : public Component
{
    RTTI_ENABLE(Component)                                 // names the RTTI base — walk this chain
    DECLARE_COMPONENT(SpinComponent, SpinComponentInstance) // component only: binds resource<->instance
public:
    SpinComponent() : Component() { }
    float mSpeed = 0.0f;                 ///< Property: 'Speed'
    ResourcePtr<Fixture> mFixture;       ///< Property: 'Fixture'
};
```

- `RTTI_ENABLE(Base)` — declares the class's RTTI and its base for the type system. A plain resource
  uses `RTTI_ENABLE(Resource)`; an instance uses `RTTI_ENABLE(ComponentInstance)`. Inherited
  properties come from the bases named here — follow the chain to enumerate them all.
- `DECLARE_COMPONENT(Comp, CompInstance)` — component only; makes `Comp::getInstanceType()` return
  `CompInstance` so NAP knows which runtime object to build. (`<SDK>/system_modules/napscene/include/component.h:197`)

## Class registration (in the `.cpp`)

```cpp
RTTI_BEGIN_CLASS(nap::SpinComponent, "Spins a transform")          // 1st arg == the JSON "Type"
    RTTI_PROPERTY("Speed",   &nap::SpinComponent::mSpeed,   nap::rtti::EPropertyMetaData::Default)
    RTTI_PROPERTY("Fixture", &nap::SpinComponent::mFixture, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::SpinComponentInstance) // instance has no default ctor
    RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)          // the mandatory instance ctor
RTTI_END_CLASS
```

Verified pattern: `<SDK>/apps/lxcontrol/module/src/fixturedmxwritercomponent.cpp:5-11`,
`<SDK>/demos/curveball/module/src/animationcomponent.cpp:14-20`.

- `RTTI_BEGIN_CLASS(Type)` / `RTTI_BEGIN_CLASS(Type, "desc")` — register a default-constructible class.
- `RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(Type)` — for classes without a default ctor (all
  `ComponentInstance`s, since they take `(EntityInstance&, Component&)`).
- `RTTI_PROPERTY("JsonName", &Class::mMember, flag [, "desc"])` — one per serialized member; the
  string is the JSON key, `flag` is an `EPropertyMetaData` (below).
- `RTTI_CONSTRUCTOR(argtypes…)` — registers a constructor the RTTI system may invoke.

## EPropertyMetaData flags (`<SDK>/include/rtti/typeinfo.h:165`)

| Flag | Value | JSON effect |
|------|-------|-------------|
| `Default`  | 0 | Optional; uses the class default if omitted. A pointer property → an **mID string reference**. |
| `Required` | 1 | Load fails if the property is absent. |
| `FileLink` | 2 | Value is a path to an external file. |
| `Embedded` | 4 | Pointer target is written as a **full inline object**, defined only here. |
| `ReadOnly` | 8 | Read-only. |

Combine with `|` (e.g. `Required | Embedded`). The flag is **not** visible in JSON or the header field
type — for system-module types it's in unshipped `.cpp`, so read it from a working JSON block
(`nap_usage.py <Type>`); see `nap-resource-graph/references/lookup-and-pointers.md`.

## Other registration macros

```cpp
RTTI_BEGIN_STRUCT(nap::SpinProperties)          // a plain data struct used as a property value
    RTTI_PROPERTY("Axis", &nap::SpinProperties::mAxis, nap::rtti::EPropertyMetaData::Default)
RTTI_END_STRUCT

RTTI_BEGIN_ENUM(nap::ESpinMode)                 // defines the serialized ENUM STRINGS (in .cpp)
    RTTI_ENUM_VALUE(nap::ESpinMode::One,  "One"),
    RTTI_ENUM_VALUE(nap::ESpinMode::Two,  "Two")
RTTI_END_ENUM

DEFINE_GROUP(nap::ResourceGroup, nap::Resource) // register a Group<T> template instantiation as a Type
```

- `RTTI_BEGIN_STRUCT/RTTI_END_STRUCT` — for a non-`Resource` struct exposed as a property (e.g. a
  `Properties` sub-object). Verified: `<SDK>/demos/linemorphing/module/src/linenoisecomponent.cpp:13`.
- `RTTI_BEGIN_ENUM/RTTI_ENUM_VALUE/RTTI_END_ENUM` — maps a C++ `enum class` value to the exact string
  used in JSON. Documented example: `<SDK>/include/rtti/typeinfo.h:88-92`. This is *why* enum strings
  aren't in headers — they're defined here, in the `.cpp`.
- `DEFINE_GROUP(AliasType, ObjectType)` — registers a `Group<T>` alias (e.g. `nap::ResourceGroup`) as
  a usable `"Type"`. (`<SDK>/include/nap/group.h:237`)
