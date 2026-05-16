// DLG4Units.hh
// Created by D.S. Leonard on 5/10/26.  MIT License
//
#ifndef DLG4MODUSIM_DLG4UNITS_HH
#define DLG4MODUSIM_DLG4UNITS_HH
#include "G4Types.hh"
#include "G4ExceptionSeverity.hh" // For FatalException, JustWarning, etc.
#include "globals.hh"             // For G4Exception and G4double
#include <CLHEP/Units/SystemOfUnits.h>
#include <G4ThreeVector.hh>
#include <optional>
//#include <VolumeBuilderConfigs.hh>


/**
 * @file DLG4Units.hh Typesafe units, for VolumeBuilders, Geant, ETC !! No accidental mixing values and units.
     These are type-safe, non-implicitly convertible units/measures.

     Main reop and documentation moved to https://dsleonard-coding.github.io:DLG4Units
    */

/**
 * Typesafe units!! No accidental mixing values and units.
 */

// TODO, doxygen all this.

namespace VB = DLG4::VolumeBuilders;

namespace DLG4::Units {
    template<typename T, class NativeType>
    class UnitBase;
    struct UnitTag;
    class Length;
    class Mass;
    class Volume;
    class Density;
    class Unit3Vec;
    template<typename Dimension>
    using Unit = UnitBase<Dimension, double>;

    /// @copydoc DLG4::Units::Unit3Vec
    using Length3Vec = Unit3Vec;
    class ThreeVecDimensioner;

    template<typename T>
    inline G4double global_default_unit = 1.0;


    /**
    * @brief Set the global default unit.  Global, not thread safe.
    * @ingroup Units
    * */
    template<typename T>
    inline void SetGlobalDefaultUnit(T unit) {
        static_assert(std::is_base_of_v<DLG4::Units::UnitTag, T>,
                "\n\n ERROR: SetGlobalDefaultUnit only accepts DLG4Unit types! \n"
                " You tried to pass a type that does not inherit from UnitTag.\n");
        DLG4::Units::global_default_unit<T> = unit.Native;
    }

    // Set the default value for the global default unit.
    //  (The default for the default for the default).
    template<>
    inline G4double global_default_unit<Units::Length> = CLHEP::mm;
    template<>
    inline G4double global_default_unit<Units::Mass> = CLHEP::g;
    template<>
    inline G4double global_default_unit<Units::Volume> = CLHEP::mL;
    template<>
    inline G4double global_default_unit<Units::Unit3Vec> = CLHEP::mm;

    /**
     * Effectively a property setter for units
     */
    template<typename U, typename V>
    class UnitProperties_ {
    protected:
    };

    // An inheritance tag for SFINAE
    struct UnitTag {
    };

    /**
     * CRTP base methods for Unit classes.
     * @tparam T
     */
    template<class T, typename NativeType>
    class UnitBase : UnitTag {
        using Derived = T;

    protected:
        // Raw access to the Native value through a public.
        //  We could convert it to a proxy if needed later.
        std::optional<NativeType> NativeValue_;

        // Transient temporary calculation handler
        struct Evaluator {
            UnitBase &parent;
            G4double scale;

            // Explicit constructor required for emplace / standard initialization
            Evaluator(UnitBase &p, G4double s)
                : parent(p), scale(s) {
            }

            Evaluator(const Evaluator &) = delete;

            Evaluator(Evaluator &&) = delete;

            Evaluator &operator=(const Evaluator &) = delete;

            Evaluator &operator=(Evaluator &&) = delete;

            // Assignment operator
            // Evaluator is always a temporary rvalue proxy returned by operator(),
            // but assingment passes through to the permanent parent.
            // This allows to block auto x =    because x is an l-value, and we don't
            // define l-value assignement!
            void operator=(NativeType val) && { parent.NativeValue_ = val * scale; }
            // conversion operator
            operator NativeType() const {
                parent.CheckValue("Evaluator");
                return *(parent.NativeValue_) / scale;
            }

