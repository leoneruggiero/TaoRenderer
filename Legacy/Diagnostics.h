#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <glad/glad.h>
#include <string>
#include <vector>

struct NamedBufferedQuery
{
public:
    std::string name;
    GLuint64 value;
    
    NamedBufferedQuery(const char* queryName, unsigned int frontQuery, unsigned int backQuery)
        : name(queryName), _front(frontQuery), _back(backQuery), value(0), _backStarted(false), _frontStarted(false)
    {}

    unsigned int QueryName()
    {
        return
            _switch
            ? _front
            : _back;
    }

    bool GetStarted()
    {
        return
            _switch
            ? _frontStarted
            : _backStarted;
    }

    void SetStarted(bool val)
    {
        if (_switch)
            _frontStarted = val;
        else
            _backStarted = val;
    }
    void Swap()
    {
        _switch = !_switch;
    }



private:
    bool _switch;
    unsigned int _front, _back;
    bool _frontStarted, _backStarted;
};

constexpr unsigned char PROFILER_COLORS_4RGB[]
{
    58, 214, 202, 
    123, 235, 174, 
    103, 212, 99, 
    197, 247, 116, 
};



class Profiler {

public:
    static Profiler& Instance() 
    {
        static Profiler p;
        return p;
    }

    void StartNamedStopWatch(const char* name)
    {
        int index = -1;
        for (int i = 0; i < _queries.size(); i++)
        {
            if (_queries[i].name._Equal(name))
                index = i;
        }

        if (index == -1)
            index = AddNamedQuery(name);

        StartQuery(index);
    }

    long StopNamedStopWatch(const char* name)
    {
        int index = -1;
        for (int i = 0; i < _queries.size(); i++)
        {
            if (_queries[i].name._Equal(name))
                index = i;
        }

        if (index == -1)
            throw "Named stopwatch not found";

        return StopQuery(index);
    }

    const std::vector<NamedBufferedQuery>& GetAllNamedQueries()
    {
        return _queries;
    }
    
private:
    Profiler() : _queries()
    {

    }
    Profiler(const Profiler&) = delete;
    Profiler(const Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;
    Profiler& operator=(Profiler&) = delete;

    std::vector<NamedBufferedQuery> _queries;

    int AddNamedQuery(const char* name)
    {
        unsigned int frontQuery, backQuery;
        glGenQueries(1, &frontQuery);
        glGenQueries(1, &backQuery);

        _queries.push_back(
            NamedBufferedQuery(name, frontQuery, backQuery));

        return _queries.size() - 1;
    }

    void StartQuery(int index)
    {
        if (_queries[index].GetStarted())
            throw "";

        glBeginQuery(GL_TIME_ELAPSED, _queries[index].QueryName());
        _queries[index].SetStarted(true);

    }

    float StopQuery(int index)
    {
        
        glEndQuery(GL_TIME_ELAPSED);

        _queries[index].Swap();

        if (!_queries[index].GetStarted())
            return 0;

        int done = 0;
        while (!done) {
            glGetQueryObjectiv(_queries[index].QueryName(),
                GL_QUERY_RESULT_AVAILABLE,
                &done);
        }

        glGetQueryObjectui64v(_queries[index].QueryName(), GL_QUERY_RESULT, &_queries[index].value);

        _queries[index].SetStarted(false);

        return ((long)_queries[index].value) / 1000000.0f;
    }
};

#endif
