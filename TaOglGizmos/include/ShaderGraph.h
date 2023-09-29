#ifndef TAOGL_SHADERGRAPH_H
#define TAOGL_SHADERGRAPH_H

#include <concepts>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <algorithm>
#include <format>
#include <map>

#define TGF_DEBUG

namespace tao_gizmos_shader_graph
{

    class SGNode
    {
    public:
        std::optional<std::string> Name = std::nullopt;

        virtual std::string GlslDeclString() const = 0;

        inline std::string GlslString() const
        {
            return Name.has_value() ? Name.value() : GlslStringProtected();
        }

        inline virtual int ChildrenCount()  const = 0;
        inline virtual SGNode& ChildAt(int i) const = 0;

#ifdef TGF_DEBUG
        inline virtual std::string TgfLabel() const {return "ShaderGraphNode"; };
#endif

    protected:
        inline virtual std::string GlslStringProtected() const = 0;
    };

    class SGFloatType;
    class SGIntType;
    class SGBoolType;

    template<typename T>
    concept glslType =
    std::same_as<T, SGFloatType>		||
    std::same_as<T, SGIntType>			||
    std::same_as<T, SGBoolType>			;

    class SGMat4 : public SGNode
    {
        inline virtual std::string GlslDeclString() const override {return "error"; }
#ifdef TGF_DEBUG
        inline virtual std::string TgfLabel() const override {return "SGMat4"; };
#endif
    };

    template<int N, typename T> requires glslType<T>
    class SGVector : public SGNode
    {
    public:
        static constexpr int num_components = N;
        inline virtual std::string GlslDeclString() const override {return "error"; }
#ifdef TGF_DEBUG
        inline virtual std::string TgfLabel() const override {return "SGVector" + std::to_string(N); };
#endif
    };

    typedef SGVector<1, SGFloatType> SGFloat;
    typedef SGVector<2, SGFloatType> SGVec2;
    typedef SGVector<3, SGFloatType> SGVec3;
    typedef SGVector<4, SGFloatType> SGVec4;
    template<> inline std::string SGFloat::GlslDeclString() const { return "float";};
    template<> inline std::string SGVec2::GlslDeclString() const { return "vec2";};
    template<> inline std::string SGVec3::GlslDeclString() const { return "vec3";};
    template<> inline std::string SGVec4::GlslDeclString() const { return "vec4";};

    typedef SGVector<1, SGIntType> SGInt;
    typedef SGVector<2, SGIntType> SGIvec2;
    typedef SGVector<3, SGIntType> SGIvec3;
    typedef SGVector<4, SGIntType> SGIvec4;
    template<> inline std::string SGInt::GlslDeclString() const { return "int";};
    template<> inline std::string SGIvec2::GlslDeclString() const { return "ivec2";};
    template<> inline std::string SGIvec3::GlslDeclString() const { return "ivec3";};
    template<> inline std::string SGIvec4::GlslDeclString() const { return "ivec4";};

    typedef SGVector<1, SGBoolType> SGBool;
    typedef SGVector<2, SGBoolType> SGBvec2;
    typedef SGVector<3, SGBoolType> SGBvec3;
    typedef SGVector<4, SGBoolType> SGBvec4;
    template<> inline std::string SGBool::GlslDeclString() const { return "bool";};
    template<> inline std::string SGBvec2::GlslDeclString() const { return "bvec2";};
    template<> inline std::string SGBvec3::GlslDeclString() const { return "bvec3";};
    template<> inline std::string SGBvec4::GlslDeclString() const { return "bvec4";};

    template<int C, typename SGType, typename CType> requires std::derived_from<SGType, SGNode>
    class SGVectorConst : public SGType
    {
    public:
        template<typename... TODO> requires (sizeof...(TODO) == C) // TODO: same type as CType
        SGVectorConst(const char* name, const TODO&... args)
        {
            SGType::Name = name;
            int i=0;
            ((_value[i++] = args),...);
        }
        template<typename... TODO> requires (sizeof...(TODO) == C) // TODO: same type as CType
        SGVectorConst(const TODO&... args)
        {
            int i=0;
            ((_value[i++] = args),...);
        }
        std::string inline GlslDeclString() const override;
        int inline ChildrenCount() const override { return 0; }
        inline SGFloat& ChildAt(int i) const override{ throw std::runtime_error(std::string("Cannot access child in a leaf node")); }

