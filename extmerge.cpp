#include <stdio.h>
#include <vector>
#include <queue>
#include <cstdint>

int main(int argc, char const *argv[]) {
  int bufsize = 2000000;
  if (argc < 3) {
    fprintf(stderr, "usage: ./extmerge <out> <files..>\n");
    return 2;
  }
  int nfiles = argc - 2;
  FILE *fout = fopen(argv[1], "wb");
  if (!fout) {
    fprintf(stderr, "cannot open file %s\n", argv[1]);
    return 1;
  }
  std::vector<FILE*> files(nfiles);
  std::vector<uint64_t> buf(nfiles * bufsize);
  std::vector<int> pos(nfiles);
  std::vector<int> total(nfiles);
  typedef std::pair<uint64_t, int> typep;
  std::priority_queue<typep, std::vector<typep>, std::greater<typep>> pq;
  for (int i = 0; i < nfiles; i++) {
    files[i] = fopen(argv[i+2], "rb");
    if (!files[i]) {
      fprintf(stderr, "cannot open file %s\n", argv[i+2]);
      return 1;
    }
    total[i] = fread(buf.data()+i*bufsize, sizeof(uint64_t), bufsize, files[i]);
    if (total[i] > 0) {
      pq.push(typep(buf[i*bufsize], i));
      pos[i] += 1;
    }
  }
  
  std::vector<uint64_t> out;
  while (!pq.empty()) {
    typep cho = pq.top();
    pq.pop();
    out.push_back(cho.first);
    if (out.size() == bufsize) {
      fwrite(out.data(), sizeof(uint64_t), bufsize, fout);
      out.clear();
    }
    int id = cho.second;
    if (pos[id] == total[id] && total[id] == bufsize) {
      // still has data
      total[id] = fread(buf.data()+id*bufsize, sizeof(uint64_t), bufsize, files[id]);
      pos[id] = 0;
      if (total[id] > 0) {
        pq.push(typep(buf[id*bufsize], id));
        pos[id] += 1;
      }
    }
    else if (pos[id] < total[id]) {
      pq.push(typep(buf[id*bufsize + pos[id]], id));
      pos[id] += 1;
    }
  }
  fwrite(out.data(), sizeof(uint64_t), out.size(), fout);
  fclose(fout);
  for (int i = 0; i < nfiles; i++) {
    fclose(files[i]);
  }
  return 0;
}
