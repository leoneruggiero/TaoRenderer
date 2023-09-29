
#ifndef TAOGL_SHADERGRAPHUTILS_H
#define TAOGL_SHADERGRAPHUTILS_H

#include "ShaderGraph.h"

namespace tao_gizmos_shader_graph
{
    std::shared_ptr<SGVec2> inline SGVec(std::shared_ptr<SGFloat> x, std::shared_ptr<SGFloat> y){return std::shared_ptr<SGVec2>{new SGMakeVec2{x, y}};}
    std::shared_ptr<SGVec3> inline SGVec(std::shared_ptr<SGFloat> x, std::shared_ptr<SGFloat> y, std::shared_ptr<SGFloat> z){return std::shared_ptr<SGVec3>{new SGMakeVec3{x, y, z}};}
    std::shared_ptr<SGVec4> inline SGVec(std::shared_ptr<SGFloat> x, std::shared_ptr<SGFloat> y, std::shared_ptr<SGFloat> z, std::shared_ptr<SGFloat> w){return std::shared_ptr<SGVec4>{new SGMakeVec4{x, y, z, w}};}

    std::shared_ptr<SGFloat> inline SGConstVec(float x){return std::shared_ptr<SGFloat>{new SGFloatConst{x}};}
    std::shared_ptr<SGVec2>  inline SGConstVec(float x ,float y){return std::shared_ptr<SGVec2>{new SGVec2Const{x, y}};}
    std::shared_ptr<SGVec3>  inline SGConstVec(float x, float y, float z){return std::shared_ptr<SGVec3>{new SGVec3Const{x, y, z}};}
    std::shared_ptr<SGVec4>  inline SGConstVec(float x, float y, float z, float w){return std::shared_ptr<SGVec4>{new SGVec4Const{x, y, z, w}};}

