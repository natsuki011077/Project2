#include <fstream>
using namespace std;

int main(int argc, char* argv[]) {
  ofstream outfile;
  outfile.open("gen.txt");
  int i = (1 << 16);

  for(;i>0;i--)
    outfile << '1';
  outfile.close();
  return 0;
}
