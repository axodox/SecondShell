param([string]$Dir, [int]$Seconds = 15, [string]$WinMode = 'ext')
Add-Type -AssemblyName System.Windows.Forms
$refs = @("System.Windows.Forms", "System.Drawing", "$Dir\MSTSCLib.dll", "$Dir\AxMSTSCLib.dll")
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

    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("wtsapi32.dll")] static extern bool WTSLogoffSession(IntPtr server, int sessionId, bool wait);

    static IntPtr FindChild(IntPtr h, string cls)
    {
        var sb = new StringBuilder(128); GetClassName(h, sb, 128);
        if (sb.ToString() == cls) return h;
        for (IntPtr c = GetWindow(h, 5 /* GW_CHILD */); c != IntPtr.Zero; c = GetWindow(c, 2 /* GW_HWNDNEXT */))
        { IntPtr r = FindChild(c, cls); if (r != IntPtr.Zero) return r; }
        return IntPtr.Zero;
    }

    public static IntPtr RdpHandle;
    static string keyPath = "post"; // force posted messages: no SendKeys, no focus needed

    static void Key(IMsRdpClientNonScriptable ns, int scan, bool up)
    {
        if (keyPath != "post")
        {
            try { bool u = up; int s = scan; ns.SendKeys(1, ref u, ref s); if (keyPath == null) { keyPath = "sendkeys"; Log("Key path: SendKeys (foreground=" + (GetForegroundWindow() != IntPtr.Zero) + ")"); } Pump(0.03); return; }
            catch (Exception ex) { Log("SendKeys failed (" + ex.Message.Split('\n')[0] + "), switching to posted messages"); keyPath = "post"; }
        }
        IntPtr ih = FindChild(RdpHandle, "IHWindowClass");
        if (ih == IntPtr.Zero) { Log("IHWindowClass not found"); return; }
        bool ext = (scan & 0x100) != 0 || (scan & 0xFF00) == 0xE000;
        uint sc = (uint)(scan & 0xFF);
        uint vk = ext && sc == 0x5B ? 0x5Bu : MapVirtualKey(sc, 1 /* MAPVK_VSC_TO_VK */);
        long l = 1 | (sc << 16) | (ext ? (1L << 24) : 0) | (up ? (3L << 30) : 0);
        PostMessage(ih, up ? 0x0101u /* WM_KEYUP */ : 0x0100u /* WM_KEYDOWN */, (IntPtr)vk, new IntPtr(unchecked((int)l)));
        Pump(0.03);
    }
    static void Tap(IMsRdpClientNonScriptable ns, int scan) { Key(ns, scan, false); Key(ns, scan, true); }

    [DllImport("wtsapi32.dll")] static extern bool WTSGetChildSessionId(out int id);

    static int CountNotepadInChild()
    {
        int id; WTSGetChildSessionId(out id);
        int n = 0;
        foreach (var p in Process.GetProcessesByName("notepad")) if (p.SessionId == id) n++;
        return n;
    }


    [ComImport, Guid("00000001-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IClassFactory
    {
        [PreserveSig] int CreateInstance([MarshalAs(UnmanagedType.IUnknown)] object outer, ref Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object obj);
        [PreserveSig] int LockServer(bool lockServer);
    }
    [DllImport("ole32.dll")] static extern int CreateBindCtx(int reserved, out System.Runtime.InteropServices.ComTypes.IBindCtx ctx);
    [DllImport("urlmon.dll", CharSet = CharSet.Unicode)] static extern int MkParseDisplayNameEx(System.Runtime.InteropServices.ComTypes.IBindCtx ctx, string name, out int eaten, out System.Runtime.InteropServices.ComTypes.IMoniker moniker);

    // Same approach as the Windows Simulator (COMHelpers.CreateObjectInRemoteSession)
    static object ActivateInSession(int sessionId, Guid clsid, out string info)
    {
        string name = string.Format("Session:{0}!clsid:{1:B}", sessionId, clsid);
        System.Runtime.InteropServices.ComTypes.IBindCtx ctx;
        int hr = CreateBindCtx(0, out ctx);
        if (hr != 0) { info = "CreateBindCtx hr=0x" + hr.ToString("X8"); return null; }
        int eaten; System.Runtime.InteropServices.ComTypes.IMoniker mk;
        hr = MkParseDisplayNameEx(ctx, name, out eaten, out mk);
        if (hr != 0) { info = "MkParseDisplayNameEx(" + name + ") hr=0x" + hr.ToString("X8"); return null; }
        object factoryObj;
        Guid iidFactory = typeof(IClassFactory).GUID;
        try { mk.BindToObject(ctx, null, ref iidFactory, out factoryObj); }
        catch (Exception ex) { info = "BindToObject(" + name + ") failed: 0x" + Marshal.GetHRForException(ex).ToString("X8") + " " + ex.Message; return null; }
        var factory = (IClassFactory)factoryObj;
        Guid iidUnknown = new Guid("00000000-0000-0000-C000-000000000046");
        object obj;
        hr = factory.CreateInstance(null, ref iidUnknown, out obj);
        info = name + " -> CreateInstance hr=0x" + hr.ToString("X8");
        return hr == 0 ? obj : null;
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
        RdpHandle = rdp.Handle;
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

        form.Hide();
        Log("RDP window hidden right after sign-in");
        Log("Waiting for shell to be ready...");
        Pump(seconds);
        var map = new System.Collections.Generic.Dictionary<char, int> {
            {'s',0x1F},{'e',0x12},{'c',0x2E},{'o',0x18},{'n',0x31},{'d',0x20},{'h',0x23},{'l',0x26},{'t',0x14} };
        int childId; WTSGetChildSessionId(out childId);
        var outDir = @"C:\Users\Axodox\AppData\Local\Temp\claude\h--work-Windows-Simulator-16-0\b5eeca30-5c5e-4ed2-aa32-a3a09ea9bf1b\scratchpad\sscapture\out";
        System.IO.Directory.CreateDirectory(outDir);
        System.IO.File.WriteAllText(System.IO.Path.Combine(outDir, "expected_session.txt"), childId.ToString());
        string logPath = System.IO.Path.Combine(outDir, "loop.log");
        Func<string> readLog = () => System.IO.File.Exists(logPath) ? System.IO.File.ReadAllText(logPath) : "";

        // Test 1: launch with posted keyboard messages while the RDP window is hidden
        Log(string.Format("TEST 1: child session {0}, window visible={1}, foreground={2}; launching via posted messages",
            childId, form.Visible, GetForegroundWindow() == form.Handle));
        Tap(ns, 0x01 /* Esc */);
        Key(ns, 0x15B, false); Tap(ns, 0x13 /* R */); Key(ns, 0x15B, true);
        Pump(1.5);
        foreach (char ch in "secondshelltest") Tap(ns, map[ch]);
        Pump(0.3);
        Tap(ns, 0x1C /* Enter */);
        var t1 = Stopwatch.StartNew();
        while (!readLog().Contains("START session=" + childId) && t1.Elapsed.TotalSeconds < 15) Pump(0.25);
        Log("TEST 1 result: " + (readLog().Contains("START session=" + childId) ? "SUCCESS, test program started in session " + childId : "FAILED, test program did not start"));
        Pump(6);

        // Test 2: COM activation of the per-user registered server; it is expected to start in the caller's session,
        // where the session guard must make it exit immediately.
        Log("TEST 2: COM activation Session:" + childId + " (per-user registration), on a background thread");
        string comInfo = null; object comObj = null;
        var comThread = new System.Threading.Thread(() => { comObj = ActivateInSession(childId, new Guid("C5A1E7D2-3B9F-4E6A-8D1C-2F7B9E4A6C0D"), out comInfo); });
        comThread.SetApartmentState(System.Threading.ApartmentState.MTA);
        var t2 = Stopwatch.StartNew();
        comThread.Start();
        while (comThread.IsAlive && t2.Elapsed.TotalSeconds < 150) Pump(0.25);
        Log("TEST 2 COM result after " + (int)t2.Elapsed.TotalSeconds + " s: " + (comThread.IsAlive ? "still waiting (abandoned)" : comInfo + " => " + (comObj != null ? "object created" : "failed")));
        foreach (var line in readLog().Split('\n'))
            if (line.Contains("GUARD") || line.Contains("START")) Log("  driver log: " + line.Trim());

        // Clean up: log the child session off explicitly
        Log("Logging off child session " + childId + ": " + WTSLogoffSession(IntPtr.Zero, childId, true));
        form.Show(); form.WindowState = FormWindowState.Normal;
        Pump(1);
        rdp.Disconnect();
        Pump(2);
        form.Close();
    }
}
'@
[CredTest]::Run($Seconds, $WinMode)