            friend std::istream &operator>>(std::istream &is, Evaluator &&ev) {
                NativeType temp_val;
                if (is >> temp_val) {
                    // Use existing rvalue assignment mechanism to update the parent
                    std::move(ev) = temp_val;
                }
                return is;
            }
        };

        template<typename X>
        struct ScalableProxy {
            // A callable proxy, returns a temporary Evaluator that handles both get and set
            UnitBase &parent;

            // delete copy and move ctor to protect the proxy.
            ScalableProxy(const ScalableProxy &) = delete;

            ScalableProxy(ScalableProxy &&) = delete;

            // Assignment is handled by the temporary Evaluator
            ScalableProxy &operator=(const ScalableProxy &) = delete;

            // We return by r-value temporary so we can block the move operator and
            // prevent auto x = y.InuUnits(...)
            // But the cost is we need temp storage, and if we have multiple temps in a calculation
            // we need multiple/separate storage.
            // An empty token whose only job is to force the C++ compiler to
            // reserve a dedicated slot on the local stack frame for this exact call.
            struct ExpressionToken {
                mutable std::optional<Evaluator> storage;
            };

            // Called on the lvalue member:
            // This can be const or non-const, but it returns a temporary Evaluator rvalue.
            // Enables obj.InDefaultUnits = 20.0; // and...
            // double x = obj.InUnits(cm)
            Evaluator &&operator()(const X &unit, const ExpressionToken &token = ExpressionToken{}) const {
                // Initialize the evaluator directly on this call's private stack space
                token.storage.emplace(parent, unit.ValidNativeUnitValue());
                return std::move(*token.storage);
            }
        };

        struct DefaultUnitsProxy {
            // Non-callable field-like proxy with assignment and conversion operators
            // Gets a default value for unit conversion
            UnitBase &parent;

            // delete copy and move
            DefaultUnitsProxy(const DefaultUnitsProxy &) = delete;

            DefaultUnitsProxy(DefaultUnitsProxy &&) = delete;

            // Enables obj.InDefaultUnits = 20.0;
            void operator=(NativeType val) { Evaluator{parent, GetGlobalDefault().NativeValue_.value()} = val; }
            // double x = obj.InDefaultUnits
            operator NativeType() const { return Evaluator{parent, GetGlobalDefault().NativeValue_.value()}; }

            friend std::istream &operator>>(std::istream &is, DefaultUnitsProxy &proxy) {
                NativeType temp_val;
                if (is >> temp_val) {
                    proxy = temp_val; // Reuses your assignment operator above safely
                }
                return is;
            }
        };

        struct NativeProxy {
            // Non-callable field-like proxy with assignment and conversion operators
            // Assigns to or reads the native internal value, no conversion value.
            UnitBase &parent;

            //delete copy and move
            NativeProxy(const NativeProxy &) = delete;

            NativeProxy(NativeProxy &&) = delete;

            // obj.Native = 10.0;
            void operator=(NativeType val) { parent.NativeValue_ = val; }
            // double x = obj.Native
            operator NativeType() const {
                parent.CheckValue("NativeProxy");
                return *(parent.NativeValue_);
            }

            friend std::istream &operator>>(std::istream &is, NativeProxy &proxy) {
                NativeType temp_val;
                if (is >> temp_val) {
                    proxy = temp_val; // Reuses your assignment operator above safely
                }
                return is;
            }
        };

    public:
        ScalableProxy<UnitBase> InUnits{*this};
        DefaultUnitsProxy InDefaultUnits{*this};
        NativeProxy Native{*this};

    protected:
        /// Validator for NativeValue_ if used in a unit context, errors if it was not initialized
        template<typename U = T, typename = std::enable_if_t<!std::is_base_of_v<Unit3Vec, U>> >
        [[nodiscard]] G4double ValidNativeUnitValue() const {
            this->CheckUnit("ValidNativeUnitValue");
            return NativeValue_.value();
        }

