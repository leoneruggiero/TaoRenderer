#pragma once
#include <concepts>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace tao_gizmos_sdf
{
	// From: https://iquilezles.org/articles/distfunctions2d/
	///////////////////////////////////////////////////////////
	
	template<std::floating_point T>
	T Remap(T val, glm::vec<2, T> oldRange, glm::vec<2, T> newRange)
	{
		return newRange.x + (newRange.y-newRange.x) * (val - oldRange.x) / (oldRange.y - oldRange.x);
	}

	template<std::floating_point T>
	glm::vec<2,T> Remap(
		glm::vec<2, T> val, 
		glm::vec<2, T> oldRangeX, glm::vec<2, T> newRangeX,
		glm::vec<2, T> oldRangeY, glm::vec<2, T> newRangeY)
	{
		return glm::vec<2, T>
		{
			Remap(val.x, oldRangeX, newRangeX),
			Remap(val.y, oldRangeY, newRangeY)
		};
	}

	template<std::floating_point T>
	[[nodiscard]] T SdfShell(T v, T thickness)
	{
		return glm::abs(v) - thickness;
	}

	template<std::floating_point T>
	[[nodiscard]] T SdfInflate(T v, T thickness)
	{
		return v - thickness;
	}

	template<std::floating_point T>
	[[nodiscard]] T SdfAdd(T v1, T v2)
	{
		return glm::min(v1, v2);
	}

	template<std::floating_point T>
	T SdfSubtract(T v1, T v2)
	{
		return glm::max(-v1, v2);
	}

	template<std::floating_point T>
	[[nodiscard]] T SdfIntersect(T v1, T v2)
	{
		return glm::max(v1, v2);
	}

	template<std::floating_point T>
	class Sdf
	{
	public:
		Sdf(glm::vec<2, T> p) : _p(p), _t(static_cast<T>(1.0))
		{
		}

		Sdf<T>& Rotate(T rad)
		{
			glm::mat<4, 4, T> rot{1};
			_t =  _t * glm::rotate(rot, -rad, { 0, 0, 1 });

			return *this;
		}

		Sdf<T>& Translate(glm::vec<2, T> t)
		{
			glm::mat<4, 4, T> tr{1};
			_t = _t * glm::translate(tr, glm::vec<3, T>{-t, 0});

			return *this;
		}

		Sdf<T>& Scale(glm::vec<2, T> s)
		{
			glm::mat<4, 4, T> scale{ 1 };
			_t = _t * glm::scale(scale, glm::vec<3, T>{1.0/s.x, 1.0/s.y, 0});

			return *this;
		}

		// implicit conversion to T so that
		// you can write T val = Sdf{...params };
		operator T() const
		{
			auto p = _t * glm::vec<4, T>{_p, 0, 1};
			return Evaluate(glm::vec<2, T>{p.x, p.y});
		}

	private:
		glm::vec<2, T>		_p; // sampling location
		glm::mat<4, 4, T>	_t;	// transformation
		virtual T Evaluate(glm::vec<2, T> p) const = 0;
	};

	template<std::floating_point T>
	class SdfCircle : public Sdf<T>
	{
	public:
		SdfCircle(glm::vec<2, T> p, T radius) : Sdf<T>{p}, radius(radius)
		{
		}
	private:
		T radius;
		T Evaluate(glm::vec<2, T> p) const override
		{
			return glm::length(p) - radius;
		}
	};

	template<std::floating_point T>
	class SdfBox : public Sdf<T>
	{
	public:
		SdfBox(glm::vec<2, T> p, glm::vec<2, T> size) : Sdf<T>{ p }, size{size}
		{
		}
	private:
		glm::vec<2, T> size;
		T Evaluate(glm::vec<2, T> p) const override
		{
			glm::vec<2, T> d = glm::abs(p) - size;
			return glm::length(glm::max(d, { 0.0 })) + glm::min(glm::max(d.x, d.y), { 0.0 });
		}
	};

	template<std::floating_point T>
	class SdfTriangle : public Sdf<T>
	{
	public:
		SdfTriangle(glm::vec<2, T> p, glm::vec<2, T> size) : Sdf<T>{ p }, size{ size }
		{
		}
	private:
		glm::vec<2, T> size;
		T Evaluate(glm::vec<2, T> p) const override
		{
			p.x = glm::abs(p.x);
			glm::vec<2, T> a = p - size * glm::clamp<T>(glm::dot(p, size) / glm::dot(size, size), 0.0, 1.0);
			glm::vec<2, T> b = p - size * glm::vec<2, T>(glm::clamp<T>(p.x / size.x, 0.0, 1.0), 1.0);
			float s = -glm::sign(size.y);
			glm::vec<2, T> d = glm::min(
				glm::vec<2, T>(glm::dot(a, a), s * (p.x * size.y - p.y * size.x)),
				glm::vec<2, T>(glm::dot(b, b), s * (p.y - size.y))
			);
			return -glm::sqrt(d.x) * glm::sign(d.y);
		}
	};


	template<std::floating_point T>
	class SdfSegment : public Sdf<T>
	{
	public:
		SdfSegment(glm::vec<2, T> p, glm::vec<2, T> start, glm::vec<2, T> end)
		: Sdf<T>{ p }, start{start}, end{end}
		{
		}
	private:
		glm::vec<2, T> start;
		glm::vec<2, T> end;
		T Evaluate(glm::vec<2, T> p) const override
		{
			glm::vec<2, T> pa =   p - start;
			glm::vec<2, T> ba = end - start;
			float h = glm::clamp<T>(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0, 1.0);
			return glm::length(pa - ba * h);
		}
	};


}

