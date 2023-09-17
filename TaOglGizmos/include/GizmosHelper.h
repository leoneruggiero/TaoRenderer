#pragma once
#include <concepts>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <memory>

namespace tao_gizmos_sdf
{
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

    /// From: https://iquilezles.org/articles/distfunctions2d/
    ///////////////////////////////////////////////////////////
    template<std::floating_point T>
    class SdfNode;
    template<std::floating_point T>
    class SdfOpTransform;
    template<std::floating_point T>
    class SdfOpInflate;
    template<std::floating_point T>
    class SdfOpShell;
    template<std::floating_point T>
    class SdfOpAdd;

    template<std::floating_point T>
    class SdfNode
    {
    public:

        virtual T Evaluate(glm::vec<2, T> p) const = 0;
        virtual std::shared_ptr<SdfNode<T>> MakeNode() const = 0;
        virtual ~SdfNode() = default;

        SdfOpTransform<T> Transform(const glm::mat<3, 3, T>& t);
        SdfOpTransform<T> Translate(const glm::vec<2, T>& t)
        {
            glm::mat<3,3,T> tr = glm::mat<3,3,T>(static_cast<T>(1.0));
            tr[2][0] = t.x;
            tr[2][1] = t.y;

            return Transform(tr);
        };
        SdfOpTransform<T> Rotate(T a)
        {
            glm::mat<3,3,T> tr = glm::mat<3,3,T>(static_cast<T>(1.0));

            T cosA = glm::cos(a);
            T sinA = glm::sin(a);

            tr[0][0] = cosA;
            tr[0][1] = sinA;
            tr[1][0] = -sinA;;
            tr[1][1] = cosA;

            return Transform(tr);
        };
        SdfOpTransform<T> Scale(const glm::vec<2, T>& s)
        {
            glm::mat<3,3,T> tr = glm::mat<3,3,T>(static_cast<T>(1.0));
            tr[0][0] = s.x;
            tr[1][1] = s.y;

            return Transform(tr);
        };
        SdfOpInflate<T> Inflate(T i);
        SdfOpShell<T> Shell(T t);
        SdfOpAdd<T> Add(const SdfNode& other);
    };

    template<std::floating_point T>
    class SdfOpTransform: public SdfNode<T>
    {
    public:
        SdfOpTransform(std::shared_ptr<SdfNode<T>> op1, const glm::mat<3,3,T>& t):
                _transformation{glm::inverse(t)},
                _operand1{op1}
        {
        }

