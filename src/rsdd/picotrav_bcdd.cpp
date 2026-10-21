#include "../picotrav.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_picotrav<rsdd_bcdd_adapter>(argc, argv);
}
