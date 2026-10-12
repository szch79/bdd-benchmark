#include "../mcnet.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_mcnet<simpleabc_bcdd_adapter>(argc, argv);
}
