#include "../queens.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_queens<aytdd_bcdd_adapter>(argc, argv);
}
