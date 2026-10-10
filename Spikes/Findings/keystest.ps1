param([string]$Dir, [int]$Seconds = 15, [string]$WinMode = 'ext')
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

    static void Pump(double seconds) { var w = Stopwatch.StartNew(); while (w.Elapsed.TotalSeconds < seconds) { Application.DoEvents(); System.Threading.Thread.Sleep(20); } }

    static void Key(IMsRdpClientNonScriptable ns, int scan, bool up) { bool u = up; int s = scan; ns.SendKeys(1, ref u, ref s); Pump(0.03); }
    static void Tap(IMsRdpClientNonScriptable ns, int scan) { Key(ns, scan, false); Key(ns, scan, true); }

    [DllImport("wtsapi32.dll")] static extern bool WTSGetChildSessionId(out int id);

    static int CountNotepadInChild()
    {
        int id; WTSGetChildSessionId(out id);
        int n = 0;
        foreach (var p in Process.GetProcessesByName("notepad")) if (p.SessionId == id) n++;
        return n;
    }

    public static void Run(int seconds, string winScanMode)
    {
        string user, pass;
        if (!Load(out user, out pass)) { Log("No stored credential"); return; }
        var name = new StringBuilder(514); var domain = new StringBuilder(338);
        if (CredUIParseUserName(user, name, name.Capacity, domain, domain.Capacity) != 0) { name.Clear().Append(user); domain.Clear(); }

        var form = new Form { Text = "SecondShell keys test", Width = 900, Height = 700 };
        var rdp = new AxMsRdpClient7NotSafeForScripting { Dock = DockStyle.Fill };
        form.Controls.Add(rdp);
        form.Show();
        Application.DoEvents();

        object ocx = rdp.GetOcx();
        rdp.Server = "localhost";
        rdp.UserName = name.ToString(); rdp.Domain = domain.ToString();
        rdp.AdvancedSettings7.ClearTextPassword = pass;
        rdp.DesktopWidth = 1024; rdp.DesktopHeight = 768;
        rdp.AdvancedSettings7.EnableCredSspSupport = true;
        rdp.AdvancedSettings6.RedirectClipboard = false;
        object t = true;
        ((IMsRdpExtendedSettings)ocx).set_Property("ConnectToChildSession", ref t);
        var ns5 = (IMsRdpClientNonScriptable5)ocx;
        ns5.AllowPromptingForCredentials = false; ns5.PromptForCredentials = false;
        var ns = (IMsRdpClientNonScriptable)ocx;

        bool loggedIn = false, failed = false;
        rdp.OnLoginComplete += (s, e) => { Log("OnLoginComplete"); loggedIn = true; };
        rdp.OnDisconnected += (s, e) => { Log("OnDisconnected " + e.discReason); failed = true; };
        rdp.Connect();
        var sw = Stopwatch.StartNew();
        while (!loggedIn && !failed && sw.Elapsed.TotalSeconds < 60) { Application.DoEvents(); System.Threading.Thread.Sleep(20); }
        if (!loggedIn) { Log("Login failed"); form.Close(); return; }

        Log("Waiting for shell to be ready...");
        Pump(seconds);
        Log("Notepad in child before: " + CountNotepadInChild());

        // Win key: try the requested encoding of the extended scancode 0x5B
        int win = winScanMode == "plain" ? 0x5B : winScanMode == "e0" ? 0xE05B : 0x15B;
        Log("Sending Win+R with Win scancode 0x" + win.ToString("X"));
        Key(ns, win, false); Tap(ns, 0x13 /* R */); Key(ns, win, true);
        Pump(1.5);

        // "secondshelltest" in set-1 scancodes (no layout-sensitive letters)
        var map = new System.Collections.Generic.Dictionary<char, int> {
            {'s',0x1F},{'e',0x12},{'c',0x2E},{'o',0x18},{'n',0x31},{'d',0x20},{'h',0x23},{'l',0x26},{'t',0x14} };
        foreach (char ch in "secondshelltest") Tap(ns, map[ch]);
        Pump(0.3);
        Tap(ns, 0x1C /* Enter */);
        Pump(4);
        int after = CountNotepadInChild();
        Log("Notepad in child after: " + after);
        Log("Result: " + (after > 0 ? "SUCCESS" : "NOT STARTED"));

        rdp.Disconnect();
        Pump(2);
        form.Close();
    }
}
'@
[CredTest]::Run($Seconds, $WinMode)
