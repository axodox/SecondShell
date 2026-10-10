param([string]$Dir, [int]$Seconds = 25)
Add-Type -AssemblyName System.Windows.Forms
$refs = @("System.Windows.Forms", "$Dir\MSTSCLib.dll", "$Dir\AxMSTSCLib.dll")
[void][Reflection.Assembly]::LoadFrom("$Dir\MSTSCLib.dll")
[void][Reflection.Assembly]::LoadFrom("$Dir\AxMSTSCLib.dll")
Add-Type -ReferencedAssemblies $refs -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Windows.Forms;
using MSTSCLib;
using AxMSTSCLib;

public static class ChildTest
{
    static void Log(string m) { Console.WriteLine("[{0:HH:mm:ss.fff}] {1}", DateTime.Now, m); }

    public static void Run(int seconds)
    {
        var form = new Form { Text = "ChildSessionTest", Width = 900, Height = 700 };
        var rdp = new AxMsRdpClient7NotSafeForScripting { Dock = DockStyle.Fill };
        form.Controls.Add(rdp);
        form.Show();
        Application.DoEvents();

        object ocx = rdp.GetOcx();
        rdp.Server = "localhost";
        rdp.DesktopWidth = 1024; rdp.DesktopHeight = 768;
        rdp.AdvancedSettings7.EnableCredSspSupport = true;
        object t = true;
        ((IMsRdpExtendedSettings)ocx).set_Property("ConnectToChildSession", ref t);
        var ns = (IMsRdpClientNonScriptable5)ocx;
        ns.AllowPromptingForCredentials = false; ns.PromptForCredentials = false;

        bool done = false;
        rdp.OnConnected += (s, e) => Log("OnConnected");
        rdp.OnLoginComplete += (s, e) => Log("OnLoginComplete");
        rdp.OnLogonError += (s, e) => Log("OnLogonError " + e.lError);
        rdp.OnDisconnected += (s, e) =>
        {
            string desc = ((IMsRdpClient5)ocx).GetErrorDescription((uint)e.discReason, (uint)rdp.ExtendedDisconnectReason);
            Log(string.Format("OnDisconnected discReason={0} ext={1} desc='{2}'", e.discReason, rdp.ExtendedDisconnectReason, desc));
            done = true;
        };

        Log("Connecting...");
        rdp.Connect();
        var sw = Stopwatch.StartNew();
        while (!done && sw.Elapsed.TotalSeconds < seconds) { Application.DoEvents(); System.Threading.Thread.Sleep(50); }
        Log("Connected state: " + rdp.Connected);
        Log("Sessions:");
        var p = Process.Start(new ProcessStartInfo(Environment.ExpandEnvironmentVariables(@"%WINDIR%\Sysnative\qwinsta.exe")) { UseShellExecute = false, RedirectStandardOutput = true });
        Console.Write(p.StandardOutput.ReadToEnd());
        if (rdp.Connected == 1)
        {
            rdp.Disconnect();
            var w = Stopwatch.StartNew();
            while (rdp.Connected != 0 && w.Elapsed.TotalSeconds < 5) { Application.DoEvents(); System.Threading.Thread.Sleep(50); }
        }
        form.Close();
        Log("Done");
    }
}
'@
[ChildTest]::Run($Seconds)
