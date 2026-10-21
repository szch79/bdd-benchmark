#include "../relprod.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_relprod<rsdd_bcdd_adapter>(argc, argv);
}
