#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <optional>

//class OGLTexture2D : public OGLResource
//{
//private:
//	int 
//		_width, _height;
//
//	void Init(int level, TextureInternalFormat internalFormat, int width, int height, GLenum format, GLenum type, const void* data)
//	{
//		Bind();
//
//		glTexImage2D(GL_TEXTURE_2D, level, internalFormat,
//			width, height, 0, format, type, data);
//
//		OGLUtils::CheckOGLErrors();
//
//		UnBind();
//	}
//
//	void SetParameters(TextureFiltering minfilter, TextureFiltering magfilter, TextureWrap wrapS, TextureWrap wrapT)
//	{
//		Bind();
//
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilter);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
//
//		UnBind();
//	}
//
//	GLenum ResolveFormat(TextureInternalFormat internal)
//	{
//		switch (internal)
//		{
//		case(TextureInternalFormat::Rgba_32f):
//				return GL_RGBA;
//				break;
//		case(TextureInternalFormat::Rgb_16f):
//				return GL_RGB;
//				break;
//		case(TextureInternalFormat::R_16ui):
//			return GL_RED;
//			break;
//		default:
//				return internal;
//				break;
//		}
//	}
//
//public:
//	OGLTexture2D(int width, int height, TextureInternalFormat internalFormat) 
//	{
//		OGLResource::Create(OGLResourceType::TEXTURE);
//
//		Init(0, internalFormat, width, height, ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, NULL);
//
//		SetParameters(
//			TextureFiltering::Nearest, TextureFiltering::Nearest,
//			TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);
//
//		OGLUtils::CheckOGLErrors();
//	}
//
//	OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, void *data,  GLenum dataFormat, GLenum type)
//	{
//		OGLResource::Create(OGLResourceType::TEXTURE);
//
//		Init(0, internalFormat, width, height, dataFormat, type, data);
//
//		SetParameters(
//			TextureFiltering::Nearest, TextureFiltering::Nearest,
//			TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);
//
//		OGLUtils::CheckOGLErrors();
//	}
//
//	OGLTexture2D(OGLTexture2D&& other) noexcept : OGLResource(std::move(other))
//	{
//		_width = other._width;
//		_height = other._height;
//	};
//
//	void Bind() const
//	{
//		glBindTexture(GL_TEXTURE_2D, OGLResource::ID());
//	}
//
//	void UnBind() const
//	{
//		glBindTexture(GL_TEXTURE_2D, 0);
//	}
//
//	OGLTexture2D& operator=(OGLTexture2D&& other) noexcept
//	{
//		if (this != &other)
//		{
//			OGLResource::operator=(std::move(other));
//			_height = other._height;
//			_width = other._width;
//		}
//		return *this;
//	};
//
//	~OGLTexture2D()
//	{
//		OGLResource::Destroy();
//	}
//
//};
//class FrameBuffer : public OGLResource
//{
//private:
//	std::optional<OGLTexture2D> _depthTexture;
//	std::vector<OGLTexture2D> _colorTextures;
//	int
//		_width, _height;
//
//	void Initialize(int width, int height, bool depth, bool color, int colorAttachments)
//	{
//		
//		if (depth)
//			_depthTexture = OGLTexture2D(width, height, TextureInternalFormat::Depth_Component);
//
//		if (color)
//		{
//			for (int i = 0; i < colorAttachments; i++)
//				_colorTextures.push_back(OGLTexture2D(width, height, TextureInternalFormat::Rgba_32f));
//		}
//		
//		Bind();
//
//		if (color)
//		{
//			for (int i = 0; i < colorAttachments; i++)
//			{
//				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, _colorTextures.at(i).ID(), 0);
//			}
//
//			if (depth)
//				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture.value().ID(), 0);
//
//		}
//		else if (depth)
//		{
//			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture.value().ID(), 0);
//
//			glDrawBuffer(GL_NONE);
//			glReadBuffer(GL_NONE);
//		}
//
//		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
//			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
//
//		UnBind();
//
//	}
//public:
//	FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth)
//		:_width(width), _height(height)
//	{
//		OGLResource::Create(OGLResourceType::FRAMEBUFFER);
//
//		Initialize(_width, _height, depth, color, colorAttachments);
//	}
//
//	FrameBuffer(FrameBuffer&& other) noexcept : OGLResource(std::move(other))
//	{
//		_width = other._width;
//		_height = other._height;
//
//		_colorTextures = std::vector<OGLTexture2D>(std::move(other._colorTextures));
//		_depthTexture = std::optional<OGLTexture2D>(std::move(other._depthTexture));
//	};
//
//	
//	FrameBuffer& operator=(FrameBuffer&& other) noexcept
//	{
//		if (this != &other)
//		{
//			OGLResource::operator=(std::move(other));
//
//			_height = other._height;
//			_width = other._width;
//
//			_colorTextures = std::vector<OGLTexture2D>(std::move(other._colorTextures));
//			_depthTexture = std::optional<OGLTexture2D>(std::move(other._depthTexture));
//		}
//		return *this;
//
//	};
//	~FrameBuffer()
//	{
//		OGLResource::Destroy();
//	}
//
//	
//	void Bind(bool read, bool write) const 
//	{
//		int mask = (read ? GL_READ_FRAMEBUFFER : 0) | (write ? GL_DRAW_FRAMEBUFFER : 0);
//		glBindFramebuffer(mask, OGLResource::ID());
//	}
//
//	void Bind() const
//	{
//		glBindFramebuffer(GL_FRAMEBUFFER, OGLResource::ID());
//	}
//
//	void UnBind() const
//	{
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//
//	
//	unsigned int DepthTextureId() const
//	{
//		return _depthTexture.value().ID();
//	}
//
//	unsigned int ColorTextureId(int attachment = 0) const
//	{
//		return _colorTextures.at(attachment).ID();
//	}
//
//	unsigned int Width() const
//	{
//		return _width;
//	}
//
//	unsigned int Height() const
//	{
//		return _height;
//	}
//
//	void CopyFromOtherFbo(const FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1) const
//	{
//
//		unsigned int id = other!=nullptr ? other->ID() : 0;
//
//		unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);
//
//		// Bind read buffer
//		glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
//
//		// Bind draw buffer
//		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLResource::ID());
//
//		if (color)
//			glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);
//
//		// Blit
//		glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);
//
//		// Unbind
//		glDrawBuffer(GL_COLOR_ATTACHMENT0);
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//
//	void CopyToOtherFbo(const FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1) const
//	{
//
//		unsigned int id = other != nullptr ? other->ID() : 0;
//
//		unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);
//
//		// Bind read buffer
//		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLResource::ID());
//
//		// Bind draw buffer
//		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
//
//		if (color)
//		{
//			if (id != 0)
//				glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);
//			else
//				glDrawBuffer(GL_BACK);
//		}
//
//		// Blit
//		glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);
//
//		// Unbind
//		if (color && id != 0)
//			glDrawBuffer(GL_COLOR_ATTACHMENT0);
//
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//
//};

