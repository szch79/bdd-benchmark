#include "../mcnet.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_mcnet<rsdd_bcdd_adapter>(argc, argv);
}
