#include "pch.h"
#include "McpServer.h"
#include "ProductInfo.h"
#include "SessionToolSource.h"
#include "StdioTransport.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Environment;
using namespace SecondShell::Mcp;
using namespace SecondShell::Rpc;
using namespace std;

//The composition root: serves MCP on stdio until the client closes stdin.
int main()
{
  auto transport = make_shared<StdioTransport>();

  dependency_container container;
  container.add<IMessageTransport>(transport);
  container.resolve<registry<ToolSource>>()->add_permanent(container.resolve<SessionToolSource>());

  logger{ "Main" }.log(log_severity::information, "{} MCP server {} started.", ProductInfo::Name(), ProductInfo::Version());
  container.resolve<McpServer>()->Start();
  transport->Run();
  return 0;
}