    public:
        static inline T native{1.0};
        static T FromNative(NativeType native_value) { return T(native_value); }


        UnitBase()
            : NativeValue_(std::nullopt) {
        }

        virtual ~UnitBase() = default;

        UnitBase(const UnitBase &other)
            : NativeValue_(other.NativeValue_) {
        }

        UnitBase &operator=(const UnitBase &other) {
            this->NativeValue_ = other.NativeValue_;
            return *this;
        }

        explicit UnitBase(double f, Derived u)
            : NativeValue_(f * u.ValidNativeUnitValue()) {
        }

        static T GetGlobalDefault() { return T(global_default_unit<T>); }

        void CheckValue(const G4String &context) const {
            // volatile may help with debugging dangling references.
            volatile bool engagement_flag = NativeValue_.has_value();
            if (!engagement_flag) {
                //if (!NativeValue_.has_value()) {
                G4Exception(context, "InvalidScale", FatalException,
                        "\n Attempted to use a Unit (or measure) before setting it;\n");
                exit(1);
            }
        }

        void CheckUnit(const G4String &context) const {
            CheckValue(context);
            if (!(NativeValue_ > 0.0)) {
                G4Exception(context, "InvalidScale", FatalException,
                        "\n Attempted to apply a negative Unit\n");
                exit(1);
            }
        }

    protected:

    protected:
        // The public can only make units from existing units, not doubles!!
        explicit UnitBase(G4double f)
            : NativeValue_(f) {
        };
    };


    //#########################################################################//
    //*********************************Length***************************#
    //#########################################################################//
    class Length : public UnitBase<Length, G4double> {
        friend UnitBase;
        friend Unit3Vec;

    private:
        explicit Length(double Native)
            : UnitBase(Native) {
        }

    public:
        explicit Length(double raw, Length u)
            : UnitBase(raw, u) {
        }

        Length() { NativeValue_ = GetGlobalDefault().NativeValue_; };

        // Deeclare essentially named singleton factories:
        static const Length fermi;
        static const Length angstrom;
        static const Length nm;
        static const Length pm;
        static const Length um;
        static const Length micron;
        static const Length mm;
        static const Length cm;
        static const Length dm;
        static const Length m;
        static const Length km;

        // Imperial
        static const Length mil;
        static const Length inch;
        static const Length foot;
        static const Length feet;
    };

    //#########################################################################//
    // ******************Mass Class
    //#########################################################################//
    class Mass : public UnitBase<Mass, G4double> {
        friend UnitBase<Mass, G4double>;

    private:
        explicit Mass(double Native)
            : UnitBase(Native) {
        }

    public:
        Mass() { NativeValue_ = GetGlobalDefault().NativeValue_; };

        explicit Mass(double raw, Mass u)
            : UnitBase(raw, u) {
        }

        static const Mass g, mg, kg;
    };

    //#########################################################################//
    // ******************Volume Class
    //#########################################################################//
    class Volume : public UnitBase<Volume, G4double> {
        friend UnitBase<Volume, G4double>;

    private:
        explicit Volume(double Native)
            : UnitBase(Native) {
        }

    public:
        Volume() { NativeValue_ = CLHEP::cm3; };

        explicit Volume(double raw, Volume u)
            : UnitBase(raw, u) {
        }

        static const Volume cm3, mL, L;
    };


    //#########################################################################//
    // ******************Density Class
    //#########################################################################//
    class Density : public UnitBase<Density, G4double> {
        friend UnitBase<Density, G4double>;

    private:
        explicit Density(double Native)
            : UnitBase(Native) {
        }

    public:
        Density() { NativeValue_ = GetGlobalDefault().NativeValue_; };

        explicit Density(double raw, Density u)
            : UnitBase(raw, u) {
        }

        static const Density g_per_cm3;
        static const Density g_per_L; // Same as mg_cm3
        static const Density mg_per_cm3;
    };


