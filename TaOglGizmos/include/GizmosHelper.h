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
    class SdfOpSubtract;
    template<std::floating_point T>
    class SdfOpIntersect;
    template<std::floating_point T>
    class SdfOpRepeatRadial;
    template<std::floating_point T>
    class SdfOpRepeatGrid;

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

        SdfOpInflate<T>      Inflate(T i);
        SdfOpShell<T>        Shell(T t);
        SdfOpAdd<T>          Add(const SdfNode& other);
        SdfOpSubtract<T>     Subtract(const SdfNode& other);
        SdfOpIntersect<T>    Intersect(const SdfNode& other);
        SdfOpRepeatRadial<T> RepeatRadial(int r);
        SdfOpRepeatGrid<T>   RepeatGrid(const glm::vec<2, T>& i);
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
    class SdfOpRepeatRadial: public SdfNode<T>
    {
    public:
        SdfOpRepeatRadial(std::shared_ptr<SdfNode<T>> op1, int r):
                _repetitions{r},
                _operand1{op1}
        {
        }

        virtual ~SdfOpRepeatRadial() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpRepeatRadial<T>{_operand1, _repetitions});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            // from: https://www.ronja-tutorials.com/post/036-sdf-space-manipulation/#radial-cells
            T cellSize = pi<T>() * static_cast<T>(2.0) / static_cast<T>(_repetitions);
            glm::vec<2, T> radialPosition = glm::vec<2, T>(glm::atan(p.y, p.x), glm::length(p));
            radialPosition.x = glm::mod(radialPosition.x, cellSize);

            // center the angle interval around positive X axis
            radialPosition.x-=cellSize*static_cast<T>(0.5);
            p.x = glm::cos(radialPosition.x);
            p.y = glm::sin(radialPosition.x);

            p = p * radialPosition.y;

            return _operand1->Evaluate(p);
        }

    private:
        int _repetitions = 1;
        std::shared_ptr<SdfNode<T>> _operand1;
    };

    template<std::floating_point T>
    class SdfOpRepeatGrid: public SdfNode<T>
    {
    public:
        SdfOpRepeatGrid(std::shared_ptr<SdfNode<T>> op1, glm::vec<2, T> i):
                _interval{i},
                _operand1{op1}
        {
        }

        virtual ~SdfOpRepeatGrid() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpRepeatGrid<T>{_operand1, _interval});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            p = glm::mod(p, _interval);

            return _operand1->Evaluate(p);
        }

    private:
        glm::vec<2, T> _interval = glm::vec<2, T>{static_cast<T>(0.0)};
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
    class SdfOpSubtract: public SdfNode<T>
    {
    public:
        SdfOpSubtract(std::shared_ptr<SdfNode<T>> op1, std::shared_ptr<SdfNode<T>> op2):
                _operand1{op1},
                _operand2{op2}
        {
        }

        virtual ~SdfOpSubtract() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpSubtract<T>{_operand1, _operand2});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return glm::max<T>(_operand1->Evaluate(p), -_operand2->Evaluate(p));
        }

    private:
        std::shared_ptr<SdfNode<T>> _operand1;
        std::shared_ptr<SdfNode<T>> _operand2;
    };

    template<std::floating_point T>
    class SdfOpIntersect: public SdfNode<T>
    {
    public:
        SdfOpIntersect(std::shared_ptr<SdfNode<T>> op1, std::shared_ptr<SdfNode<T>> op2):
                _operand1{op1},
                _operand2{op2}
        {
        }

        virtual ~SdfOpIntersect() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfOpIntersect<T>{_operand1, _operand2});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            return glm::max<T>(_operand1->Evaluate(p), _operand2->Evaluate(p));
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
    SdfOpSubtract<T> SdfNode<T>::Subtract(const SdfNode<T> &other)
    {
        return SdfOpSubtract<T>{this->MakeNode(), other.MakeNode()};
    }

    template<std::floating_point T>
    SdfOpIntersect<T> SdfNode<T>::Intersect(const SdfNode<T> &other)
    {
        return SdfOpIntersect<T>{this->MakeNode(), other.MakeNode()};
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
    SdfOpRepeatRadial<T> SdfNode<T>::RepeatRadial(int r)
    {
        return SdfOpRepeatRadial<T>{this->MakeNode(), r};
    }

    template<std::floating_point T>
    SdfOpRepeatGrid<T> SdfNode<T>::RepeatGrid(const glm::vec<2, T>& i)
    {
        return SdfOpRepeatGrid<T>{this->MakeNode(), i};
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

    template<std::floating_point T>
    class SdfRoundedRect: public SdfNode<T>
    {
    public:
        SdfRoundedRect(glm::vec<2, T> s, glm::vec<4, T> r): _s{s}, _r{r}
        {

        }

        ~SdfRoundedRect() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfRoundedRect<T>{_s, _r});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            glm::vec<2, T> rr =
                    (p.x>static_cast<T>(0.0))
                    ? glm::vec<2, T>{_r.x, _r.y}
                    : glm::vec<2, T>{_r.z, _r.w};

            rr.x  = (p.y>0.0) ? rr.x  : rr.y;

            glm::vec<2, T> q = glm::abs(p) - (static_cast<T>(0.5) * _s) + rr.x;

            return glm::min(glm::max(q.x,q.y),static_cast<T>(0.0)) + glm::length(glm::max(q,static_cast<T>(0.0))) - rr.x;
        }

    private:
        glm::vec<2, T> _s = glm::vec<2, T>{static_cast<T>(0.0)};
        glm::vec<4, T> _r = glm::vec<4, T>{static_cast<T>(0.0)};
    };

    template<std::floating_point T>
    class SdfRect: public SdfNode<T>
    {
    public:
        SdfRect(glm::vec<2, T> s): _s{s}
        {

        }

        ~SdfRect() override = default;

        virtual std::shared_ptr<SdfNode<T>> MakeNode() const override
        {
            return std::shared_ptr<SdfNode<T>>(new SdfRect<T>{_s});
        }

        virtual T Evaluate(glm::vec<2, T> p) const override
        {
            glm::vec<2, T> d = glm::abs(p) - static_cast<T>(0.5) * _s;
            return
                glm::length(glm::max(d, glm::vec<2,T>{static_cast<T>(0.0)})) +
                glm::min(glm::max(d.x,d.y),static_cast<T>(0.0));
        }

    private:
        glm::vec<2, T> _s = glm::vec<2, T>{static_cast<T>(0.0)};
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