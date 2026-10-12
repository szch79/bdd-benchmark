#include "../cnf.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_cnf<ruddy_bdd_adapter>(argc, argv);
}
