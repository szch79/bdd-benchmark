#include "../cnf.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_cnf<simpleabc_bcdd_adapter>(argc, argv);
}
