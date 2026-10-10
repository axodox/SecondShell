param([string]$Dir, [int]$Seconds = 60)
Add-Type -AssemblyName System.Windows.Forms
$refs = @("System.Windows.Forms", "$Dir\MSTSCLib.dll", "$Dir\AxMSTSCLib.dll")
[void][Reflection.Assembly]::LoadFrom("$Dir\MSTSCLib.dll")
[void][Reflection.Assembly]::LoadFrom("$Dir\AxMSTSCLib.dll")
Add-Type -ReferencedAssemblies $refs -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using MSTSCLib;
using AxMSTSCLib;

public static class CredTest
{
    const string Target = "SecondShell:ChildSession";

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct CREDENTIAL
    {
        public uint Flags; public uint Type; public string TargetName; public string Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public uint CredentialBlobSize; public IntPtr CredentialBlob; public uint Persist;
        public uint AttributeCount; public IntPtr Attributes; public string TargetAlias; public string UserName;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct CREDUI_INFO { public int cbSize; public IntPtr hwndParent; public string pszMessageText; public string pszCaptionText; public IntPtr hbmBanner; }

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CredRead(string target, uint type, uint flags, out IntPtr cred);
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CredWrite(ref CREDENTIAL cred, uint flags);
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CredDelete(string target, uint type, uint flags);
    [DllImport("advapi32.dll")] static extern void CredFree(IntPtr p);
    [DllImport("credui.dll", CharSet = CharSet.Unicode)] static extern int CredUIPromptForCredentials(ref CREDUI_INFO info, string target, IntPtr reserved, int authError, StringBuilder user, int userMax, StringBuilder pass, int passMax, ref bool save, uint flags);
    [DllImport("credui.dll", CharSet = CharSet.Unicode)] static extern int CredUIParseUserName(string user, StringBuilder name, int nameMax, StringBuilder domain, int domainMax);

    static void Log(string m) { Console.WriteLine("[{0:HH:mm:ss.fff}] {1}", DateTime.Now, m); }

    static bool Load(out string user, out string pass)
    {
        user = pass = null;
        IntPtr p;
        if (!CredRead(Target, 1, 0, out p)) return false;
        var c = (CREDENTIAL)Marshal.PtrToStructure(p, typeof(CREDENTIAL));
        user = c.UserName;
        pass = Marshal.PtrToStringUni(c.CredentialBlob, (int)c.CredentialBlobSize / 2);
        CredFree(p);
        return true;
    }

    static void Save(string user, string pass)
    {
        byte[] blob = Encoding.Unicode.GetBytes(pass);
        IntPtr b = Marshal.AllocHGlobal(blob.Length);
        Marshal.Copy(blob, 0, b, blob.Length);
        var c = new CREDENTIAL { Type = 1, TargetName = Target, UserName = user, CredentialBlob = b, CredentialBlobSize = (uint)blob.Length, Persist = 2 /* LOCAL_MACHINE */ };
        bool ok = CredWrite(ref c, 0);
        Marshal.FreeHGlobal(b);
        Log("CredWrite: " + ok);
    }

    static bool Prompt(out string user, out string pass)
    {
        var info = new CREDUI_INFO { cbSize = Marshal.SizeOf(typeof(CREDUI_INFO)), pszCaptionText = "SecondShell", pszMessageText = "Enter your Windows password to sign in to the child session (saved after successful sign-in)." };
        var u = new StringBuilder(Environment.UserDomainName + "\\" + Environment.UserName, 514);
        var pw = new StringBuilder(256);
        bool save = false;
        // GENERIC_CREDENTIALS | ALWAYS_SHOW_UI | DO_NOT_PERSIST | KEEP_USERNAME? (no) -> 0x40000 | 0x80 | 0x2
        int r = CredUIPromptForCredentials(ref info, Target, IntPtr.Zero, 0, u, u.Capacity, pw, pw.Capacity, ref save, 0x40000 | 0x80 | 0x2);
        user = u.ToString(); pass = pw.ToString();
        return r == 0;
    }

    public static void Run(int seconds)
    {
        string user, pass;
        bool fromStore = Load(out user, out pass);
        Log(fromStore ? "Using stored credential for " + user : "No stored credential, prompting");
        if (!fromStore && !Prompt(out user, out pass)) { Log("Prompt cancelled"); return; }

        var name = new StringBuilder(514); var domain = new StringBuilder(338);
        if (CredUIParseUserName(user, name, name.Capacity, domain, domain.Capacity) != 0) { name.Clear().Append(user); domain.Clear(); }

        var form = new Form { Text = "SecondShell credential test", Width = 900, Height = 700 };
        var rdp = new AxMsRdpClient7NotSafeForScripting { Dock = DockStyle.Fill };
        form.Controls.Add(rdp);
        form.Show();
        Application.DoEvents();

        object ocx = rdp.GetOcx();
        rdp.Server = "localhost";
        rdp.UserName = name.ToString();
        rdp.Domain = domain.ToString();
        rdp.AdvancedSettings7.ClearTextPassword = pass;
        rdp.DesktopWidth = 1024; rdp.DesktopHeight = 768;
        rdp.AdvancedSettings7.EnableCredSspSupport = true;
        object t = true;
        ((IMsRdpExtendedSettings)ocx).set_Property("ConnectToChildSession", ref t);
        var ns = (IMsRdpClientNonScriptable5)ocx;
        ns.AllowPromptingForCredentials = false; ns.PromptForCredentials = false;

        bool done = false, loggedIn = false;
        rdp.OnConnected += (s, e) => Log("OnConnected");
        rdp.OnLoginComplete += (s, e) => { Log("OnLoginComplete"); loggedIn = true; done = true; };
        rdp.OnLogonError += (s, e) => Log("OnLogonError " + e.lError);
        rdp.OnDisconnected += (s, e) =>
        {
            Log(string.Format("OnDisconnected discReason={0} ext={1}", e.discReason, rdp.ExtendedDisconnectReason));
            if (e.discReason == 2055 && fromStore) { Log("Stored credential rejected, deleting it"); CredDelete(Target, 1, 0); }
            done = true;
        };

        Log("Connecting as " + (domain.Length > 0 ? domain + "\\" : "") + name);
        rdp.Connect();
        var sw = Stopwatch.StartNew();
        while (!done && sw.Elapsed.TotalSeconds < seconds) { Application.DoEvents(); System.Threading.Thread.Sleep(50); }

        if (loggedIn)
        {
            if (!fromStore) Save(user, pass);
            var w = Stopwatch.StartNew();
            while (w.Elapsed.TotalSeconds < 3) { Application.DoEvents(); System.Threading.Thread.Sleep(50); }
            var p = Process.Start(new ProcessStartInfo(Environment.ExpandEnvironmentVariables(@"%WINDIR%\Sysnative\qwinsta.exe")) { UseShellExecute = false, RedirectStandardOutput = true });
            Console.Write(p.StandardOutput.ReadToEnd());
            rdp.Disconnect();
            w.Restart();
            while (rdp.Connected != 0 && w.Elapsed.TotalSeconds < 5) { Application.DoEvents(); System.Threading.Thread.Sleep(50); }
        }
        form.Close();
        Log("Result: " + (loggedIn ? "SUCCESS" : "FAILED"));
    }
}
'@
[CredTest]::Run($Seconds)
