#include "../relprod.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_relprod<ruddy_bdd_adapter>(argc, argv);
}
