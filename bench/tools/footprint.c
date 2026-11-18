// Prints "<pid> <phys_footprint_kb>" for each live pid given as an argument (macOS).
// phys_footprint is what Activity Monitor shows as "Memory": unlike ps RSS it does not count
// clean file-backed (e.g. mmap'd SQLite) pages once per mapping.
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    int pid = atoi(argv[i]);
    struct rusage_info_v4 ri;
    if (proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t*)&ri) == 0)
      printf("%d %llu\n", pid, (unsigned long long)(ri.ri_phys_footprint / 1024));
  }
  return 0;
}
