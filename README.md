Created by D.S. Leonard on 5/10/26.
MIT License.


# Typesafe units for Geant4.
>> !! No accidental mixing values and units.
>>    These are type-safe, non-implicitly convertible units/measures.

Originally built for [DLG4::VolumeBuilders](https:://dsleonard-coding.github.io:VolumeBuilders)

## Installation
This library is delievered as header-only inline/template code in a single file.  No special build system is required.  Just copy the file to your include directory and do:
```
#include "DLG4Units.hh"
```

## Motivation:
In Geant4 you cannot distinguish between a length and a unitless value.
You can multiply mm * mm and treat that as a length. You can have the Native number 12 and
treat that as a length.  At some low level the computer only knows numbers
but we can do better at the API and avoid these bugs.
 For positive values, a Unit Length is the same a Length.
 And in VB  a Lmength is in fact(derives from) a Unit<Length>  !!
 Unitless doubles are things that multiply lengths to describe
 other lengths in reference to that Unit<Length>.   The result is also a Unit<Lenght>.

 ## Usage:
 ### Setting a Dimensioned length:
 And this is how DLG4Units works... You can only consruct a Length from an existing Length and a double
 All of these are equivalent:
 ```cpp
 #include <DLG4Units.hh>
 using DLG4::Units;
 Length x = 5.0 * Length::mm;    // No different from Geant syntax! but we used a typed unit (not double) !!
 Length y = 5.0 * Unit<Length>::mm;  //  Same thing.  Length is a Unit<Length>
 Length z = Length( 5.0, Length::mm );  // constructor version.
// Or even directly from a stream:
 std::stringstream s {"5.0"};
 Length a;
 s >> a.InUnits(Length::mm);
 ```
 ### -------------THE SAFETY  NET -------------------
 ```cpp
 Length x = 5.0 * Length::mm;    // No different from Geant syntax! but we used a typed unit (not double) !!
 Length y = (5.0   +  x ) * Length::mm;     // ******* THIS WON'T COMPILE!!!  x is ALREADY A LENGHT!!!!********
 Length y = x * Length::m;   //   Even this won't.  Even if we later support this multiplication, it wouldn't assign to a Length.
 ```

 ### Setting with Native/legacy/system values:
 You can still define a Length in system units in any of these
 explicit ways, ordered by preference:
 ```cpp
 Length x.Native = some_geant_double;
 Length x = some_geant_double * Length::native;
 Length x = Length::FromNative(some_geant_double);
 Length x = Length( some_geant_double, Length::native );
 ```
 Where one G4double may have passed from other code as 5.0 * CLHEP::mm for instance,
 but it's still a double, not a DLG4::Units::Length until you make it one.
 So simply assinging to x.Native allows interfacing with legacy code. You almost CANNOT mess this up.
 If you try to assign legacy values (doubles) to x directly it will fail.

 ### Retrieving Values in designated units:
```cpp
 SetGlobalDefaultUnit(Length::mm);
 x = 5.0 * Length::mm;
 G4double y= x.InUnits(length::cm);
 G4double z= x.InDefaultUnits;
 ```
 y is now 0.5 and z is 5.0


 ### Retrieving/Passing Values as/to system units:
 for the same x as above:
 ```cpp
G4Box* box = new G4Box("MyBox", x.Native, x.Native, x.Native);
 ```
### No auto
THIS DOES NOT WORK:
```
x = 5.0 * Length::mm;
auto y= x.InUnits(length::cm);  //  WILL NOT COMPILE
```
You must type the explicit G4double type.  The reason is the accessors like Native, InUnits() and InDefaultUnits are actually proxy objects.
They are not doubles.  They have conversion operators to doubles, but in C++ auto can only deduce the real object type,
and we do NOT want to capture proxies and lose forced access through their explicit names.
So, through some rather intricate modern C++ r-value magic, auto deduction is fully blocked.  This is better anyway for clarity.

 That's it, and again  you almost cannot mess this up. If you try to pass x, it will fail.
 You'd have to intentionally use x.InUnits(...) to mess it up.


### Type safety:
 In DLG4Units, or thus VolumeBuilders, you cannot pass a Length to a double parameter or a double to a Length parameter
 and a Units::Length::mm * Units::Lentgh::mm  is not a length (or presently even valid)
 And so you cannot accidentally multiply by the unit twice, OR forget to multiply and still
 assign to your target Length.  Ex:      10*length_obj  is assignable to a length, but length_obj*lenght_obj is not.
 moss/volume is presently assignable to a density.

### Avoiding CLHEP units
The .Native interface is necessary for interface within shared variables and Geant calls,
but can also be abused, causing loss of type safety:
```
 Length x.Native =  5.0 *  CLHEP::mm );  // this IS still valid and correct, but poor form now.
 //  *******THIS COMPILES JUST FINE  BUT IS WRONG**********
 Length y.Native  = (5.0   +  x.Native ) * CLHEP::mm;  
 ```
And this is why using CLHEP will lead to bugs.  The type safe system is only safe if you use it. Type .Native only when needed, and never use CLHEP units.

### Vectors
The Unit3Vec (typdefed Length3Vec) behaves almost exactly the same.  It provides standard G4ThreeVector through .Native
, including access through .Native vector, the .Native.x()  double element, .x().Native double element, or .x()  Length.
It is typed and can be multiplied with Length objects.  It has a few extra convenience constructors that are self explanitory.  One difference is that the units used for Unit3Vec are Length not Unit3Vec. While the later is mathemtically possible and could be supported, it isn't presently.  Length is typically more idiomatic.  

## Usage in VolumeBuilders
DLG4::VolumeBuilders takes units and values natively and distinguishes them, so no .Native use
is needed except for direct geant calls.  If you're working with raw numbers and default units of mm, you will just
need to translate shared (context/global) values with .InDefaultUnits after setting your the default for your local method.
Shared variables can be of type Length and used in either VB or traditional code.