    inline Density operator/(const Mass &m, const Volume &v) {
        Density x = Density::FromNative(m.Native / v.Native);
        return x;
    }


    /**
     * A 3 vector that is scalable with/to Units.
     */
    class Unit3Vec : public UnitBase<Unit3Vec, G4ThreeVector> {
        //    class Unit3Vec : public Length {
        friend UnitBase<Length, G4ThreeVector>;
        //        friend Length;
    private:
        //obj.Native.x() doesn't work without casting obj.Native to the vector first.
        // This proxy wraps the casts to give us direct .Native.x() methods.'
        struct NativeVectorProxy {
            Unit3Vec &parent;

            void operator=(const G4ThreeVector &val) { parent.NativeValue_ = val; }

            operator G4ThreeVector() const {
                return static_cast<G4ThreeVector>(static_cast<const UnitBase &>(parent).Native);
            }

            G4double x() const { return static_cast<G4ThreeVector>(parent.Native).x(); }
            G4double y() const { return static_cast<G4ThreeVector>(parent.Native).y(); }
            G4double z() const { return static_cast<G4ThreeVector>(parent.Native).z(); }

            friend std::istream &operator>>(std::istream &is, NativeVectorProxy &proxy) {
                G4ThreeVector temp_val;
                if (is >> temp_val) {
                    // Invokes Geant4's native stream parser, then
                    // routes straight into proxy assignment operator
                    proxy = temp_val;
                }
                return is;
            }
        };

    public:
        // Shadow the base class member variable
        NativeVectorProxy Native{*this};
        // shadow Scalable Proxy to scale with a length, not a vector
        ScalableProxy<Length> InUnits{*this};

        Unit3Vec()
            : Native{*this} {
        }

        //Copy Constructor
        Unit3Vec(const Unit3Vec &other)
            : UnitBase(other),
              default_length_(other.default_length_),
              Native{*this},
              InUnits{*this} {
        }

        // Move Constructor
        Unit3Vec(Unit3Vec &&other) noexcept
            : UnitBase(std::move(other)),
              default_length_(std::move(other.default_length_)),
              Native{*this},
              InUnits{*this} {
        }

        // Copy Assignment Operator
        Unit3Vec &operator=(const Unit3Vec &other) {
            if (this != &other) {
                UnitBase::operator=(other); // Safely copies the unified base NativeValue_
                this->default_length_ = other.default_length_;
            }
            return *this;
        }

        // Move Assignment Operator
        Unit3Vec &operator=(Unit3Vec &&other) noexcept {
            if (this != &other) {
                UnitBase::operator=(std::move(other)); // Safely moves the unified base NativeValue_
                this->default_length_ = std::move(other.default_length_);
            }
            return *this;
        }

        Unit3Vec(double x, double y, double z, const Length &u) {
            u.CheckUnit("Unit3Vec(x,y,z,u)");
            NativeValue_ = G4ThreeVector(x, y, z) * u.NativeValue_.value();
        }

        Unit3Vec(const G4ThreeVector &v, const Length &u) {
            u.CheckUnit("Unit3Vec(x,y,z,u)");
            NativeValue_ = v * u.NativeValue_.value();
        }

        static Unit3Vec FromNative(const G4ThreeVector &v) {
            return Unit3Vec(v, Length::native);
        }


        // components as Length types...
        Length x() const {
            CheckValue("Unit3Vec");
            return Length::FromNative(NativeValue_.value().x());
        }

        Length y() const {
            CheckValue("Unit3Vec");
            return Length::FromNative(NativeValue_.value().y());
        }

        Length z() const {
            CheckValue("Unit3Vec");
            return Length::FromNative(NativeValue_.value().z());
        }

        void x(const Length &val) {
            val.CheckValue("Unit3Vec, x");
            EnsureInternalVec().setX(val.NativeValue_.value());
        }

        void y(const Length &val) {
            val.CheckValue("Unit3Vec, x");
            EnsureInternalVec().setY(val.NativeValue_.value());
        }

