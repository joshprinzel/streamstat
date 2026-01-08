#include <iostream>
#include "fastnum/running_stats.hpp"

int main() {
  std::cout << "streamfeat starting...\n";
  fastnum::RunningStats<double> rs;
  for (int i = 1; i <= 10; ++i) rs.observe(i);

  std::cout << "n=" << rs.count()
            << " mean=" << rs.mean()
            << " var(sample)=" << rs.variance_sample()
            << ":)\n";
  return 0;
}
