#ifndef BDD_BENCHMARK_COMMON_ADAPTER_H
#define BDD_BENCHMARK_COMMON_ADAPTER_H

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <cassert>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

#include "./chrono.h"
#include "./input.h"
#include "./json.h"

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// \brief Integer logarithm floor(log2(n))
///
/// \param n  Must not be 0
////////////////////////////////////////////////////////////////////////////////
constexpr unsigned
ilog2(unsigned long long n)
{
  assert(n > 0);

#ifdef __GNUC__ // GCC and Clang support `__builtin_clzll`
  // "clz" stands for count leading zero bits. The builtin function may be
  // implemented more efficiently than the loop below.
  return sizeof(unsigned long long) * 8 - __builtin_clzll(n) - 1;
#else
  unsigned exp           = 1u;
  unsigned long long val = 2u; // 2^1
  while (val < n) {
    val <<= 1u;
    exp++;
  }

  return exp;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Converts a model count computed in floating point to the integer the
///        benchmarks print.
///
/// \details The conversion is exact below 2^53. A count that is not a number,
///          is negative, or is 2^64 or more has no such integer, and casting it
///          is undefined behaviour, so the run ends instead with a message on
///          the standard error and the exit status 5. It does not throw, since
///          an exception that passes through the frames of a package's worker
///          threads aborts the process and truncates its output.
////////////////////////////////////////////////////////////////////////////////
inline uint64_t
checked_satcount(const double count)
{
  if (!(count >= 0.0 && count < 18446744073709551616.0)) {
    std::cerr << "The model count " << count << " is not an integer below 2^64" << std::endl;
    std::exit(5);
  }
  return static_cast<uint64_t>(count);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Ends a run that the BDD package cannot continue for lack of memory.
///
/// \details The package's own message is printed to the standard error, and
///          the process exits with the status 4, which the benchmarks reserve
///          for this outcome, as they reserve 3 for a task the package does not
///          support and 5 for a model count that `checked_satcount` cannot
///          convert.
////////////////////////////////////////////////////////////////////////////////
[[noreturn]] inline void
exit_out_of_memory(const std::string& message)
{
  std::cerr << message << std::endl;
  std::exit(4);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Print resource usage as JSON object
////////////////////////////////////////////////////////////////////////////////
struct resource_usage
{
  const rusage& before;
  const rusage& after;
  const uint64_t elapsed_ms;

  template <class Elem, class Traits>
  friend std::basic_ostream<Elem, Traits>&
  operator<<(std::basic_ostream<Elem, Traits>& os, const resource_usage& self)
  {
    const rusage &b = self.before, &a = self.after;

    const uint64_t stime = (a.ru_stime.tv_sec - b.ru_stime.tv_sec) * 1000LL
      + (a.ru_stime.tv_usec - b.ru_stime.tv_usec) / 1000;
    const uint64_t utime = (a.ru_utime.tv_sec - b.ru_utime.tv_sec) * 1000LL
      + (a.ru_utime.tv_usec - b.ru_utime.tv_usec) / 1000;

    // `getrusage` reports the maximum resident set in bytes on macOS and in kibibytes elsewhere
#ifdef __APPLE__
    const uint64_t maxrss_mib = a.ru_maxrss / (1024 * 1024);
#else
    const uint64_t maxrss_mib = a.ru_maxrss / 1024;
#endif

    os << json::brace_open << json::endl
       << json::field("stime (ms)") << json::value(stime) << json::comma << json::endl
       << json::field("utime (ms)") << json::value(utime) << json::comma << json::endl
       << json::field("CPU (%)");
    // A run that ends within the millisecond has no CPU share to report
    if (self.elapsed_ms > 0) {
      os << json::value((stime + utime) * 100 / self.elapsed_ms);
    } else {
      os << json::nil;
    }
    os << json::comma << json::endl
       << json::field("maximum resident set size (MiB)") << json::value(maxrss_mib)
       << json::comma << json::endl
       << json::field("minor page faults") << json::value(a.ru_minflt - b.ru_minflt) << json::comma
       << json::endl
       << json::field("major page faults") << json::value(a.ru_majflt - b.ru_majflt) << json::comma
       << json::endl
       << json::field("block input operations") << json::value(a.ru_inblock - b.ru_inblock)
       << json::comma << json::endl
       << json::field("block output operations") << json::value(a.ru_oublock - b.ru_oublock)
       << json::comma << json::endl
       << json::field("voluntary context switches") << json::value(a.ru_nvcsw - b.ru_nvcsw)
       << json::comma << json::endl
       << json::field("involuntary context switches") << json::value(a.ru_nivcsw - b.ru_nivcsw)
       << json::endl
       << json::brace_close;
    return os;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// \brief Initializes the BDD package and runs the given benchmark
////////////////////////////////////////////////////////////////////////////////
template <typename Adapter, typename F>
int
run(const std::string& benchmark_name, const int varcount, const F& f)
{
  std::cout << json::brace_open << json::endl;

  std::cout
    // Debug mode
    << json::field("debug_mode")
#ifndef NDEBUG
    << json::value(true)
#else
    << json::value(false)
#endif
    << json::comma
    << json::endl
    // Statistics
    << json::field("statistics")
#ifdef BDD_BENCHMARK_STATS
    << json::value(true)
#else
    << json::value(false)
#endif
    << json::comma << json::endl
    << json::endl
    // BDD package substruct
    << json::field("bdd package") << json::brace_open
    << json::endl
    // Name
    << json::field("name") << json::value(Adapter::name) << json::comma
    << json::endl
    // BDD Type
    << json::field("type") << json::value(Adapter::dd) << json::comma << json::endl;

  const time_point t_before = now();
  Adapter adapter(varcount);
  const time_point t_after = now();

  const time_duration t_duration = duration_ms(t_before, t_after);
#ifdef BDD_BENCHMARK_INCL_INIT
  init_time = t_duration;
#endif // BDD_BENCHMARK_INCL_INIT

  std::cout
    // Initialisation Time
    << json::field("init time (ms)") << json::value(t_duration) << json::comma
    << json::endl
    // Memory
    << json::field("memory (MiB)") << json::value(M) << json::comma
    << json::endl
    // Variables
    << json::field("variables") << json::value(varcount)
    << json::endl
    // ...
    << json::brace_close << json::comma << json::endl
    << json::endl;

  std::cout << json::field("benchmark") << json::brace_open << json::endl;
  std::cout << json::field("name") << json::value(benchmark_name) << json::comma << json::endl
            << json::flush;

  rusage rusage_before;
  getrusage(RUSAGE_SELF, &rusage_before);
  time_point start = now();

  const int exit_code = adapter.run([&]() { return f(adapter); });

  rusage rusage_after;
  getrusage(RUSAGE_SELF, &rusage_after);
  uint64_t elapsed_ms = duration_ms(start, now());

  std::cout << json::brace_close << json::comma << json::endl
            << json::endl
            << json::field("resource usage")
            << resource_usage{ rusage_before, rusage_after, elapsed_ms } << json::endl
            << json::brace_close << json::endl
            << json::flush;

#ifdef BDD_BENCHMARK_STATS
  if (!exit_code) { adapter.print_stats(); }
#endif

#ifdef BDD_BENCHMARK_WAIT
  // TODO: move to 'std::cerr' to keep 'std::cout' pure JSON?
  std::cout << "\npress any key to exit . . .\n" << std::flush;
  ;
  std::getchar();
  std::cout << "\n";
#endif

  return exit_code;
}

#endif // BDD_BENCHMARK_COMMON_ADAPTER_H