        void z(const Length &val) {
            val.CheckValue("Unit3Vec, x");
            EnsureInternalVec().setZ(val.NativeValue_.value());
        }

        Unit3Vec(const Length &x, const Length &y, const Length &z) {
            x.CheckValue("Unit3Vec");
            y.CheckValue("Unit3Vec");
            z.CheckValue("Unit3Vec");
            NativeValue_ = G4ThreeVector(x.NativeValue_.value(), y.NativeValue_.value(), z.NativeValue_.value());
        }

    protected:
        Length default_length_{Length::native};

    private:
        Unit3Vec(G4double value) {
        }

        // Helper to ensure the optional is initialized before we write to it
        G4ThreeVector &EnsureInternalVec() {
            if (!NativeValue_.has_value()) {
                NativeValue_ = G4ThreeVector(0, 0, 0);
            }
            return *NativeValue_;
        }
    };


    //********************************************************************//
    //                Unit Definitions
    //********************************************************************//

    // Definitions
    inline const Length Length::fermi{CLHEP::fermi};
    inline const Length Length::angstrom{CLHEP::angstrom};
    inline const Length Length::nm{CLHEP::nanometer};
    inline const Length Length::pm{1e-9 * CLHEP::mm};
    inline const Length Length::um{CLHEP::micrometer};
    inline const Length Length::micron{CLHEP::micrometer}; // Safe alias to um
    inline const Length Length::mm{CLHEP::mm};
    inline const Length Length::cm{CLHEP::cm};
    inline const Length Length::m{CLHEP::m};
    inline const Length Length::km{CLHEP::kilometer};

    // Imperial Logic: 1 inch = 25.4 mm
    inline const Length Length::mil{0.0254}; // 1/1000 of an inch
    inline const Length Length::inch{25.4}; // CLHEP::inch
    inline const Length Length::foot{304.8}; // 12 inches
    inline const Length Length::feet{304.8}; // 12 inches

    inline const Mass Mass::mg{CLHEP::mg};
    inline const Mass Mass::g{CLHEP::g};
    inline const Mass Mass::kg{CLHEP::kg};

    inline const Volume Volume::mL{CLHEP::mL};
    inline const Volume Volume::cm3{CLHEP::mL};
    inline const Volume Volume::L{CLHEP::L};

    inline const Density Density::g_per_cm3{CLHEP::g / CLHEP::cm3};
    inline const Density Density::g_per_L{CLHEP::g / CLHEP::liter};
    inline const Density Density::mg_per_cm3{CLHEP::mg / CLHEP::cm3};

    /**
     * Type-erases unit or value to just a value. BYPASSES TYPE SAFETY!!
     */
    template<class T, class NativeType>
    struct UnitOrValue {
        G4double val;

        UnitOrValue(const UnitBase<T, NativeType> &u)
            : val(u.Native) { u.CheckUnit("UnitOrValue"); }

        // Catch any numeric scalar (int, float, etc.)
        template<typename Scalar, typename = std::enable_if_t<std::is_arithmetic_v<Scalar> > >
        UnitOrValue(Scalar d)
            : val(static_cast<G4double>(d)) {
        }
    };

    //********************************************************************//
    //                Math Operators
    //********************************************************************//


    // Mutliplication and division of scalar and unit create Unit.
    // Which one is the Unit?
    template<typename T, typename U>
    using ResultType = std::conditional_t<std::is_base_of_v<UnitTag, T>, T, U>;
    template<typename T>
    using NativeType = std::conditional_t<std::is_base_of_v<Unit3Vec, T>, G4ThreeVector,
        std::conditional_t<std::is_base_of_v<UnitTag, T>, G4double,
            T> >;

