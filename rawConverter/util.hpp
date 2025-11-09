
#include <chrono>
#include <iostream>

// Macro to capture caller information in a portable way
#if defined(__GNUC__) || defined(__clang__)
    #define CALLER __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define CALLER __FUNCSIG__
#else
    #define CALLER "Unknown Caller"
#endif

struct Timer
{
    Timer(std::string caller)
    : t_(std::chrono::high_resolution_clock::now())
    , caller_(std::move(caller))
    {

    }

    ~Timer()
    {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()-t_);
        std::cout << caller_ << " time elapsed: " << duration.count() << " microseconds" << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> t_;
    std::string caller_;

};

#define TIMER Timer t(CALLER)