    protected:
        inline std::string GlslStringProtected() const override final
        {
            auto str = GlslDeclString().append("(");
            for(int i=0;i<C;i++)
            {
                str.append(std::to_string(_value[i]));
                if(i<C-1)str.append(",");
            }
            str.append(")");

            return str;
        };
#ifdef TGF_DEBUG
        inline virtual std::string TgfLabel() const override;
#endif
    private:
        CType _value[C];
    };

    typedef SGVectorConst<1, SGFloat, float> SGFloatConst;
    typedef SGVectorConst<2, SGVec2 , float> SGVec2Const;
    typedef SGVectorConst<3, SGVec3 , float> SGVec3Const;
    typedef SGVectorConst<4, SGVec4 , float> SGVec4Const;
    template<> inline std::string SGFloatConst::GlslDeclString() const { return "const float";}
    template<> inline std::string  SGVec2Const::GlslDeclString() const { return "const vec2";}
    template<> inline std::string  SGVec3Const::GlslDeclString() const { return "const vec3";}
    template<> inline std::string  SGVec4Const::GlslDeclString() const { return "const vec4";}
#ifdef TGF_DEBUG
    template<>  inline std::string  SGFloatConst::TgfLabel() const { return "SGFloatConst";}
    template<>  inline std::string  SGVec2Const::TgfLabel() const  { return "SGVec2Const";}
    template<>  inline std::string  SGVec3Const::TgfLabel() const  { return "SGVec3Const";}
    template<>  inline std::string  SGVec4Const::TgfLabel() const  { return "SGVec4Const";}
#endif

    typedef SGVectorConst<1, SGInt    , int>   SGIntConst;
    typedef SGVectorConst<2, SGIvec2  , int>   SGIvec2Const;
    typedef SGVectorConst<3, SGIvec3  , int>   SGIvec3Const;
    typedef SGVectorConst<4, SGIvec4  , int>   SGIvec4Const;
    template<> inline std::string   SGIntConst::GlslDeclString() const { return "const int";}
    template<> inline std::string SGIvec2Const::GlslDeclString() const { return "const ivec2";}
    template<> inline std::string SGIvec3Const::GlslDeclString() const { return "const ivec3";}
    template<> inline std::string SGIvec4Const::GlslDeclString() const { return "const ivec4";}
#ifdef TGF_DEBUG
    template<> inline std::string    SGIntConst::TgfLabel() const { return "SGIntConst";}
    template<> inline std::string SGIvec2Const::TgfLabel() const  { return "SGIvec2Const";}
    template<> inline std::string SGIvec3Const::TgfLabel() const  { return "SGIvec3Const";}
    template<> inline std::string SGIvec4Const::TgfLabel() const  { return "SGIvec4Const";}
#endif

    typedef SGVectorConst<1, SGBool, bool>   SGBoolConst;
    typedef SGVectorConst<2, SGBool, bool>   SGBvec2Const;
    typedef SGVectorConst<3, SGBool, bool>   SGBvec3Const;
    typedef SGVectorConst<4, SGBool, bool>   SGBvec4Const;
    template<> inline std::string SGBoolConst::GlslDeclString() const { return "const bool";}
    template<> inline std::string SGBvec2Const::GlslDeclString() const { return "const bvec2";}
    template<> inline std::string SGBvec3Const::GlslDeclString() const { return "const bvec3";}
    template<> inline std::string SGBvec4Const::GlslDeclString() const { return "const bvec4";}
#ifdef TGF_DEBUG
    template<> inline std::string SGBoolConst ::TgfLabel() const { return "SGBoolConst";}
    template<> inline std::string SGBvec2Const::TgfLabel() const  { return "SGBvec2Const";}
    template<> inline std::string SGBvec3Const::TgfLabel() const  { return "SGBvec3Const";}
    template<> inline std::string SGBvec4Const::TgfLabel() const  { return "SGBvec4Const";}
#endif

    template<typename T>
    concept genType =
    std::derived_from<T, SGFloat>			||
    std::derived_from<T, SGVec2>			||
    std::derived_from<T, SGVec3>			||
    std::derived_from<T, SGVec4>;

    template<typename T>
    concept genIType =
    std::derived_from<T, SGInt>			    ||
    std::derived_from<T, SGIvec2>			||
    std::derived_from<T, SGIvec3>			||
    std::derived_from<T, SGIvec4>;

    template<typename T>
    concept genBType =
    std::derived_from<T, SGBool>			||
    std::derived_from<T, SGBvec2>			||
    std::derived_from<T, SGBvec3>			||
    std::derived_from<T, SGBvec4>;