        virtual ~SdfOpTransform() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpTransform<T>{_operand1, glm::inverse(_transformation) /* ugly? see the constructor */});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            glm::vec<2, T> newP = _transformation * glm::vec<3, T>{p, static_cast<T>(1.0)};
            return _operand1->Evaluate(newP);
        }

    private:
        glm::mat<3,3,T> _transformation = glm::mat<3,3,T>{static_cast<T>(1.0)};
        std::shared_ptr<SdfNode<T>> _operand1;
    };

    template<std::floating_point T>
    class SdfOpInflate: public SdfNode<T>
    {
    public:
        SdfOpInflate(std::shared_ptr<SdfNode<T>> op1, T i):
                _thickness{i},
                _operand1{op1}
        {
        }

        virtual ~SdfOpInflate() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpInflate<T>{_operand1, _thickness});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return _operand1->Evaluate(p) - _thickness;
        }

    private:
        T _thickness = static_cast<T>(0.0);
        std::shared_ptr<SdfNode<T>> _operand1;
    };

    template<std::floating_point T>
    class SdfOpShell: public SdfNode<T>
    {
    public:
        SdfOpShell(std::shared_ptr<SdfNode<T>> op1, T i):
                _thickness{i},
                _operand1{op1}
        {
        }

        virtual ~SdfOpShell() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpShell<T>{_operand1, _thickness});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return glm::abs<T>(_operand1->Evaluate(p)) - _thickness;
        }

    private:
        T _thickness = static_cast<T>(0.0);
        std::shared_ptr<SdfNode<T>> _operand1;
    };

    template<std::floating_point T>
    class SdfOpAdd: public SdfNode<T>
    {
    public:
        SdfOpAdd(std::shared_ptr<SdfNode<T>> op1, std::shared_ptr<SdfNode<T>> op2):
                _operand1{op1},
                _operand2{op2}
        {
        }

        virtual ~SdfOpAdd() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpAdd<T>{_operand1, _operand2});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return glm::min<T>(_operand1->Evaluate(p), _operand2->Evaluate(p));
        }

    private:
        std::shared_ptr<SdfNode<T>> _operand1;
        std::shared_ptr<SdfNode<T>> _operand2;
    };

    template<std::floating_point T>
    SdfOpAdd<T> SdfNode<T>::Add(const SdfNode<T> &other)
    {
        return SdfOpAdd<T>{this->MakeNode(), other.MakeNode()};
    }

    template<std::floating_point T>
    SdfOpInflate<T> SdfNode<T>::Inflate(T i)
    {
        return SdfOpInflate<T>{this->MakeNode(),i};
    }

    template<std::floating_point T>
    SdfOpShell<T> SdfNode<T>::Shell(T t)
    {
        return SdfOpShell<T>{this->MakeNode(),t};
    }

    template<std::floating_point T>
    SdfOpTransform<T> SdfNode<T>::Transform(const glm::mat<3,3,T>& t)
    {
        return SdfOpTransform<T>{this->MakeNode(), t};
    }

    template<std::floating_point T>
    class SdfCircle: public SdfNode<T>
    {
    public:
        SdfCircle(glm::vec<2, T> center, T radius): _center{center}, _radius{radius}
        {

        }

        ~SdfCircle() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfCircle<T>{_center, _radius});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return glm::length(p-_center) - _radius;
        }

    private:
        glm::vec<2, T> _center = glm::vec<2, T>{static_cast<T>(0.0)};
        T _radius = static_cast<T>(0.0);
    };

    template<std::floating_point T>
    class SdfSegment: public SdfNode<T>
    {
    public:
        SdfSegment(glm::vec<2, T> s, glm::vec<2, T> e): _start{s}, _end{e}
        {

        }

        ~SdfSegment() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfSegment<T>{_start, _end});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            glm::vec<2, T> pa =   p - _start;
            glm::vec<2, T> ba = _end - _start;
            float h = glm::clamp<T>(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0, 1.0);
            return glm::length(pa - ba * h);
        }

    private:
        glm::vec<2, T> _start = glm::vec<2, T>{static_cast<T>(0.0)};
        glm::vec<2, T> _end = glm::vec<2, T>{static_cast<T>(0.0)};
    };

    template<std::floating_point T>
    class SdfTrapezoid: public SdfNode<T>
    {
    public:
        SdfTrapezoid(T l0, T l1, T h): _l0{l0}, _l1{l1}, _h{h}
        {

        }

        ~SdfTrapezoid() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfTrapezoid<T>{_l0, _l1, _h});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            T l0 = _l0*static_cast<T>(0.5);
            T l1 = _l1*static_cast<T>(0.5);
            T h  = _h *static_cast<T>(0.5);
            
            glm::vec<2, T> k1{l1,h};
            glm::vec<2, T> k2{l1-l0,2.0*h};
            p.x = abs(p.x);
            glm::vec<2, T> ca = glm::vec<2, T>(p.x-glm::min(p.x,(p.y<0.0)?l0:l1), glm::abs(p.y)-h);
            glm::vec<2, T> cb = p - k1 + k2*glm::clamp<T>( glm::dot(k1-p,k2)/glm::dot(k2, k2), 0.0, 1.0 );
            float s = (cb.x<0.0 && ca.y<0.0) ? -1.0 : 1.0;
            return s*sqrt( glm::min<T>(glm::dot(ca, ca),glm::dot(cb, cb)) );
        }

    private:
        T _l0 = static_cast<T>(0.0);
        T _l1 = static_cast<T>(0.0);
        T _h  = static_cast<T>(0.0);
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
		TextureData(unsigned int width, unsigned int height, texel_type initialValue)
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