    std::shared_ptr<SGFloat> inline SGSwizzleX(std::shared_ptr<SGVec2> v){return std::shared_ptr<SGFloat>(new SGSwizzleXf<SGVec2>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleX(std::shared_ptr<SGVec3> v){return std::shared_ptr<SGFloat>(new SGSwizzleXf<SGVec3>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleX(std::shared_ptr<SGVec4> v){return std::shared_ptr<SGFloat>(new SGSwizzleXf<SGVec4>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleY(std::shared_ptr<SGVec2> v){return std::shared_ptr<SGFloat>(new SGSwizzleYf<SGVec2>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleY(std::shared_ptr<SGVec3> v){return std::shared_ptr<SGFloat>(new SGSwizzleYf<SGVec3>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleY(std::shared_ptr<SGVec4> v){return std::shared_ptr<SGFloat>(new SGSwizzleYf<SGVec4>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleZ(std::shared_ptr<SGVec3> v){return std::shared_ptr<SGFloat>(new SGSwizzleZf<SGVec3>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleZ(std::shared_ptr<SGVec4> v){return std::shared_ptr<SGFloat>(new SGSwizzleZf<SGVec4>{v});}
    std::shared_ptr<SGFloat> inline SGSwizzleW(std::shared_ptr<SGVec4> v){return std::shared_ptr<SGFloat>(new SGSwizzleWf<SGVec4>{v});}

    std::shared_ptr<SGFloat> inline SGAdd(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) { return std::shared_ptr<SGFloat>( new SGAddf<SGFloat, SGFloat>{a, b} );}
    std::shared_ptr<SGVec2>  inline SGAdd(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGVec2>( new SGAddf<SGVec2, SGVec2>{a, b} );}
    std::shared_ptr<SGVec3>  inline SGAdd(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGVec3>( new SGAddf<SGVec3, SGVec3>{a, b} );}
    std::shared_ptr<SGVec4>  inline SGAdd(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGAddf<SGVec4, SGVec4>{a, b} );}

    std::shared_ptr<SGFloat> inline SGSubtract(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) { return std::shared_ptr<SGFloat>( new SGSubtractf<SGFloat, SGFloat>{a, b} );}
    std::shared_ptr<SGVec2>  inline SGSubtract(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGVec2>( new SGSubtractf<SGVec2, SGVec2>{a, b} );}
    std::shared_ptr<SGVec3>  inline SGSubtract(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGVec3>( new SGSubtractf<SGVec3, SGVec3>{a, b} );}
    std::shared_ptr<SGVec4>  inline SGSubtract(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGSubtractf<SGVec4, SGVec4>{a, b} );}

    std::shared_ptr<SGFloat> inline SGMultiply(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) { return std::shared_ptr<SGFloat>( new SGMultiplyf<SGFloat, SGFloat>{a, b} );}
    std::shared_ptr<SGVec2>  inline SGMultiply(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGVec2>( new SGMultiplyf<SGVec2, SGVec2>{a, b} );}
    std::shared_ptr<SGVec3>  inline SGMultiply(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGVec3>( new SGMultiplyf<SGVec3, SGVec3>{a, b} );}
    std::shared_ptr<SGVec4>  inline SGMultiply(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGMultiplyf<SGVec4, SGVec4>{a, b} );}
    std::shared_ptr<SGMat4>  inline SGMultiply(std::shared_ptr<SGMat4> a, std::shared_ptr<SGMat4> b) { return std::shared_ptr<SGMat4>( new SGMultiplymm{a, b} );}
    std::shared_ptr<SGVec4>  inline SGMultiply(std::shared_ptr<SGMat4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGMultiplymv{a, b} );}
    std::shared_ptr<SGVec2>  inline SGMultiply(std::shared_ptr<SGFloat> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGVec2>( new SGMultiplysv2{a, b} );}
    std::shared_ptr<SGVec3>  inline SGMultiply(std::shared_ptr<SGFloat> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGVec3>( new SGMultiplysv3{a, b} );}
    std::shared_ptr<SGVec4>  inline SGMultiply(std::shared_ptr<SGFloat> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGMultiplysv4{a, b} );}

    std::shared_ptr<SGFloat> inline SGDivide(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGFloat>( new SGDividef<SGFloat, SGFloat>{a, b} );}
    std::shared_ptr<SGVec2>  inline SGDivide(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)     { return std::shared_ptr<SGVec2> ( new SGDividef<SGVec2, SGVec2>{a, b} );}
    std::shared_ptr<SGVec3>  inline SGDivide(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)     { return std::shared_ptr<SGVec3> ( new SGDividef<SGVec3, SGVec3>{a, b} );}
    std::shared_ptr<SGVec4>  inline SGDivide(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)     { return std::shared_ptr<SGVec4> ( new SGDividef<SGVec4, SGVec4>{a, b} );}

    std::shared_ptr<SGFloat> inline operator+(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)  {return SGAdd(a, b);}
    std::shared_ptr<SGVec2>  inline operator+(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return SGAdd(a, b);}
    std::shared_ptr<SGVec3>  inline operator+(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return SGAdd(a, b);}
    std::shared_ptr<SGVec4>  inline operator+(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return SGAdd(a, b);}

    std::shared_ptr<SGFloat> inline operator-(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)  {return SGSubtract(a, b);}
    std::shared_ptr<SGVec2>  inline operator-(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return SGSubtract(a, b);}
    std::shared_ptr<SGVec3>  inline operator-(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return SGSubtract(a, b);}
    std::shared_ptr<SGVec4>  inline operator-(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return SGSubtract(a, b);}

    std::shared_ptr<SGFloat> inline operator*(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)  {return SGMultiply(a, b);}
    std::shared_ptr<SGVec2>  inline operator*(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return SGMultiply(a, b);}
    std::shared_ptr<SGVec3>  inline operator*(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return SGMultiply(a, b);}
    std::shared_ptr<SGVec4>  inline operator*(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return SGMultiply(a, b);}
    std::shared_ptr<SGMat4>  inline operator*(std::shared_ptr<SGMat4> a, std::shared_ptr<SGMat4> b)    {return SGMultiply(a, b);}
    std::shared_ptr<SGVec4>  inline operator*(std::shared_ptr<SGMat4> a, std::shared_ptr<SGVec4> b)    {return SGMultiply(a, b);}

    std::shared_ptr<SGFloat> inline operator/(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)  {return SGDivide(a, b);}
    std::shared_ptr<SGVec2>  inline operator/(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return SGDivide(a, b);}
    std::shared_ptr<SGVec3>  inline operator/(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return SGDivide(a, b);}
    std::shared_ptr<SGVec4>  inline operator/(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return SGDivide(a, b);}

    std::shared_ptr<SGBool>  inline SGAny(std::shared_ptr<SGBool> a)    { return std::shared_ptr<SGBool> ( new SGAny1{a} );}
    std::shared_ptr<SGBool>  inline SGAny(std::shared_ptr<SGBvec2> a)   { return std::shared_ptr<SGBool> ( new SGAny2{a} );}
    std::shared_ptr<SGBool>  inline SGAny(std::shared_ptr<SGBvec3> a)   { return std::shared_ptr<SGBool> ( new SGAny3{a} );}
    std::shared_ptr<SGBool>  inline SGAny(std::shared_ptr<SGBvec4> a)   { return std::shared_ptr<SGBool> ( new SGAny4{a} );}

    std::shared_ptr<SGBool>  inline SGGreater       (std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGBool> ( new SGGreaterf{a, b} );}
    std::shared_ptr<SGBool>  inline SGGreaterEqual  (std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGBool> ( new SGGEqualf{a, b} );}
    std::shared_ptr<SGBool>  inline SGLess          (std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGBool> ( new SGLessf{a, b} );}
    std::shared_ptr<SGBool>  inline SGLessEqual     (std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGBool> ( new SGLEqualf{a, b} );}
    std::shared_ptr<SGBool>  inline SGEqual         (std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)   { return std::shared_ptr<SGBool> ( new SGEqualf{a, b} );}

    std::shared_ptr<SGFloat>  inline SGDot(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) { return std::shared_ptr<SGFloat>( new SGDot1{a, b} );}
    std::shared_ptr<SGFloat>  inline SGDot(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGFloat>( new SGDot2{a, b});}
    std::shared_ptr<SGFloat>  inline SGDot(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGFloat>( new SGDot3{a, b});}
    std::shared_ptr<SGFloat>  inline SGDot(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGFloat>( new SGDot4{a, b});}

    std::shared_ptr<SGFloat>  inline SGClamp(std::shared_ptr<SGFloat> x, std::shared_ptr<SGFloat> min, std::shared_ptr<SGFloat> max) { return std::shared_ptr<SGFloat>( new SGClampf<SGFloat>{x, min, max} );}

    std::shared_ptr<SGFloat>  inline SGPow(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) { return std::shared_ptr<SGFloat>( new SGPowf<SGFloat, SGFloat>{a, b} );}
    std::shared_ptr<SGVec2>  inline SGPow(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b) { return std::shared_ptr<SGVec2>( new SGPowf<SGVec2, SGVec2>{a, b} );}
    std::shared_ptr<SGVec3>  inline SGPow(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b) { return std::shared_ptr<SGVec3>( new SGPowf<SGVec3, SGVec3>{a, b} );}
    std::shared_ptr<SGVec4>  inline SGPow(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b) { return std::shared_ptr<SGVec4>( new SGPowf<SGVec4, SGVec4>{a, b} );}

    std::shared_ptr<SGFloat> inline SGBranch(std::shared_ptr<SGBool> c, std::shared_ptr<SGFloat> t, std::shared_ptr<SGFloat> f){return std::shared_ptr<SGFloat>(new SGBranchf<SGFloat>{c, t, f});}
    std::shared_ptr<SGVec2>  inline SGBranch(std::shared_ptr<SGBool> c, std::shared_ptr<SGVec2> t, std::shared_ptr<SGVec2> f){return std::shared_ptr<SGVec2>(new SGBranchf<SGVec2>{c, t, f});}
    std::shared_ptr<SGVec3>  inline SGBranch(std::shared_ptr<SGBool> c, std::shared_ptr<SGVec3> t, std::shared_ptr<SGVec3> f){return std::shared_ptr<SGVec3>(new SGBranchf<SGVec3>{c, t, f});}
    std::shared_ptr<SGVec4>  inline SGBranch(std::shared_ptr<SGBool> c, std::shared_ptr<SGVec4> t, std::shared_ptr<SGVec4> f){return std::shared_ptr<SGVec4>(new SGBranchf<SGVec4>{c, t, f});}

    std::shared_ptr<SGFloat> inline SGNorm(std::shared_ptr<SGFloat> a){return std::shared_ptr<SGFloat>(new SGNormalize<SGFloat>(a));}
    std::shared_ptr<SGVec2>  inline SGNorm(std::shared_ptr<SGVec2> a){return std::shared_ptr<SGVec2>(new SGNormalize<SGVec2>(a));}
    std::shared_ptr<SGVec3>  inline SGNorm(std::shared_ptr<SGVec3> a){return std::shared_ptr<SGVec3>(new SGNormalize<SGVec3>(a));}
    std::shared_ptr<SGVec4>  inline SGNorm(std::shared_ptr<SGVec4> a){return std::shared_ptr<SGVec4>(new SGNormalize<SGVec4>(a));}

    std::shared_ptr<SGFloat> inline SGMix(std::shared_ptr<SGFloat> x, std::shared_ptr<SGFloat> y, std::shared_ptr<SGFloat> a) {return std::shared_ptr<SGFloat>(new SGMix1f(x, y, a));}
    std::shared_ptr<SGVec2>  inline SGMix(std::shared_ptr<SGVec2> x, std::shared_ptr<SGVec2> y, std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGVec2>(new SGMix2f(x, y, a));}
    std::shared_ptr<SGVec3>  inline SGMix(std::shared_ptr<SGVec3> x, std::shared_ptr<SGVec3> y, std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGVec3>(new SGMix3f(x, y, a));}
    std::shared_ptr<SGVec4>  inline SGMix(std::shared_ptr<SGVec4> x, std::shared_ptr<SGVec4> y, std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGVec4>(new SGMix4f(x, y, a));}

    std::shared_ptr<SGFloat> inline SGMin(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)    {return std::shared_ptr<SGFloat>(new SGMinf<SGFloat, SGFloat>(a,b));}
    std::shared_ptr<SGVec2>  inline SGMin(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return std::shared_ptr<SGVec2>(new SGMinf<SGVec2, SGVec2>(a,b));}
    std::shared_ptr<SGVec3>  inline SGMin(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return std::shared_ptr<SGVec3>(new SGMinf<SGVec3, SGVec3>(a,b));}
    std::shared_ptr<SGVec4>  inline SGMin(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return std::shared_ptr<SGVec4>(new SGMinf<SGVec4, SGVec4>(a,b));}

    std::shared_ptr<SGFloat> inline SGMax(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b)    {return std::shared_ptr<SGFloat>(new SGMaxf<SGFloat, SGFloat>(a,b));}
    std::shared_ptr<SGVec2>  inline SGMax(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return std::shared_ptr<SGVec2>(new SGMaxf<SGVec2, SGVec2>(a,b));}
    std::shared_ptr<SGVec3>  inline SGMax(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return std::shared_ptr<SGVec3>(new SGMaxf<SGVec3, SGVec3>(a,b));}
    std::shared_ptr<SGVec4>  inline SGMax(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return std::shared_ptr<SGVec4>(new SGMaxf<SGVec4, SGVec4>(a,b));}

    std::shared_ptr<SGFloat> inline SGFWidth(std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGFloat>(new SGFWidthf<SGFloat>(a));}
    std::shared_ptr<SGVec2>  inline SGFWidth(std::shared_ptr<SGVec2> a)     {return std::shared_ptr<SGVec2>(new SGFWidthf<SGVec2>(a));}
    std::shared_ptr<SGVec3>  inline SGFWidth(std::shared_ptr<SGVec3> a)     {return std::shared_ptr<SGVec3>(new SGFWidthf<SGVec3>(a));}
    std::shared_ptr<SGVec4>  inline SGFWidth(std::shared_ptr<SGVec4> a)     {return std::shared_ptr<SGVec4>(new SGFWidthf<SGVec4>(a));}

    std::shared_ptr<SGFloat> inline SGFract(std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGFloat>(new SGFractf<SGFloat>(a));}
    std::shared_ptr<SGVec2>  inline SGFract(std::shared_ptr<SGVec2> a)     {return std::shared_ptr<SGVec2>( new SGFractf<SGVec2>(a));}
    std::shared_ptr<SGVec4>  inline SGFract(std::shared_ptr<SGVec4> a)     {return std::shared_ptr<SGVec4>( new SGFractf<SGVec4>(a));}
    std::shared_ptr<SGVec3>  inline SGFract(std::shared_ptr<SGVec3> a)     {return std::shared_ptr<SGVec3>( new SGFractf<SGVec3>(a));}

    std::shared_ptr<SGFloat> inline SGModulo(std::shared_ptr<SGFloat> a, std::shared_ptr<SGFloat> b) {return std::shared_ptr<SGFloat>(new SGMod<SGFloat, SGFloat>(a, b));}
    std::shared_ptr<SGVec2> inline SGModulo(std::shared_ptr<SGVec2> a, std::shared_ptr<SGVec2> b)    {return std::shared_ptr<SGVec2> (new SGMod<SGVec2, SGVec2>(a, b));}
    std::shared_ptr<SGVec3> inline SGModulo(std::shared_ptr<SGVec3> a, std::shared_ptr<SGVec3> b)    {return std::shared_ptr<SGVec3> (new SGMod<SGVec3, SGVec3>(a, b));}
    std::shared_ptr<SGVec4> inline SGModulo(std::shared_ptr<SGVec4> a, std::shared_ptr<SGVec4> b)    {return std::shared_ptr<SGVec4> (new SGMod<SGVec4, SGVec4>(a, b));}

    std::shared_ptr<SGFloat> inline SGFloor(std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGFloat>(new SGFloorf<SGFloat>(a));}
    std::shared_ptr<SGVec2>  inline SGFloor(std::shared_ptr<SGVec2> a)     {return std::shared_ptr<SGVec2>( new SGFloorf<SGVec2>(a));}
    std::shared_ptr<SGVec4>  inline SGFloor(std::shared_ptr<SGVec4> a)     {return std::shared_ptr<SGVec4>( new SGFloorf<SGVec4>(a));}
    std::shared_ptr<SGVec3>  inline SGFloor(std::shared_ptr<SGVec3> a)     {return std::shared_ptr<SGVec3>( new SGFloorf<SGVec3>(a));}

    std::shared_ptr<SGFloat> inline SGAbs(std::shared_ptr<SGFloat> a)    {return std::shared_ptr<SGFloat>(new SGAbsf<SGFloat>(a));}
    std::shared_ptr<SGVec2>  inline SGAbs(std::shared_ptr<SGVec2> a)     {return std::shared_ptr<SGVec2>( new SGAbsf<SGVec2>(a));}
    std::shared_ptr<SGVec4>  inline SGAbs(std::shared_ptr<SGVec4> a)     {return std::shared_ptr<SGVec4>( new SGAbsf<SGVec4>(a));}
    std::shared_ptr<SGVec3>  inline SGAbs(std::shared_ptr<SGVec3> a)     {return std::shared_ptr<SGVec3>( new SGAbsf<SGVec3>(a));}
}

#endif //TAOGL_SHADERGRAPHUTILS_H
