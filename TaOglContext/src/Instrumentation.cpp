
#include "Instrumentation.h"
#include <vector>
#include <string>
#include <chrono>

namespace tao_instrument
{

    bool GpuStopwatch::HasResult(query_pair& p)
    {
        int start = 0;
        int end = 0;

        p.startTimeQuery->GetIntegerv(tao_ogl_resources::query_result_available, &start);
        p.endTimeQuery->GetIntegerv(tao_ogl_resources::query_result_available, &end);

        return start&&end;
    }



    template<Stopwatch::TimeFormat fmt>
    unsigned long long int GpuStopwatch::GetResult(query_pair& p)
    {
        GLint64 start = 0;
        GLint64 end = 0;

        p.startTimeQuery->GetInteger64v(tao_ogl_resources::query_result_no_wait, &start);
        p.endTimeQuery->GetInteger64v(tao_ogl_resources::query_result_no_wait, &end);

        unsigned long long int ns_count = end - start;

        switch (fmt) {
            case Stopwatch::NANOSECONDS:
            {
                return ns_count;
            }
            case Stopwatch::MICROSECONDS:
            {
                std::uint64_t up = ((ns_count / 100) % 10 >= 5) ? 1 : 0;
                const auto mus_count = (ns_count / 1000) + up;
                return mus_count;
            }
            case Stopwatch::MILLISECONDS:
            {
                std::uint64_t up = ((ns_count / 100000) % 10 >= 5) ? 1 : 0;
                const auto ms_count = (ns_count / 1000000) + up;
                return ms_count;
            }
            case Stopwatch::SECONDS:
            {
                std::uint64_t up = ((ns_count / 100000000) % 10 >= 5) ? 1 : 0;
                const auto s_count = (ns_count / 1000000000) + up;
                return s_count;
            }
        }
    }

    void GpuStopwatch::IssueNewPair(const std::string &name)
    {
        if(!_stopwatches.contains(name))
        {
            named_stopwatch stopwatch{};
            _stopwatches.emplace(name, std::move(stopwatch));
        }

        _stopwatches.at(name).queries.push(query_pair
           {
                   .startTimeQuery=std::make_shared<tao_ogl_resources::OglQuery>(_rc.CreateQuery()),
                   .endTimeQuery=std::make_shared<tao_ogl_resources::OglQuery>(_rc.CreateQuery())
           });
    }

    GpuStopwatch::gpu_stopwatch GpuStopwatch::Start(const std::string &name)
    {
        if(_stopwatches.contains(name) && !(_stopwatches.at(name).state == ended))
            throw std::runtime_error("Bad GpuStopwatch usage: the given name is not valid");

        IssueNewPair(name);

        auto& sw = _stopwatches.at(name);

        sw.state = started;
        sw.queries.back().startTimeQuery->QueryCounter(tao_ogl_resources::query_timestamp);

        return gpu_stopwatch{name};
    }

    template<Stopwatch::TimeFormat fmt>
    unsigned long long int GpuStopwatch::Stop(const gpu_stopwatch &stopwatch)
    {
        const std::string& name = stopwatch.name;
        long long result = 0;

        if(!_stopwatches.contains(name) || !(_stopwatches.at(name).state == started))
            throw std::runtime_error("Bad GpuStopwatch usage: the given name is not valid");

        auto& sw = _stopwatches.at(name);

        sw.state = ended;
        sw.queries.back().endTimeQuery->QueryCounter(tao_ogl_resources::query_timestamp);

        if(HasResult(sw.queries.front()))
        {
            result = GetResult<fmt>(sw.queries.front());
            sw.queries.pop();
        }

        return result;
    }

    template unsigned long long GpuStopwatch::GetResult<nanoseconds>(tao_instrument::GpuStopwatch::query_pair &p);
    template unsigned long long GpuStopwatch::GetResult<microseconds>(tao_instrument::GpuStopwatch::query_pair &p);
    template unsigned long long GpuStopwatch::GetResult<milliseconds>(tao_instrument::GpuStopwatch::query_pair &p);
    template unsigned long long GpuStopwatch::GetResult<seconds>(tao_instrument::GpuStopwatch::query_pair &p);
    template unsigned long long GpuStopwatch::Stop<nanoseconds>(const tao_instrument::GpuStopwatch::gpu_stopwatch &stopwatch);
    template unsigned long long GpuStopwatch::Stop<microseconds>(const tao_instrument::GpuStopwatch::gpu_stopwatch &stopwatch);
    template unsigned long long GpuStopwatch::Stop<milliseconds>(const tao_instrument::GpuStopwatch::gpu_stopwatch &stopwatch);
    template unsigned long long GpuStopwatch::Stop<seconds>(const tao_instrument::GpuStopwatch::gpu_stopwatch &stopwatch);
}