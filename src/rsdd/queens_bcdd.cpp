#include "../queens.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_queens<rsdd_bcdd_adapter>(argc, argv);
}
