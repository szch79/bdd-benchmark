#include "../game-of-life.cpp"

#include "adapter.h"

int
main(int argc, char** argv)
{
  return run_gameoflife<ruddy_bdd_adapter>(argc, argv);
}