    template<typename OutType, typename... InTypes> requires std::derived_from<OutType, SGNode> && ((std::derived_from<InTypes, SGNode>),...)
    class SGOp : public OutType
    {
    public:
        SGOp(const std::shared_ptr<InTypes>&...in)
        {
            int i=0;
            ((_args[i++] = in),...);
        }

        inline virtual int ChildrenCount()  const override {return sizeof...(InTypes);}
        inline virtual SGNode& ChildAt(int i) const override {return *_args[i];}

    protected:
        std::shared_ptr<SGNode> _args[sizeof...(InTypes)];
    };

#define SG_MAKE_OP( __class_name__, __glsl_string__, __out_type__, ... ) \
    class __class_name__ : public SGOp<__out_type__, __VA_ARGS__>            \
    {                                                                        \
    public:                                                                  \
        using SGOp<__out_type__, __VA_ARGS__>::SGOp;                         \
    inline virtual std::string TgfLabel() const override {return #__class_name__; };\
    protected:                                                               \
    inline std::string GlslStringProtected() const override final                 \
    {                                                                        \
             return __glsl_string__;                                         \
    }                                                                        \
};                                                                           \

#define SG_MAKE_SWIZZLE(__class_name__, __glsl_string__, __out_type__ , __in_generic_type__,  __required_components__ ) \
    template<typename InGenType>                                                                                        \
        requires (  __in_generic_type__<InGenType> &&                                                                   \
                    std::derived_from< __out_type__ , SGNode > &&                                                       \
                    InGenType::num_components >= __required_components__ )                                              \
    class __class_name__ : public SGOp<__out_type__, InGenType>                                                         \
    {                                                                                                                   \
    public:                                                                                                             \
        using SGOp<__out_type__, InGenType>::SGOp;                                                                      \
        inline virtual std::string TgfLabel() const override { return #__class_name__ ; };                                     \
    protected:                                                                                                          \
        inline std::string GlslStringProtected() const override final                                                        \
        {                                                                                                               \
            return std::format( __glsl_string__ , SGOp<__out_type__, InGenType>::_args[0]->GlslString());               \
        }                                                                                                               \
    };                                                                                                                  \


#define SG_MAKE_GENERIC_UN_OP(__class_name__, __glsl_string__, __generic_type__) \
    template<__generic_type__ GenType>                              \
    class __class_name__ : public SGOp<GenType, GenType>                                             \
    {                                                                                                     \
    public:                                                                                               \
        using SGOp<GenType, GenType>::SGOp;                                                          \
        inline virtual std::string TgfLabel() const override { return #__class_name__ ; };                       \
    protected:                                                                                            \
        inline std::string GlslStringProtected() const override final                                            \
        {                                                                                                 \
            return std::format( __glsl_string__ , SGOp<GenType, GenType>::_args[0]->GlslString());   \
        }                                                                                                 \
    };                                                                                                    \

#define SG_MAKE_GENERIC_BIN_OP(__class_name__, __glsl_string__, __out_generic_type__, __in_generic_type__) \
    template<__out_generic_type__ OutGenType, __in_generic_type__ InGenType>                                                    \
        requires  (InGenType::num_components == OutGenType::num_components)                                                     \
    class __class_name__:                                                                                                       \
    public SGOp<OutGenType, InGenType, InGenType>                                                                               \
    {                                                                                                                           \
    public:                                                                                                                     \
        using SGOp<OutGenType, InGenType, InGenType>::SGOp;                                                                     \
        inline virtual std::string TgfLabel() const override { return #__class_name__ ; };                                             \
    protected:                                                                                                                  \
        inline std::string GlslStringProtected() const override final                                             \
        {                                                                                                                       \
            return std::format( __glsl_string__ ,                                                                               \
                    SGOp<OutGenType, InGenType, InGenType>::_args[0]->GlslString(),                                             \
                    SGOp<OutGenType, InGenType, InGenType>::_args[1]->GlslString());                                            \
        }                                                                                                                       \
    };                                                                                                                          \


#define SG_MAKE_GENERIC_TERN_OP(__class_name__, __glsl_string__, __generic_type__) \
    template<__generic_type__ GenT>                                     \
    class __class_name__ : public SGOp<GenT, GenT, GenT, GenT>                     \
    {                                                                              \
    public:                                                                        \
        using SGOp<GenT, GenT, GenT, GenT>::SGOp;                                  \
        inline virtual std::string TgfLabel() const override { return #__class_name__ ; };\
    protected:                                                                     \
        inline std::string GlslStringProtected() const override final                   \
        {                                                                          \
            return std::format( __glsl_string__ ,                                  \
            SGOp<GenT, GenT, GenT, GenT>::_args[0]->GlslString(),                  \
            SGOp<GenT, GenT, GenT, GenT>::_args[1]->GlslString(),                  \
            SGOp<GenT, GenT, GenT, GenT>::_args[2]->GlslString()                   \
            );                                                                     \
        }                                                                          \
    };                                                                             \

#define SG_MAKE_GENERIC_TERN_OP_2(__class_name__, __glsl_string__, __generic_type__, __in_type_0__) \
    template<__generic_type__ GenT>                                                                 \
    class __class_name__ : public SGOp<GenT, __in_type_0__ , GenT, GenT>                            \
    {                                                                                               \
    public:                                                                                         \
        using SGOp<GenT, __in_type_0__ , GenT, GenT>::SGOp;                                         \
        inline virtual std::string TgfLabel() const override { return #__class_name__ ; };                 \
    protected:                                                                                      \
        inline std::string GlslStringProtected() const override final                                    \
        {                                                                                           \
            return std::format( __glsl_string__ ,                                                   \
            SGOp<GenT, __in_type_0__ , GenT, GenT>::_args[0]->GlslString(),                         \
            SGOp<GenT, __in_type_0__ , GenT, GenT>::_args[1]->GlslString(),                         \
            SGOp<GenT, __in_type_0__ , GenT, GenT>::_args[2]->GlslString()                          \
            );                                                                                      \
        }                                                                                           \
    };

    /// NOTE
    /// SG_MAKE_OP( class_name, glsl string (use _args[0...N]), output_type, input_types[0...N])
    /// ----------------------------------------------------------------------------------------------------------------------------------------------

    /// Unary Operators
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_GENERIC_UN_OP(SGCos, "cos( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGSin, "sin( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGNormalize, "normalize( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGAbsf, "abs( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGAbsi, "abs( {} )", genIType)
    SG_MAKE_GENERIC_UN_OP(SGFWidthf, "fwidth( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGFractf, "fract( {} )", genType)
    SG_MAKE_GENERIC_UN_OP(SGFloorf, "floor( {} )", genType)

    SG_MAKE_OP(SGLength1, std::format("length( {} )", _args[0]->GlslString()), SGFloat, SGFloat)
    SG_MAKE_OP(SGLength2, std::format("length( {} )", _args[0]->GlslString()), SGFloat, SGVec2)
    SG_MAKE_OP(SGLength3, std::format("length( {} )", _args[0]->GlslString()), SGFloat, SGVec3)
    SG_MAKE_OP(SGLength4, std::format("length( {} )", _args[0]->GlslString()), SGFloat, SGVec4)

    SG_MAKE_OP(SGAny1, std::format("any( {} )", _args[0]->GlslString()), SGBool, SGBool)
    SG_MAKE_OP(SGAny2, std::format("any( {} )", _args[0]->GlslString()), SGBool, SGBvec2)
    SG_MAKE_OP(SGAny3, std::format("any( {} )", _args[0]->GlslString()), SGBool, SGBvec3)
    SG_MAKE_OP(SGAny4, std::format("any( {} )", _args[0]->GlslString()), SGBool, SGBvec4)

    /// Binary operators
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_GENERIC_BIN_OP(SGAddf, "( ( {} ) + ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGAddi, "( ( {} ) + ( {} ) )", genIType, genIType)
    SG_MAKE_GENERIC_BIN_OP(SGSubtractf, "( ( {} ) - ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGSubtracti, "( ( {} ) - ( {} ) )", genIType, genIType)
    SG_MAKE_GENERIC_BIN_OP(SGMultiplyf, "( ( {} ) * ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGMultiplyi, "( ( {} ) * ( {} ) )", genIType, genIType)
    SG_MAKE_GENERIC_BIN_OP(SGDividef, "( ( {} ) / ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGDividei, "( ( {} ) / ( {} ) )", genIType, genIType)
    SG_MAKE_GENERIC_BIN_OP(SGPowf, "pow( ( {} ) , ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGMod, "mod( ( {} ) , ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGCross, "cross( ( {} ) , ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGMinf, "min( ( {} ) , ( {} ) )", genType, genType)
    SG_MAKE_GENERIC_BIN_OP(SGMaxf, "max( ( {} ) , ( {} ) )", genType, genType)


    SG_MAKE_OP(SGLessf      , std::format("( ( {} ) <  ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGFloat, SGFloat)
    SG_MAKE_OP(SGLEqualf    , std::format("( ( {} ) <= ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGFloat, SGFloat)
    SG_MAKE_OP(SGEqualf     , std::format("( ( {} ) == ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGFloat, SGFloat)
    SG_MAKE_OP(SGGreaterf   , std::format("( ( {} ) >  ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGFloat, SGFloat)
    SG_MAKE_OP(SGGEqualf    , std::format("( ( {} ) >= ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGFloat, SGFloat)
    SG_MAKE_OP(SGLessi      , std::format("( ( {} ) <  ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGInt, SGInt)
    SG_MAKE_OP(SGLEquali    , std::format("( ( {} ) <= ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGInt, SGInt)
    SG_MAKE_OP(SGEquali     , std::format("( ( {} ) == ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGInt, SGInt)
    SG_MAKE_OP(SGGreateri   , std::format("( ( {} ) >  ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGInt, SGInt)
    SG_MAKE_OP(SGGEquali    , std::format("( ( {} ) >= ( {} ) )", _args[0]->GlslString(), _args[1]->GlslString()), SGBool, SGInt, SGInt)

    SG_MAKE_OP(SGDot1, std::format("dot( {}, {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGFloat, SGFloat, SGFloat)
    SG_MAKE_OP(SGDot2, std::format("dot( {}, {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGFloat, SGVec2 , SGVec2)
    SG_MAKE_OP(SGDot3, std::format("dot( {}, {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGFloat, SGVec3, SGVec3)
    SG_MAKE_OP(SGDot4, std::format("dot( {}, {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGFloat, SGVec4, SGVec4)

    SG_MAKE_OP(SGMultiplymm, std::format("( {} * {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGMat4, SGMat4, SGMat4)
    SG_MAKE_OP(SGMultiplymv, std::format("( {} * {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGVec4, SGMat4, SGVec4)
    SG_MAKE_OP(SGMultiplysv2, std::format("( {} * {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGVec2, SGFloat, SGVec2)
    SG_MAKE_OP(SGMultiplysv3, std::format("( {} * {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGVec3, SGFloat, SGVec3)
    SG_MAKE_OP(SGMultiplysv4, std::format("( {} * {} )", _args[0]->GlslString(), _args[1]->GlslString()), SGVec4, SGFloat, SGVec4)

    /// Ternary operators
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_GENERIC_TERN_OP(SGClampf    , "( clamp( ( {} ) , ( {} ), ( {} ) ) )", genType)
    SG_MAKE_GENERIC_TERN_OP(SGClampi    , "( clamp( ( {} ) , ( {} ), ( {} ) ) )", genIType)

    SG_MAKE_OP(SGMix1f, std::format("mix( {}, {}, {} )", _args[0]->GlslString(), _args[1]->GlslString(), _args[2]->GlslString()), SGFloat, SGFloat, SGFloat, SGFloat)
    SG_MAKE_OP(SGMix2f, std::format("mix( {}, {}, {} )", _args[0]->GlslString(), _args[1]->GlslString(), _args[2]->GlslString()), SGVec2 , SGVec2 , SGVec2 , SGFloat)
    SG_MAKE_OP(SGMix3f, std::format("mix( {}, {}, {} )", _args[0]->GlslString(), _args[1]->GlslString(), _args[2]->GlslString()), SGVec3 , SGVec3 , SGVec3 , SGFloat)
    SG_MAKE_OP(SGMix4f, std::format("mix( {}, {}, {} )", _args[0]->GlslString(), _args[1]->GlslString(), _args[2]->GlslString()), SGVec4 , SGVec4 , SGVec4 , SGFloat)

    /// Branch
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_GENERIC_TERN_OP_2(SGBranchf , " ( {} ) ? ( {} ) : ( {} ) ", genType, SGBool)
    SG_MAKE_GENERIC_TERN_OP_2(SGBranchi , " ( {} ) ? ( {} ) : ( {} ) ", genIType, SGBool)
    SG_MAKE_GENERIC_TERN_OP_2(SGBranchb , " ( {} ) ? ( {} ) : ( {} ) ", genBType, SGBool)

    /// Vector constructor
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_OP(SGMakeVec2  , std::format("vec2( ( {} ), ( {} ) )"                       , _args[0]->GlslString(),_args[1]->GlslString())                                                         , SGVec2, SGFloat, SGFloat)
    SG_MAKE_OP(SGMakeVec3  , std::format("vec3( ( {} ), ( {} ), ( {} ) )"               , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString())                              , SGVec3, SGFloat, SGFloat, SGFloat)
    SG_MAKE_OP(SGMakeVec4  , std::format("vec4( ( {} ), ( {} ), ( {} ), ( {} ) )"       , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString() ,_args[3]->GlslString())   , SGVec4, SGFloat, SGFloat, SGFloat, SGFloat)
    SG_MAKE_OP(SGMakeIvec2  , std::format("ivec2( ( {} ), ( {} ) )"                     , _args[0]->GlslString(),_args[1]->GlslString())                                                         , SGIvec2 , SGInt, SGInt)
    SG_MAKE_OP(SGMakeIvec3  , std::format("ivec3( ( {} ), ( {} ), ( {} ) )"             , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString())                              , SGIvec3, SGInt, SGInt, SGInt)
    SG_MAKE_OP(SGMakeIvec4  , std::format("ivec4( ( {} ), ( {} ), ( {} ), ( {} ) )"     , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString() ,_args[3]->GlslString())   , SGIvec4, SGInt, SGInt, SGInt, SGInt)
    SG_MAKE_OP(SGMakeBvec2  , std::format("bvec2( ( {} ), ( {} ) )"                     , _args[0]->GlslString(),_args[1]->GlslString())                                                         , SGBvec2 , SGBool, SGBool)
    SG_MAKE_OP(SGMakeBvec3  , std::format("bvec3( ( {} ), ( {} ), ( {} ) )"             , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString())                              , SGBvec3, SGBool, SGBool, SGBool)
    SG_MAKE_OP(SGMakeBvec4  , std::format("bvec4( ( {} ), ( {} ), ( {} ), ( {} ) )"     , _args[0]->GlslString(),_args[1]->GlslString() ,_args[2]->GlslString() ,_args[3]->GlslString())   , SGBvec4, SGBool, SGBool, SGBool, SGBool)

    /// Vector swizzle
    /// ----------------------------------------------------------------------------------------------------------------------------------------------
    SG_MAKE_SWIZZLE(SGSwizzleXf, "({}).x", SGFloat, genType, 1)
    SG_MAKE_SWIZZLE(SGSwizzleXi, "({}).x", SGFloat, genIType, 1)
    SG_MAKE_SWIZZLE(SGSwizzleXb, "({}).x", SGFloat, genBType, 1)
    SG_MAKE_SWIZZLE(SGSwizzleYf, "({}).y", SGFloat, genType, 2)
    SG_MAKE_SWIZZLE(SGSwizzleYi, "({}).y", SGFloat, genIType, 2)
    SG_MAKE_SWIZZLE(SGSwizzleYb, "({}).y", SGFloat, genBType, 2)
    SG_MAKE_SWIZZLE(SGSwizzleZf, "({}).z", SGFloat, genType, 3)
    SG_MAKE_SWIZZLE(SGSwizzleZi, "({}).z", SGFloat, genIType, 3)
    SG_MAKE_SWIZZLE(SGSwizzleZb, "({}).z", SGFloat, genBType, 3)
    SG_MAKE_SWIZZLE(SGSwizzleWf, "({}).w", SGFloat, genType, 4)
    SG_MAKE_SWIZZLE(SGSwizzleWi, "({}).w", SGFloat, genIType, 4)
    SG_MAKE_SWIZZLE(SGSwizzleWb, "({}).w", SGFloat, genBType, 4)

    void TraverseShaderGraph(SGNode& root, std::function<bool(SGNode&)> func);

    void TraverseShaderGraph(SGNode& root, std::function<bool(SGNode&, int)> func, int stepsFromRoot = 0);

    void TraverseShaderGraph(SGNode& root, std::function<bool(SGNode&, int, std::optional<SGNode*>)> func, int stepsFromRoot = 0, std::optional<SGNode*> prevNode = std::nullopt);

    bool IsInputNode(SGNode& node);

    std::string ParseShaderGraph(SGNode& outNode);

#ifdef TGF_DEBUG
    void ParseShaderGraphTGF(SGNode& outNode, const char* fileName);
#endif
}
#endif //TAOGL_SHADERGRAPH_H
