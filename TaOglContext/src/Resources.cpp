#include "Resources.h"

namespace tao_ogl_resources
{
    // ReSharper disable CppMemberFunctionMayBeConst

    GLuint vertex_shader::Create() { GLuint id = 0; GL_CALL(id = glCreateShader(GL_VERTEX_SHADER)); return id; }
    void   vertex_shader::Destroy(GLuint id) { GL_CALL(glDeleteShader(id)); }
    GLuint fragment_shader::Create() { GLuint id = 0; GL_CALL(id = glCreateShader(GL_FRAGMENT_SHADER));  return id; }
    void   fragment_shader::Destroy(GLuint id) { GL_CALL(glDeleteShader(id)); }
    GLuint geometry_shader::Create() { GLuint id = 0; GL_CALL(id = glCreateShader(GL_GEOMETRY_SHADER)); return id; }
    void   geometry_shader::Destroy(GLuint id) { GL_CALL(glDeleteShader(id)); }
    GLuint shader_program::Create() { GLuint id = 0; GL_CALL(id = glCreateProgram()); return id; }
    void   shader_program::Destroy(GLuint id) { GL_CALL(glDeleteProgram(id)); }
    GLuint vertex_buffer_object::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   vertex_buffer_object::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint index_buffer::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   index_buffer::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint uniform_buffer::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   uniform_buffer::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint shader_storage_buffer::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   shader_storage_buffer::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint pixel_pack_buffer::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   pixel_pack_buffer::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint pixel_unpack_buffer::Create() { GLuint id = 0; GL_CALL(glCreateBuffers(1, &id)); return id; }
    void   pixel_unpack_buffer::Destroy(GLuint id) { GL_CALL(glDeleteBuffers(1, &id)); }
    GLuint vertex_attrib_array::Create() { GLuint id = 0; GL_CALL(glCreateVertexArrays(1, &id)); return id; }
    void   vertex_attrib_array::Destroy(GLuint id) { GL_CALL(glDeleteVertexArrays(1, &id)); }
    GLuint texture_1D::Create() { GLuint id = 0; GL_CALL(glCreateTextures(GL_TEXTURE_1D, 1, &id)); return id; }
    void   texture_1D::Destroy(GLuint id) { GL_CALL(glDeleteTextures(1, &id)); }
    GLuint texture_2D::Create() { GLuint id = 0; GL_CALL(glCreateTextures(GL_TEXTURE_2D, 1, &id)); return id; }
    void   texture_2D::Destroy(GLuint id) { GL_CALL(glDeleteTextures(1, &id)); }
    GLuint texture_2D_multisample::Create() { GLuint id = 0; GL_CALL(glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &id)); return id; }
    void   texture_2D_multisample::Destroy(GLuint id) { GL_CALL(glDeleteTextures(1, &id)); }
    GLuint texture_cube::Create() { GLuint id = 0; GL_CALL(glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id)); return id; }
    void   texture_cube::Destroy(GLuint id) { GL_CALL(glDeleteTextures(1, &id)); }
    GLuint framebuffer::Create() { GLuint id = 0; GL_CALL(glCreateFramebuffers(1, &id)); return id; }
    void   framebuffer::Destroy(GLuint id) { GL_CALL(glDeleteFramebuffers(1, &id)); }
    GLuint sampler::Create() { GLuint id = 0; GL_CALL(glCreateSamplers(1, &id)); return id; }
    void   sampler::Destroy(GLuint id) { GL_CALL(glDeleteSamplers(1, &id)); }


    /// SHADER
    ////////////////
    template <typename T> requires ogl_shader<T>
    static void compileShader(const OglResource<T>& obj, const char* src)
    {
        GL_CALL(glShaderSource(obj.ID(), 1, &src, nullptr));
        GL_CALL(glCompileShader(obj.ID()));

        // compile error check
        int ok = 1;
        GL_CALL(glGetShaderiv(obj.ID(), GL_COMPILE_STATUS, &ok));
        if (!ok)
        {
            char buf[1024];
            GL_CALL(glGetShaderInfoLog(obj.ID(), 1024, nullptr, buf));
            throw std::runtime_error((std::string{ "Shader compile error: \n" } + std::string(buf)).c_str());
        }
    }

    void OglVertexShader::Compile(const char* source) { compileShader(_ogl_obj, source); }
    void OglGeometryShader::Compile(const char* source) { compileShader(_ogl_obj, source); }
    void OglFragmentShader::Compile(const char* source) { compileShader(_ogl_obj, source); }
    void OglShaderProgram::LinkProgram()
    {
	    GL_CALL(glLinkProgram(_ogl_obj.ID()));

        // link error check
        int ok = 1;
        GL_CALL(glGetProgramiv(_ogl_obj.ID(), GL_LINK_STATUS, &ok));
        if (!ok)
        {
            char buf[1024];
            GL_CALL(glGetProgramInfoLog(_ogl_obj.ID(), 1024, nullptr, buf));
            throw std::runtime_error((std::string{ "Shader program link error: \n" } + std::string(buf)).c_str());
        }

    }
    void OglShaderProgram::AttachShader(GLuint shader)  { GL_CALL(glAttachShader(_ogl_obj.ID(), shader)); }
    void OglShaderProgram::UseProgram()                 { GL_CALL(glUseProgram  (_ogl_obj.ID())); }
    
    GLint OglShaderProgram::GetUniformLocation(const char* name) const
    {
        GL_CALL(GLint uniformLocation = glGetUniformLocation(_ogl_obj.ID(), name));
        return uniformLocation;
    }

    // TODO
    // fail at compile-time if T is not one of uint/int/float
    //template <typename T>void OglShaderProgram::SetUniform(GLint location, T v0) { static_assert(false); }
    //template <typename T>void OglShaderProgram::SetUniform(GLint location, T v0, T v1) { static_assert(false); }
    //template <typename T>void OglShaderProgram::SetUniform(GLint location, T v0, T v1, T v2) { static_assert(false); }
    //template <typename T>void OglShaderProgram::SetUniform(GLint location, T v0, T v1, T v2, T v3) { static_assert(false); }

    // UINT template spec
    template<>void OglShaderProgram::SetUniform<GLuint>(GLint location, GLuint v0) { GL_CALL(glUniform1ui(location, v0)); }
    template<>void OglShaderProgram::SetUniform<GLuint>(GLint location, GLuint v0, GLuint v1) { GL_CALL(glUniform2ui(location, v0, v1)); }
    template<>void OglShaderProgram::SetUniform<GLuint>(GLint location, GLuint v0, GLuint v1, GLuint v2) { GL_CALL(glUniform3ui(location, v0, v1, v2)); }
    template<>void OglShaderProgram::SetUniform<GLuint>(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) { GL_CALL(glUniform4ui(location, v0, v1, v2, v3)); }

    // INT template spec
    template<>void OglShaderProgram::SetUniform<GLint>(GLint location, GLint v0) { GL_CALL(glUniform1i(location, v0)); }
    template<>void OglShaderProgram::SetUniform<GLint>(GLint location, GLint v0, GLint v1) { GL_CALL(glUniform2i(location, v0, v1)); }
    template<>void OglShaderProgram::SetUniform<GLint>(GLint location, GLint v0, GLint v1, GLint v2) { GL_CALL(glUniform3i(location, v0, v1, v2)); }
    template<>void OglShaderProgram::SetUniform<GLint>(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) { GL_CALL(glUniform4i(location, v0, v1, v2, v3)); }

    // FLOAT template spec
    template<>void OglShaderProgram::SetUniform<GLfloat>(GLint location, GLfloat v0) { GL_CALL(glUniform1f(location, v0)); }
    template<>void OglShaderProgram::SetUniform<GLfloat>(GLint location, GLfloat v0, GLfloat v1) { GL_CALL(glUniform2f(location, v0, v1)); }
    template<>void OglShaderProgram::SetUniform<GLfloat>(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { GL_CALL(glUniform3f(location, v0, v1, v2)); }
    template<>void OglShaderProgram::SetUniform<GLfloat>(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { GL_CALL(glUniform4f(location, v0, v1, v2, v3)); }

    void OglShaderProgram::SetUniformMatrix2(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { GL_CALL(glUniformMatrix2fv(location, count, transpose, value)); }
    void OglShaderProgram::SetUniformMatrix3(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { GL_CALL(glUniformMatrix3fv(location, count, transpose, value)); }
    void OglShaderProgram::SetUniformMatrix4(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { GL_CALL(glUniformMatrix4fv(location, count, transpose, value)); }

    void OglShaderProgram::SetUniformBlockBinding(const char* name, GLuint uniformBlockBinding)
    {
        GL_CALL(GLuint uboIdx = glGetUniformBlockIndex(_ogl_obj.ID(), name);      );
        GL_CALL(glUniformBlockBinding(_ogl_obj.ID(), uboIdx, uniformBlockBinding););
    }


    /// Vertex Buffer
    ///////////////////
    static void namedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) { GL_CALL(glNamedBufferData(buffer, size, data, usage)); }
    static void namedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) { GL_CALL(glNamedBufferSubData(buffer, offset, size, data)); }

    void OglVertexBuffer::Bind() { GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, _ogl_obj.ID())); }
    void OglVertexBuffer::UnBind() { GL_CALL(glBindBuffer(GL_ARRAY_BUFFER,0)); }
    void OglVertexBuffer::SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage){namedBufferData(_ogl_obj.ID(), size, data, usage);}
    void OglVertexBuffer::SetSubData(GLintptr offset, GLsizeiptr size, const void* data) { namedBufferSubData(_ogl_obj.ID(), offset, size, data); }
   
    /// Index Buffer
    ///////////////////
    void OglIndexBuffer::Bind() { GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ogl_obj.ID())); }
    void OglIndexBuffer::UnBind() { GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)); }
    void OglIndexBuffer::SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage) { namedBufferData(_ogl_obj.ID(), size, data, usage); }
    void OglIndexBuffer::SetSubData(GLintptr offset, GLsizeiptr size, const void* data) { namedBufferSubData(_ogl_obj.ID(), offset, size, data); }

    /// VertexAttrib Array
    //////////////////////////
    void OglVertexAttribArray::Bind() { GL_CALL(glBindVertexArray(_ogl_obj.ID())); };
    void OglVertexAttribArray::UnBind() { GL_CALL(glBindVertexArray(0)); };
    void OglVertexAttribArray::EnableVertexAttrib(GLuint index)  { GL_CALL(glEnableVertexArrayAttrib( _ogl_obj.ID(), index)); }
    void OglVertexAttribArray::DisableVertexAttrib(GLuint index) { GL_CALL(glDisableVertexArrayAttrib(_ogl_obj.ID(), index)); }
    void OglVertexAttribArray::SetVertexAttribPointer(OglVertexBuffer& vertexBuffer, GLuint index, GLint size, ogl_vertex_attrib_type type, GLboolean normalized, GLsizei stride, const void* pointer, GLuint divisor)
    {
        Bind();

        vertexBuffer.Bind();

        GL_CALL(glVertexAttribPointer(index, size, type, normalized, stride, pointer));
        GL_CALL(glVertexAttribDivisor(index,divisor));

        OglVertexBuffer::UnBind();

        UnBind();
    }
    
	void OglVertexAttribArray::SetIndexBuffer(OglIndexBuffer& indexBuffer)
	{
        Bind();
        indexBuffer.Bind();
        UnBind();
        OglIndexBuffer::UnBind();
	}

    /// Uniform Buffer
    ///////////////////
    void OglUniformBuffer::Bind(GLuint index) { GL_CALL(glBindBufferBase(GL_UNIFORM_BUFFER, index, _ogl_obj.ID())); }
    void OglUniformBuffer::BindRange(GLuint index, GLintptr offset, GLsizeiptr size) { GL_CALL(glBindBufferRange(GL_UNIFORM_BUFFER, index, _ogl_obj.ID(), offset, size)); }
    void OglUniformBuffer::SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage) { namedBufferData(_ogl_obj.ID(), size, data, usage); }
    void OglUniformBuffer::SetSubData(GLintptr offset, GLsizeiptr size, const void* data) { namedBufferSubData(_ogl_obj.ID(), offset, size, data); }
    
    /// Shader Storage Buffer
    ////////////////////////////
    void OglShaderStorageBuffer::Bind(GLuint index) { GL_CALL(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, _ogl_obj.ID())); }
    void OglShaderStorageBuffer::BindRange(GLuint index, GLintptr offset, GLsizeiptr size) { GL_CALL(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, _ogl_obj.ID(), offset, size)); }
    void OglShaderStorageBuffer::SetData(GLsizeiptr size, const void* data, ogl_buffer_usage usage) { namedBufferData(_ogl_obj.ID(), size, data, usage); }
    void OglShaderStorageBuffer::SetSubData(GLintptr offset, GLsizeiptr size, const void* data) { namedBufferSubData(_ogl_obj.ID(), offset, size, data); }

    /// Pixel Pack Buffer
    ////////////////////////////
    void  OglPixelPackBuffer::Bind()     { GL_CALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, _ogl_obj.ID())); }
    void  OglPixelPackBuffer::UnBind()   { GL_CALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0)); }
    void  OglPixelPackBuffer::BufferStorage(GLsizeiptr size, const void* data, ogl_buffer_flags flags){GL_CALL(glNamedBufferStorage(_ogl_obj.ID(), size, data, flags);)}
    void* OglPixelPackBuffer::MapBuffer(ogl_map_flags access){GL_CALL(return glMapNamedBuffer(_ogl_obj.ID(), access);)}
    void  OglPixelPackBuffer::UnmapBuffer(){ GL_CALL(glUnmapNamedBuffer(_ogl_obj.ID())); }

    /// Pixel Unpack buffer
    ////////////////////////////
    void OglPixelUnpackBuffer::Bind()   { GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _ogl_obj.ID())); }
    void OglPixelUnpackBuffer::UnBind() { GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0)); }

    /// Texture Utils
    ///////////////////
    static void bind(GLenum target, GLuint texture) { GL_CALL(glBindTexture( target, texture)); }
    static void unBind(GLenum target) { GL_CALL(glBindTexture(target, 0)); }
    static void bindToTextureUnit(GLenum target, GLuint texture, GLenum unit)
    {
        GL_CALL(glActiveTexture(unit));
	    GL_CALL(glBindTexture(target, texture));
    }
    static void unBindToTextureUnit(GLenum target, GLenum unit)
    {
        GL_CALL(glActiveTexture(unit));
	    GL_CALL(glBindTexture(target, 0));
    }
    static void texImage1D(GLuint texture, GLenum target, GLint level, ogl_texture_internal_format internalFormat,
        GLsizei width, ogl_texture_format format, ogl_texture_data_type type, const void* data)
    {
        bind(target, texture);
        GL_CALL(glTexImage1D(GL_TEXTURE_1D, level, internalFormat, width, 0, format, type, data));
        unBind(target);
    }
    static void texImage2D(GLuint texture, GLenum target, GLint level, ogl_texture_internal_format internalFormat,
        GLsizei width, GLint height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data)
    {
        bind(target, texture);
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, border, format, type, data));
        unBind(target);
    }
    void static generateMipmap(GLuint texture) { GL_CALL(glGenerateTextureMipmap(texture)); }
    void static setDepthStencilTextureMode(GLuint texture, ogl_texture_depth_stencil_tex_mode mode)
    {
        GL_CALL(glTextureParameteri(texture, GL_DEPTH_STENCIL_TEXTURE_MODE, mode));
    }
    void static setTextureCompareParams(GLuint texture, ogl_tex_compare_params params)
    {
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_COMPARE_MODE, params.compare_mode));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_COMPARE_FUNC, params.compare_func));
    }
    void static setTextureFilterParms(GLuint texture, ogl_tex_filter_params params)
    {
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, params.min_filter));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, params.mag_filter));
    }
    void static setTextureLodParams(GLuint texture, ogl_tex_lod_params params)
    {
        GL_CALL(glTextureParameterf(texture, GL_TEXTURE_LOD_BIAS, params.lod_bias));
        GL_CALL(glTextureParameterf(texture, GL_TEXTURE_MIN_LOD, params.min_lod));
        GL_CALL(glTextureParameterf(texture, GL_TEXTURE_MAX_LOD, params.max_lod));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_BASE_LEVEL, params.base_level));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, params.max_level));
    }
    void static setTextureWrapParams(GLuint texture, ogl_tex_wrap_params params)
    {
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_WRAP_S, params.wrap_s));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_WRAP_T, params.wrap_t));
        GL_CALL(glTextureParameteri(texture, GL_TEXTURE_WRAP_R, params.wrap_r));
    }
    void static setTextureParams(GLuint texture, ogl_tex_params params)
    {
        setDepthStencilTextureMode(texture, params.depth_stencil_tex_mode);
        setTextureCompareParams(texture, params.compare_params);
        setTextureFilterParms(texture, params.filter_params);
        setTextureLodParams(texture, params.lod_params);
        setTextureWrapParams(texture, params.wrap_params);
    }

    /// Texture1D
    ///////////////////
    void OglTexture1D::Bind()                                       { bind(GL_TEXTURE_1D, _ogl_obj.ID()); }
    void OglTexture1D::UnBind()                                     { unBind(GL_TEXTURE_1D); }
    void OglTexture1D::BindToTextureUnit(ogl_texture_unit unit)     { bindToTextureUnit(GL_TEXTURE_1D, _ogl_obj.ID(), unit); }
    void OglTexture1D::UnBindToTextureUnit(ogl_texture_unit unit)   { unBindToTextureUnit(GL_TEXTURE_1D, unit); }

    void OglTexture1D::TexImage(GLint level, ogl_texture_internal_format internalFormat, GLsizei width, GLsizei height,
        GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data)
    {
        texImage1D(_ogl_obj.ID(), GL_TEXTURE_1D, level, internalFormat, width, format, type, data);
    }
    void OglTexture1D::GenerateMipmap() { generateMipmap(_ogl_obj.ID()); }
    void OglTexture1D::SetFilterParams(ogl_tex_filter_params params) { setTextureFilterParms(_ogl_obj.ID(), params); }
    void OglTexture1D::SetLodParams(ogl_tex_lod_params params) { setTextureLodParams(_ogl_obj.ID(), params); }
    void OglTexture1D::SetWrapParams(ogl_tex_wrap_params params) { setTextureWrapParams(_ogl_obj.ID(), params); }

    /// Texture2D
    ///////////////////
    void OglTexture2D::Bind()                                       { bind(GL_TEXTURE_2D, _ogl_obj.ID()); }
    void OglTexture2D::UnBind()                                     { unBind(GL_TEXTURE_2D); }
    void OglTexture2D::BindToTextureUnit(ogl_texture_unit unit)     { bindToTextureUnit(GL_TEXTURE_2D, _ogl_obj.ID(), unit); }
    void OglTexture2D::UnBindToTextureUnit(ogl_texture_unit unit)   { unBindToTextureUnit(GL_TEXTURE_2D, unit); }

    void OglTexture2D::TexImage(GLint level, ogl_texture_internal_format internalFormat,
        GLsizei width, GLsizei height, ogl_texture_format format, ogl_texture_data_type type, const void* data)
    {
        texImage2D(_ogl_obj.ID(), GL_TEXTURE_2D, level, internalFormat, width, height, 0, format, type, data);
    }
    void OglTexture2D::GenerateMipmap() { generateMipmap(_ogl_obj.ID()); }
    void OglTexture2D::SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode) { setDepthStencilTextureMode(_ogl_obj.ID(), mode); }
    void OglTexture2D::SetCompareParams(ogl_tex_compare_params params) { setTextureCompareParams(_ogl_obj.ID(), params); }
    void OglTexture2D::SetFilterParams(ogl_tex_filter_params params) { setTextureFilterParms(_ogl_obj.ID(), params); }
    void OglTexture2D::SetLodParams(ogl_tex_lod_params params) { setTextureLodParams(_ogl_obj.ID(), params); }
    void OglTexture2D::SetWrapParams(ogl_tex_wrap_params params) { setTextureWrapParams(_ogl_obj.ID(), params); }
    void OglTexture2D::SetParams(ogl_tex_params params) { setTextureParams(_ogl_obj.ID(), params); }

    /// Texture2D Multisample
    ///////////////////
    void OglTexture2DMultisample::Bind() { bind(GL_TEXTURE_2D_MULTISAMPLE, _ogl_obj.ID()); }
    void OglTexture2DMultisample::UnBind() { unBind(GL_TEXTURE_2D_MULTISAMPLE); }
    void OglTexture2DMultisample::BindToTextureUnit(ogl_texture_unit unit) { bindToTextureUnit(GL_TEXTURE_2D_MULTISAMPLE, _ogl_obj.ID(), unit); }
    void OglTexture2DMultisample::UnBindToTextureUnit(ogl_texture_unit unit) { unBindToTextureUnit(GL_TEXTURE_2D_MULTISAMPLE, unit); }

    void OglTexture2DMultisample::TexImage(GLsizei samples, ogl_texture_internal_format internalFormat,
        GLsizei width, GLsizei height, GLboolean fixedSampleLocation)
    {
        bind(GL_TEXTURE_2D_MULTISAMPLE, _ogl_obj.ID());
        GL_CALL(glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, fixedSampleLocation););
        unBind(GL_TEXTURE_2D_MULTISAMPLE);
    }

    /// Texture Cube
    ///////////////////
    void OglTextureCube::Bind()                                       { bind(GL_TEXTURE_CUBE_MAP, _ogl_obj.ID()); }
    void OglTextureCube::UnBind()                                     { unBind(GL_TEXTURE_CUBE_MAP); }
    void OglTextureCube::BindToTextureUnit(ogl_texture_unit unit)     { bindToTextureUnit(GL_TEXTURE_CUBE_MAP, _ogl_obj.ID(), unit); }
    void OglTextureCube::UnBindToTextureUnit(ogl_texture_unit unit)   { unBindToTextureUnit(GL_TEXTURE_CUBE_MAP, unit); }

    void OglTextureCube::TexImage(ogl_texture_cube_target target, GLint level, ogl_texture_internal_format internalFormat, 
        GLsizei width, GLsizei height, GLint border, ogl_texture_format format, ogl_texture_data_type type, const void* data)
    {
        texImage2D(_ogl_obj.ID(), target, level, internalFormat, width, height, border, format, type, data);
    }
    void OglTextureCube::GenerateMipmap() { generateMipmap(_ogl_obj.ID()); }
    void OglTextureCube::SetDepthStencilMode(ogl_texture_depth_stencil_tex_mode mode) { setDepthStencilTextureMode(_ogl_obj.ID(), mode); }
    void OglTextureCube::SetCompareParams(ogl_tex_compare_params params) { setTextureCompareParams(_ogl_obj.ID(), params); }
    void OglTextureCube::SetFilterParams(ogl_tex_filter_params params) { setTextureFilterParms(_ogl_obj.ID(), params); }
    void OglTextureCube::SetLodParams(ogl_tex_lod_params params) { setTextureLodParams(_ogl_obj.ID(), params); }
    void OglTextureCube::SetWrapParams(ogl_tex_wrap_params params) { setTextureWrapParams(_ogl_obj.ID(), params); }
    void OglTextureCube::SetParams(ogl_tex_params params) { setTextureParams(_ogl_obj.ID(), params); }

    /// Sampler
    ///////////////////
    void OglSampler::BindToTextureUnit  (ogl_texture_unit unit) { GL_CALL(glBindSampler(unit - GL_TEXTURE0, _ogl_obj.ID())); }
    void OglSampler::UnBindToTextureUnit(ogl_texture_unit unit) { GL_CALL(glBindSampler(unit - GL_TEXTURE0, 0)); }

    void OglSampler::SetCompareParams(ogl_sampler_compare_params params)
    {
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_COMPARE_MODE, params.compare_mode));
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_COMPARE_FUNC, params.compare_func));
    }
    void OglSampler::SetFilterParams(ogl_sampler_filter_params params)
    {
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_MIN_FILTER, params.min_filter));
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_MAG_FILTER, params.mag_filter));
    }
    void OglSampler::SetLodParams(ogl_sampler_lod_params params)
    {
        GL_CALL(glSamplerParameterf(_ogl_obj.ID(), GL_TEXTURE_LOD_BIAS, params.lod_bias));
        GL_CALL(glSamplerParameterf(_ogl_obj.ID(), GL_TEXTURE_MAX_LOD, params.max_lod));
        GL_CALL(glSamplerParameterf(_ogl_obj.ID(), GL_TEXTURE_MIN_LOD, params.min_lod));
    }
    void OglSampler::SetWrapParams(ogl_sampler_wrap_params params)
    {
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_WRAP_S, params.wrap_s));
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_WRAP_T, params.wrap_t));
        GL_CALL(glSamplerParameteri(_ogl_obj.ID(), GL_TEXTURE_WRAP_R, params.wrap_r));
    }
    void OglSampler::SetParams(ogl_sampler_params params)
    {
        SetCompareParams(params.compare_params);
        SetFilterParams(params.filter_params);
        SetLodParams(params.lod_params);
        SetWrapParams(params.wrap_params);
    }

    /// FBO 
    ///////////////////
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::Bind(ogl_framebuffer_binding target) { GL_CALL(glBindFramebuffer(target, _ogl_obj.ID())); }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::UnBind(ogl_framebuffer_binding target) { GL_CALL(glBindFramebuffer(target, 0)); }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::AttachTexture(ogl_framebuffer_attachment attachment, const Tex& texture, GLint level)
    {
	    GL_CALL(glNamedFramebufferTexture(_ogl_obj.ID(), attachment, texture._ogl_obj.ID(), level));
    }
    template<> // Texture 2D Multisample template spec
    void OglFramebuffer<OglTexture2DMultisample>::AttachTexture(ogl_framebuffer_attachment attachment, const OglTexture2DMultisample& texture, GLint level)
    {
        // level is *silently* ignored (is it bad?)
        GL_CALL(glNamedFramebufferTexture(_ogl_obj.ID(), attachment, texture._ogl_obj.ID(), 0 /*level must be zero*/));
    }

    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
    void OglFramebuffer<Tex>::SetDrawBuffers(GLsizei n, const ogl_framebuffer_read_draw_buffs* buffs)
    {
    	GL_CALL(glNamedFramebufferDrawBuffers(_ogl_obj.ID(), n, reinterpret_cast<const GLenum*>(buffs)));
    }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
    void OglFramebuffer<Tex>::SetReadBuffer(ogl_framebuffer_read_draw_buffs buff)
    {
        GL_CALL(glNamedFramebufferReadBuffer(_ogl_obj.ID(), buff));
    }

    static void FramebufferBlit(
        GLuint readFbo, GLuint drawFbo, 
        GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, // src rect
        GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, // dst rect
        ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter)
    {
        GL_CALL(glBlitNamedFramebuffer(readFbo, drawFbo, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter));
    }

    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::CopyFrom(const OglFramebuffer* src, GLint width, GLint height, ogl_framebuffer_copy_mask mask)
    {
        // src and dst have the same size.
        // copies a rect from <0,0> to <width, height>
        GLuint const srcId = src ? src->_ogl_obj.ID() : 0;
        FramebufferBlit(srcId, _ogl_obj.ID(), 0, 0, width, height, 0, 0, width, height, mask, ogl_framebuffer_copy_filter::fbo_copy_filter_nearest);
    }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::CopyFrom(const OglFramebuffer* src, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter)
    {
        GLuint const srcId = src ? src->_ogl_obj.ID() : 0;
        FramebufferBlit(srcId, _ogl_obj.ID(), srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::CopyTo(const OglFramebuffer* dst, GLint width, GLint height, ogl_framebuffer_copy_mask mask) const
    {
        // src and dst have the same size.
        // copies a rect from <0,0> to <width, height>
        GLuint const dstId = dst ? dst->_ogl_obj.ID() : 0;
        FramebufferBlit(_ogl_obj.ID(), dstId, 0, 0, width, height, 0, 0, width, height, mask, ogl_framebuffer_copy_filter::fbo_copy_filter_nearest);
    }
    template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
	void OglFramebuffer<Tex>::CopyTo(const OglFramebuffer* dst, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, ogl_framebuffer_copy_mask mask, ogl_framebuffer_copy_filter filter) const
    {
        GLuint const dstId = dst ? dst->_ogl_obj.ID() : 0;
        FramebufferBlit(_ogl_obj.ID(), dstId, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }

    // Forcing template instantiation....ugly, really ugly
    template class OglFramebuffer<OglTexture2D>;
    template class OglFramebuffer<OglTexture2DMultisample>;
    template class OglFramebuffer<OglTextureCube>;


    void OglFence::Create(ogl_sync_condition condition)
    {
        GL_CALL(_id = glFenceSync(condition, 0); )
    }

    void OglFence::Destroy()
    {
		GL_CALL(glDeleteSync(_id); )
    }

    ogl_wait_sync_result OglFence::ClientWaitSync(ogl_wait_sync_flags flags, unsigned long long timeout)
    {
	    GL_CALL(return static_cast<ogl_wait_sync_result>(glClientWaitSync(_id, flags, timeout));)
    }

	// ReSharper restore CppMemberFunctionMayBeConst
}