#include "../hamiltonian.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_hamiltonian<rsdd_bcdd_adapter>(argc, argv);
}
