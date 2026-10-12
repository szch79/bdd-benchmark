#include "../apply.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_apply<aytdd_bcdd_adapter>(argc, argv);
}