    // --- Multiplication: (Unit * Scalar) or (Scalar * Unit) ---
    template<typename T, typename U,
        typename = std::enable_if_t<std::is_base_of_v<UnitTag, T> != std::is_base_of_v<UnitTag, U>> >
    inline auto operator*(const T &a, const U &b) {
        using R = ResultType<T, U>;
        return R(UnitOrValue<T, NativeType<T> >(a).val * UnitOrValue<U, NativeType<U> >(b).val, R::native);
    }

    // --- Division:  (Unit / Scalar) ---
    template<typename T, typename U,
        typename = std::enable_if_t<std::is_base_of_v<UnitTag, T> && !std::is_base_of_v<UnitTag, U>> >
    inline auto operator/(const T &a, const U &b) {
        // T is the Unit, U is the Scalar
        return T(UnitOrValue<T, NativeType<T> >(a).val / static_cast<double>(b), T::native);
    }

    // --- Addition: T + T (where T inherits from UnitTag) ---
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<UnitTag, T> > >
    inline T operator+(const T &a, const T &b) {
        // Uses the protected constructor/FromNative to return a new dimensioned object
        return T::FromNative(a.Native + b.Native);
    }

    // --- Subtraction: T - T (where T inherits from UnitTag) ---
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<UnitTag, T> > >
    inline T operator-(const T &a, const T &b) {
        return T::FromNative(a.Native - b.Native);
    }


    using UnitlessG4Transform3D = G4Transform3D;
    using Length3Vec = Unit3Vec;

    template<typename T, typename = void>
    struct is_valid_expr : std::false_type {
    };

    template<typename T>
    struct is_valid_expr<T, std::void_t<decltype(std::declval<T>()())> > : std::true_type {
    };

    // Syntactic sugar variable template
    template<typename F>
    inline constexpr bool is_invalid_expression = !is_valid_expr<F>::value;

    //    Uncomment below for testing:
    // class Test {
    //     void testmethod(const Length &u) const {
    //         std::stringstream s {"5.0"};
    //         Length in;
    //         s >> in.InUnits(Length::mm);
    //         Length x = 5.0 * Length::mm;
    //         Length y = Length::FromNative(5.0);
    //         auto z = x + y;
    //         static_assert(is_invalid_expression<decltype([&]() {
    //                auto a = z.InDefaultUnits;
    //         })>, "Test Failed: auto deducing a proxy should not compile");
    //         auto a = z.InUnits(Length::mm);
    //         static_assert(is_invalid_expression<decltype([&]() {
    //                auto g = z.InUnits(Length::mm);
    //         })>, "Test Failed: auto deducing a proxy should not compile");
    //
    //         double a = z.InDefaultUnits;
    //         z.InDefaultUnits = 10;
    //         double b = z.InUnits(Length::mm);
    //         z.InUnits(Length::mm) = 10;
    //         static_assert(is_invalid_expression<decltype([&]() {
    //                auto b = z.InDefaultUnits;
    //         })>, "Test Failed: auto deducing a proxy should not compile");
    //
    //         static_assert(is_invalid_expression<decltype([&]() {
    //                auto a = z.InDefaultUnits;
    //         })>, "Test Failed: auto deducing a proxy should not compile");
    //
    //         static_assert(is_invalid_expression<decltype([&]() {
    //             Length x = 5.0;
    //         })>, "Test Failed: Length x = 5.0 should not compile.");
    //
    //         static_assert(is_invalid_expression<decltype([&]() {
    //             Length x = Length::mm;
    //             x = x * Length::mm; // Actual code block being tested
    //         })>, "Test Failed: x = x * Length::mm should not compile.");
    //
    //         static_assert(is_invalid_expression<decltype([&]() {
    //             auto result = Length::mm + Mass::g;
    //         })>, "Test Failed: Adding Mass to Length should not compile.");
    //
    //         static_assert(is_invalid_expression<decltype([&]() {
    //             Length x = Length::mm;
    //             auto c = x + 5.0; // Actual code block being tested
    //         })>, "Test Failed: cannot add Length to double");
    //      }
    //  };
};

#endif //DLG4MODUSIM_DLG4UNITS_HH
