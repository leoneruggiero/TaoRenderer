
#ifndef TAOGL_SHADERGRAPHIONODES_H
#define TAOGL_SHADERGRAPHIONODES_H

#include "ShaderGraph.h"
#include "ShaderGraphUtils.h"

namespace tao_gizmos_shader_graph
{
    class SGOutMeshGizmo : public SGNode
    {

    };

    class SGOutColor : public SGOutMeshGizmo
    {
    public:
        SGOutColor(const std::shared_ptr<SGVec4> color) :
                _fragColor(color)
        {

        }

        std::string GlslDeclString() const override{return "";}
        int ChildrenCount() const override{return 1;}

        SGNode &ChildAt(int i) const override
        {
            if(i==0)
                return *_fragColor;
            else
                throw std::runtime_error("...");
        }

        std::string TgfLabel() const override{return "SGOutColor";}

    protected:
        std::string GlslStringProtected() const override
        {
            return std::format("FragColor = {};\n ", _fragColor->GlslString());
        }

    private:
        std::shared_ptr<SGVec4> _fragColor;

    };

    class SGOutColorAndDepth : public SGOutMeshGizmo
    {
    public:
        SGOutColorAndDepth(const std::shared_ptr<SGVec4> color, const std::shared_ptr<SGFloat> depth) :
                _fragColor(color), _fragDepth(depth)
        {
        }

        std::string GlslDeclString() const override{return "";}
        int ChildrenCount() const override{return 2;}

        SGNode &ChildAt(int i) const override
        {
            if      (i==0) return *_fragColor;
            else if (i==1) return *_fragDepth;

            else
                throw std::runtime_error("...");
        }

        std::string TgfLabel() const override{return "SGOutColorAndDepth";}

    protected:
        std::string GlslStringProtected() const override
        {
            return
            std::format("gl_FragDepth = {};\n FragColor = {};\n",
                        _fragDepth->GlslString(),
                        _fragColor->GlslString());
        }

    private:
        std::shared_ptr<SGVec4>  _fragColor;
        std::shared_ptr<SGFloat> _fragDepth;

    };

    class SGInVertPosition : public SGVec3
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInFragPosition";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "fs_in.v_position";
        }

    };

    class SGInVertColor : public SGVec4
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInFragColor";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "fs_in.v_color";
        }

    };

    class SGInVertNormal : public SGVec4
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInVertNormal";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "fs_in.v_normal";
        }

    };

    class SGInEyePosition : public SGVec3
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInEyePosition";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "f_viewPos.xyz";
        }

    };

    class SGInViewMatrix : public SGMat4
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInViewMatrix";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "f_viewMat";
        }

    };

    class SGInProjectionMatrix : public SGMat4
    {
    public:
        int ChildrenCount()             const override {return 0;}
        SGNode &ChildAt(int i)          const override {throw std::runtime_error("");}
        std::string GlslDeclString()    const override {return "";}
        std::string TgfLabel()          const override {return "SGInProjectionMatrix";}

    protected:
        std::string GlslStringProtected() const override
        {
            return "f_projMat";
        }

    };
}


#endif //TAOGL_SHADERGRAPHIONODES_H