namespace tao_gizmos_procedural
{

	template<int N, typename P>
	requires
		std::is_floating_point_v<P> || std::is_unsigned_v<P>
		struct TexelData
	{
		P data[N];
	};

	template<int N, typename P>
	requires
		std::is_floating_point_v<P> || std::is_unsigned_v<P>
		class TextureData
	{
		typedef TexelData<N, P> texel_type;

	public:
		TextureData(unsigned int width, unsigned int height, TexelData<N, P> initialValue)
			:_width(width), _height(height)
		{
			if(CheckValidSize()) throw std::runtime_error("TODO, is it better to have something linke TaoGizmosException?");

			_data = std::vector(width * height, initialValue);
		}

		void FillWithFunction(const std::function<glm::vec<N, P>(unsigned int x, unsigned int y)>& func)
		{
			for (unsigned int y = 0; y < _height ; y++)
			for (unsigned int x = 0; x < _width; x++)
			{
				glm::vec<N, P> v = func(x, y);

				for (glm::size_t i = 0; i < N; i++)
					_data[(y * _width + x)].data[i] = v[i];
			}
		}

		void FillWithData(glm::uvec2 location, TextureData<N, P> data)
		{
			if (CheckOutOfBounds(location)) throw std::exception("TODO, is it better to have something linke TaoGizmosException?");
			
			glm::uvec2 const maxDst = glm::min
			(
				location + glm::uvec2(data._width, data._height),
				glm::uvec2(_width, _height)
			);

			glm::uvec2 const copySz = maxDst - location;

			for (unsigned int yDst = location.y, ySrc = 0; ySrc < copySz.y; yDst++, ySrc++)
			{
				std::copy
				(
					data.Data().begin() + ySrc * data._width,
					data.Data().begin() + ySrc * data._width + copySz.x,
					_data	   .begin() + yDst *      _width + location.x
				);
			}
		}

		const void* DataPtr() const		 { return static_cast<const void*>(_data.data());	}
		std::vector <texel_type>& Data() { return _data;									}

	private:
		unsigned int _width;
		unsigned int _height;
		std::vector<texel_type> _data;

		bool CheckOutOfBounds(glm::uvec2 location) const { return location.x >= _width || location.y > _height; }
		bool CheckValidSize()					   const { return _width == 0 || _height == 0; }
	};

	typedef TextureData< 4, unsigned char > TextureDataRgbaUnsignedByte;
	typedef TextureData< 3, unsigned char > TextureDataRgbUnsignedByte;
	typedef TextureData< 1, unsigned char > TextureDataRUnsignedByte;
	typedef TextureData< 4, float 		  > TextureDataRgbaFloat;
	typedef TextureData< 3, float 		  > TextureDataRgbFloat;
	typedef TextureData< 1, float 		  > TextureDataRFloat;

}