#endif
//class FrameBuffer : public OGLResource
//{
//private:
//	OGLTexture2D _depthTexture;
//	std::vector<OGLTexture2D> _colorTextures;
//	int 
//		_width, _height;
//	
//	void Initialize(unsigned int width, unsigned int height, bool depth, bool color, int colorAttachments)
//	{
//		//TODO: hardcoded formats are not the best, right?
//		glGenFramebuffers(1, &_id);
//
//		if (depth)
//		{
//			glGenTextures(1, &_idTexDepth);
//			glBindTexture(GL_TEXTURE_2D, _idTexDepth);
//			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
//				width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//
//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//
//			glBindTexture(GL_TEXTURE_2D, 0);
//		}
//
//		if (color)
//		{
//			for (int i = 0; i < colorAttachments; i++)
//			{
//				unsigned int id = 0;
//				glGenTextures(1, &id);
//				glBindTexture(GL_TEXTURE_2D, id);
//				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
//					width, height, 0, GL_RGB, GL_FLOAT, NULL);
//
//				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//
//				glBindTexture(GL_TEXTURE_2D, 0);
//
//				_idTexCol.push_back(id);
//			}
//		}
//
//		glBindFramebuffer(GL_FRAMEBUFFER, _id);
//
//
//		if (color)
//		{
//			for (int i = 0; i < colorAttachments; i++)
//			{
//				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, _idTexCol[i], 0);
//			}
//
//			if (depth)
//				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _idTexDepth, 0);
//
//		}
//		else if (depth)
//		{
//			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _idTexDepth, 0);
//
//			glDrawBuffer(GL_NONE);
//			glReadBuffer(GL_NONE);
//		}
//
//		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
//			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
//
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//	}
//public:
//	FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth)
//		:_width(width), _height(height), _depth(depth), _color(color)
//	{
//		Initialize(_width, _height, _depth, _color, colorAttachments);
//	}
//	
//	//~FrameBuffer()
//	//{
//	//	FreeUnmanagedResources();
//	//}
//
//	void FreeUnmanagedResources()
//	{
//		if (_idTexCol[0] != 0)
//		{
//			for (int i = 0; i < _idTexCol.size(); i++)
//			{
//				glDeleteTextures(1, &_idTexCol[i]);
//			}
//
//			_idTexCol.clear();
//		}
//		if (_idTexDepth != 0)
//		{
//			glDeleteTextures(1, &_idTexDepth);
//			_idTexDepth = 0;
//		}
//		if (_id != 0)
//		{
//			glDeleteFramebuffers(1, &_id);
//			_id = 0;
//		}
//	}
//	void Bind(bool read, bool write)
//	{
//		int mask = (read ? GL_READ_FRAMEBUFFER : 0) | (write ? GL_DRAW_FRAMEBUFFER : 0);
//		glBindFramebuffer(mask, _id);
//	}
//
//	void Unbind()
//	{
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//
//	unsigned int Id()
//	{
//		return _id;
//	}
//
//	unsigned int DepthTextureId()
//	{
//		return _idTexDepth;
//	}
//
//	unsigned int ColorTextureId()
//	{
//		return _idTexCol[0];
//	}
//
//	unsigned int ColorTextureId(int attachment)
//	{
//		return _idTexCol[attachment];
//	}
//
//	unsigned int Width()
//	{
//		return _width;
//	}
//
//	unsigned int Height()
//	{
//		return _height;
//	}
//
//	void CopyFromOtherFbo(FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1)
//	{
//
//		unsigned int id = other != NULL ? other->_id : 0;
//
//		unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);
//
//		// Bind read buffer
//		glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
//
//		// Bind draw buffer
//		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _id);
//
//		if(color)
//			glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);
//
//		// Blit
//		glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);
//
//		// Unbind
//		glDrawBuffer(GL_COLOR_ATTACHMENT0);
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//
//	void CopyToOtherFbo(FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1)
//	{
//
//		unsigned int id = other != NULL ? other->_id : 0;
//
//		unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);
//
//		// Bind read buffer
//		glBindFramebuffer(GL_READ_FRAMEBUFFER, _id);
//
//		// Bind draw buffer
//		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
//
//		if (color)
//		{
//			if(id!=0)
//				glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);
//			else
//				glDrawBuffer(GL_BACK);
//		}
//
//		// Blit
//		glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);
//
//		// Unbind
//		if (color && id != 0)
//			glDrawBuffer(GL_COLOR_ATTACHMENT0);
//
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//	}
//};


