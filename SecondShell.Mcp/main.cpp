#include "pch.h"
#include "ProductInfo.h"

using namespace SecondShell::Environment;
using namespace std;

int main()
{
  //The MCP server is implemented in stage 4; until then, serve nothing and exit when the client closes stdin.
  cerr << ProductInfo::Name() << " MCP server " << ProductInfo::Version() << endl;

  string line;
  while (getline(cin, line)) { }

  return 0;
}