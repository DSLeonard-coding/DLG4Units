Created by D.S. Leonard on 5/10/26.
MIT License.


# Type-safe units for Geant4.
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
 And in VB  a Length is in fact(derives from) a Unit<Length>  !!
 Unitless doubles are things that multiply lengths to describe
 other lengths in reference to that Unit<Length>.   The result is also a Unit<Lenght>.

 ## Usage:
 ### Setting a Dimensioned length:
 ...and that is how DLG4Units works: You can only consruct a Length from an existing Length and a double.  
 All of these are equivalent:
 ```cpp
 #include <DLG4Units.hh>
 using DLG4::Units;
 Length x = 5.0 * Length::mm;    // No different from Geant syntax! but we used a typed unit (not double) !!
 Length z = Length( 5.0, Length::mm );  // constructor version.
// Or even directly from a stream:
 std::stringstream s {"5.0"};
 Length a;
 s >> a.InUnits(Length::cm);  //   a is now 5cm.
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
 Length x = some_geant_double * Length::native;      // best for initialization.
 Length x; x.Native = some_geant_double;             // best for assignment. 
 Length x = Length::FromNative(some_geant_double);          //Factory Version.
 Length x = Length( some_geant_double, Length::native );    // ctor version.
 ```
 Where some_geant_double may have passed from other code as 5.0 * CLHEP::mm for instance,
 but it's still a double, not a DLG4::Units::Length until you make it one.
 So simply assinging to x.Native allows interfacing with legacy code. You almost CANNOT mess this up.
 If you try to assign legacy values (doubles) to x directly it will fail.

 ### Retrieving Values in designated units:
```cpp
 SetGlobalDefaultUnit(Length::mm);
 x = 5.0 * Length::mm;
 G4double y = x.InUnits(Length::cm);
 G4double z = x.InDefaultUnits;
 ```
 y is now 0.5 and z is 5.0


 ### Retrieving/Passing Values as/to system units:
 for the same x as above:
 ```cpp
G4Box* box = new G4Box("MyBox", x.Native, x.Native, x.Native);
 ```
 That's it, and again  you almost cannot mess this up. If you try to pass x, it will fail.
 You'd have to intentionally use x.InUnits(...) to mess it up.

 
 To pass litteral values inline directly, you can of course just revert to CLHEP or use Native:
```
G4Box* box = new G4Box("MyBox", 3.0 * CLHEP::mm, 
                                3.0 * Length::mm.Native, 
                                (someLength + 3.0 * Length::mm).Native
                              );
```

 
### No auto
THIS DOES NOT WORK:
```
x = 5.0 * Length::mm;
auto y = x.InUnits(length::cm);  //  WILL NOT COMPILE
```
You must type the explicit G4double type.  The reason is the accessors like Native, InUnits() and InDefaultUnits are actually proxy objects.
They are not doubles.  They have conversion operators to doubles, but in C++ auto can only deduce the real object type,
and we do NOT want to capture proxies and lose forced access through their explicit names.
So, through some rather intricate modern C++ r-value magic/hacking, auto deduction is fully blocked.  This is better anyway for clarity.



### Type safety:
 In DLG4Units, or thus VolumeBuilders, you cannot pass a Length to a double parameter or a double to a Length parameter
 and a Units::Length::mm * Units::Lentgh::mm  is not a length (or presently even valid)
 And so you cannot accidentally multiply by the unit twice, OR forget to multiply and still
 assign to your target Length.  Ex:      ```10 \* length\_obj```  is assignable to a length, but ```length\_obj \* lenght\_obj``` is not.


### Avoiding CLHEP units
The .Native interface is necessary for interface within shared variables and Geant calls,
but can also be abused, causing loss of type safety:
```
 Length x.Native =  5.0 * CLHEP::mm ;  // this IS still valid and correct, but poor form now.
 //  *******THIS COMPILES JUST FINE  BUT IS WRONG**********
 Length y.Native = ( 5.0 + x.Native ) * CLHEP::mm;  
 ```
And this is why using CLHEP will lead to bugs.  The type safe system is only safe if you use it. Type .Native only when needed, and never use CLHEP units.

### Vectors
The Length3Vec behaves almost exactly the same.  It provides standard G4ThreeVector through .Native
, including access through the obj.Native vector, the obj.Native.x()  double element, obj.x().Native double element, or obj.x()  Length.
It is typed and can be multiplied with doubles.  It has a few extra convenience constructors that are self explanitory.  One difference is that the units used for Length3Vec are Length not Length3Vec. While the later is mathemtically possible and could be supported, it isn't presently.  Length is typically more idiomatic.  

### Other Units, Density, Angle, etc...
As well as Length, Density, Mass, Volume and Angle are implemented. Mass/Volume presently constructs a Density and some presets exist, specifically Density::g_per_cm3,Density::g_per_L, or equivalently, Density::mg_per_cm3.   This is clearly less useful as density is often something you just pass a value to geant nearly in place, without calculations, and CLHEP can be ok there, but it is useful to maintain clarity for non-local configuration variables, and is used by the VolumeBuilders CopyMaterial methods, where it did resolve confusion. 

Angle units are also implemented, but are maybe the least useful.  Even a G4RotationMatrix does not itself carry units or even actual angles.    

## Usage in DLG4::GeoModules/HpgeSim/DLG4::ModuSim/VBDemo

The DLG4 series geometry sim packages based on [DLG4::GeoModules](https://dsleonard-coding.github.io/VolumeBuilders/md_GeoModules_2GeoModulesREADME.html) use centralized "context" variables for position interfacing.  While these can use any variables, they are being migrated to DLG4Units because this is a prime example of where confusion happens.  Does can_top_cm  mean it was multiplied by CLHEP::cm, or it means the value of 5 that is holds means 5 cm (ie should be multiplied by CLHEP::cm)? It was defined elsewhere.   To get can_top_mm  from from can_top I divide by CLHEP::mm?  (Yes, but easy to mess up).  There is no confusion with DLG4Units.  If you need the geant native you just take .Native.   If you need a value in units of cm you take .InUnits(Length::cm).
 
## Usage in VolumeBuilders
DLG4::VolumeBuilders takes units and values natively and distinguishes them, so no .Native use
is needed except for direct geant calls.  If you're working with raw numbers and default units of mm, you will just
need to translate shared (context/global) values with .InDefaultUnits after setting your the default for your local method.
Shared variables can be of type Length and used in either VB or traditional code.


